/* wusbmote: Wiimote accessory to USB Adapter
 * Copyright (C) 2012-2014 Raphaël Assénat
 *
 * Based on:
 *   VBoy2USB: Virtual Boy controller to USB Adapter
 *   Copyright (C) 2009 Raphaël Assénat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The author may be contacted at raph@raphnet.net
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>

#include "usbdrv.h"
#include "gamepad.h"
#include "eeprom.h"
#include "config.h"
#include "wusbmote_requests.h"

#include "i2c_gamepad.h"
#include "i2c_mouse.h"

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega168A__) || \
	defined(__AVR_ATmega168P__) || defined(__AVR_ATmega328__) || \
	defined(__AVR_ATmega328P__) || defined(__AVR_ATmega88__) || \
	defined(__AVR_ATmega88A__) || defined(__AVR_ATmega88P__) || \
	defined(__AVR_ATmega88PA__)
#define AT168_COMPATIBLE
#endif

static uchar const *rt_usbHidReportDescriptor=NULL;
static uchar rt_usbHidReportDescriptorSize=0;
static uchar const *rt_usbDeviceDescriptor=NULL;
static uchar rt_usbDeviceDescriptorSize=0;

char usbDescriptorConfiguration[] = { 0 }; // dummy

uchar dataHidReport[22] = {
	0x06, 0x00, 0xff,	// USAGE_PAGE (Generic Desktop)
	0x09, 0x01,			// USAGE (Vendor Usage 1)
	0xa1, 0x01,			// COLLECTION (Application)
	0x15, 0x00,			//   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,	//   LOGICAL_MAXIMUM (255)
	0x75, 0x08,			//   REPORT_SIZE (8)
	0x95, 0x80,			//   REPORT_COUNT (128)
	0x09, 0x00,			//   USAGE (Undefined)
	0xb2, 0x02, 0x01,	//   FEATURE (Data,Var,Abs,Buf)
	0xc0				// END_COLLECTION
};

uchar my_usbDescriptorConfiguration[] = {    /* USB configuration descriptor */
    9,          /* sizeof(usbDescriptorConfiguration): length of descriptor in bytes */
    USBDESCR_CONFIG,    /* descriptor type */
    18 + 7 * USB_CFG_HAVE_INTRIN_ENDPOINT + 9 + 9 + 9 + 7, 0,
                /* total length of data returned (including inlined descriptors) */
    2,          /* number of interfaces in this configuration */
    1,          /* index of this configuration */
    0,          /* configuration name string index */
#if USB_CFG_IS_SELF_POWERED
    USBATTR_SELFPOWER,  /* attributes */
#else
    USBATTR_BUSPOWER | 0x80,   /* attributes */
#endif
    USB_CFG_MAX_BUS_POWER/2,            /* max USB current in 2mA units */

	/* interface descriptor follows inline: */
    9,          /* sizeof(usbDescrInterface): length of descriptor in bytes */
    USBDESCR_INTERFACE, /* descriptor type */
    0,          /* index of this interface */
    0,          /* alternate setting for this interface */
    USB_CFG_HAVE_INTRIN_ENDPOINT,   /* endpoints excl 0: number of endpoint descriptors to follow */
    USB_CFG_INTERFACE_CLASS,
    USB_CFG_INTERFACE_SUBCLASS,
    USB_CFG_INTERFACE_PROTOCOL,
    0,          /* string index for interface */

	9,          /* sizeof(usbDescrHID): length of descriptor in bytes */
    USBDESCR_HID,   /* descriptor type: HID */
    0x01, 0x01, /* BCD representation of HID version */
    0x00,       /* target country code */
    0x01,       /* number of HID Report (or other HID class) Descriptor infos to follow */
    0x22,       /* descriptor type: report */
    USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH, 0,  /* total length of report descriptor */

    /* endpoint descriptor for endpoint 1 */
    7,          /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x81,       /* IN endpoint number 1 */
    0x03,       /* attrib: Interrupt endpoint */
    8, 0,       /* maximum packet size */
    USB_CFG_INTR_POLL_INTERVAL, /* in ms */

	/**** Interface 1 *****/

	/* interface descriptor follows inline: */
    9,          /* sizeof(usbDescrInterface): length of descriptor in bytes */
    USBDESCR_INTERFACE, /* descriptor type */
    1,          /* index of this interface */
    0,          /* alternate setting for this interface */
    USB_CFG_HAVE_INTRIN_ENDPOINT,   /* endpoints excl 0: number of endpoint descriptors to follow */
    USB_CFG_INTERFACE_CLASS,
    USB_CFG_INTERFACE_SUBCLASS,
    USB_CFG_INTERFACE_PROTOCOL,
    0,          /* string index for interface */

	9,          /* sizeof(usbDescrHID): length of descriptor in bytes */
    USBDESCR_HID,   /* descriptor type: HID */
    0x01, 0x01, /* BCD representation of HID version */
    0x00,       /* target country code */
    0x01,       /* number of HID Report (or other HID class) Descriptor infos to follow */
    0x22,       /* descriptor type: report */
    sizeof(dataHidReport), 0,  /* total length of report descriptor */

	/* endpoint descriptor for endpoint 3 */
    7,          /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x83,       /* IN endpoint number 3 */
    0x03,       /* attrib: Interrupt endpoint */
    8, 0,       /* maximum packet size */
    USB_CFG_INTR_POLL_INTERVAL, /* in ms */
};

static Gamepad *curGamepad;

/* ----------------------- hardware I/O abstraction ------------------------ */

static void hardwareInit(void)
{
	// init port C as input with pullup on all but SCL/SDA pins. Those are external to 3.3v
	DDRC = 0x00;
	PORTC = 0x0f;

	// Port B does nothing in this project
	DDRB = 0x00;
	PORTB = 0xff;

	/*
	 * For port D, activate pull-ups on all lines except the D+, D- and bit 1.
	 *
	 * For historical reasons (a mistake on an old PCB), bit 1
	 * is now always connected with bit 0. So bit 1 configured
	 * as an input without pullup.
	 *
	 * Usb pin are init as output, low. (device reset). They are released
	 * later when usbReset() is called.
	 */
	PORTD = 0xf8;
	DDRD = 0x01 | 0x04;

	/* Configure timers */
#if defined(AT168_COMPATIBLE)
	TCCR0A = 0; // normal
	TCCR0B = 5;
	TCCR2A = (1<<WGM21);
	TCCR2B =(1<<CS22)|(1<<CS21)|(1<<CS20);
	OCR2A= 196;  // for 60 hz
#else
	/* configure timer 0 for a rate of 12M/(1024 * 256) = 45.78 Hz (~22ms) */
	TCCR0 = 5;      /* timer 0 prescaler: 1024 */

	TCCR2 = (1<<WGM21)|(1<<CS22)|(1<<CS21)|(1<<CS20);
	OCR2 = 196; // for 60 hz
#endif
}

#if defined(AT168_COMPATIBLE)
	#define mustPollControllers()	(TIFR2 & (1<<OCF2A))
	#define clrPollControllers()	do { TIFR2 = 1<<OCF2A; } while(0)
#else
	#define mustPollControllers()	(TIFR & (1<<OCF2))
	#define clrPollControllers()	do { TIFR = 1<<OCF2; } while(0)
#endif

static void usbReset(void)
{
	/* [...] a single ended zero or SE0 can be used to signify a device
	   reset if held for more than 10mS. A SE0 is generated by holding
       both th D- and D+ low (< 0.3V).
	*/
	PORTD &= ~(0x01 | 0x04); // Set D+ and D- to 0
	DDRD |= 0x01 | 0x04;
	_delay_ms(15);
	DDRD &= ~(0x01 | 0x04);
}
static uchar    reportBuffer[10];    /* buffer for HID reports */

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

uchar	usbFunctionDescriptor(struct usbRequest *rq)
{
	switch(rq->bmRequestType & USBRQ_TYPE_MASK)
	{
		case USBRQ_TYPE_STANDARD:
			if (rq->bRequest == USBRQ_GET_DESCRIPTOR)
			{
				// USB spec 9.4.3, high byte is descriptor type
				switch (rq->wValue.bytes[1])
				{
					case USBDESCR_DEVICE:
						usbMsgPtr = (void*)rt_usbDeviceDescriptor;
						return rt_usbDeviceDescriptorSize;
					case USBDESCR_HID_REPORT:
						// HID 1.1 : 7.1.1 Get_Descriptor request. wIndex is the interface number.
						if (rq->wIndex.word==1) {
							usbMsgPtr = (void*)dataHidReport;
							return sizeof(dataHidReport);
						} else {
							usbMsgPtr = (void*)rt_usbHidReportDescriptor;
							return rt_usbHidReportDescriptorSize;
						}
					case USBDESCR_CONFIG:
						usbMsgPtr = my_usbDescriptorConfiguration;
						return sizeof(my_usbDescriptorConfiguration);
				}
			}
			break;

		case USBRQ_TYPE_VENDOR:
			break;

	}

	return 0;
}

uchar	usbFunctionSetup(uchar data[8])
{
	uchar rqdata[4];
	static uchar replybuf[8];
	usbRequest_t    *rq = (void *)data;

	usbMsgPtr = reportBuffer;

	switch (rq->bmRequestType & USBRQ_TYPE_MASK)
	{
		case USBRQ_TYPE_CLASS:
			switch (rq->bRequest)
			{
				case USBRQ_HID_GET_REPORT:
					/* wValue: ReportType (highbyte), ReportID (lowbyte) */
					/* we only have one report type, so don't look at wValue */
					curGamepad->buildReport(reportBuffer);
					return curGamepad->report_size;

				case USBRQ_HID_SET_REPORT:
					/* wValue: Report Type (high), ReportID (low) */
					return USB_NO_MSG; // usbFunctionWrite will be called
			}
			break;

		case USBRQ_TYPE_VENDOR:
			rqdata[0] = rq->wValue.bytes[0];
			rqdata[1] = rq->wValue.bytes[1];
			rqdata[2] = rq->wIndex.bytes[0];
			rqdata[3] = rq->wIndex.bytes[1];
			usbMsgPtr = replybuf;

			return config_handleCommand(rq->bRequest, rqdata, replybuf);
	}
	return 0;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
	unsigned char dst[8];

	if (len != 5) {
		return 0xff;
	}

	if (!config_handleCommand(data[0], data+1, dst)) {
		return 0xff;
	}

	return 1;
}

/* ------------------------------------------------------------------------- */

void transferGamepadReport(void)
{
	int i;
	int todo = curGamepad->report_size;

	curGamepad->buildReport(reportBuffer);

	for (i=0; i<=curGamepad->report_size; i+=8)
	{
		while (!usbInterruptIsReady()) {
			usbPoll(); wdt_reset();
		}
		usbSetInterrupt(reportBuffer + i, todo > 8 ? 8 : todo);
		todo -= 8;
	}
}

int main(void)
{
	char must_report = 0, first_run = 1;

	hardwareInit();
	eeprom_init();

	switch (g_eeprom_data.cfg.mode)
	{
		default:
			g_eeprom_data.cfg.mode = CFG_MODE_JOYSTICK;
			eeprom_commit();
			// fallthrough to Joystick mode
		case CFG_MODE_JOYSTICK:
			curGamepad = i2cGamepad_GetGamepad();
			break;

		case CFG_MODE_MOUSE:
			curGamepad = i2cMouse_GetGamepad();
			break;
	}

	// configure report descriptor according to
	// the current gamepad
	rt_usbHidReportDescriptor = curGamepad->reportDescriptor;
	rt_usbHidReportDescriptorSize = curGamepad->reportDescriptorSize;
	rt_usbDeviceDescriptor = (void*)curGamepad->deviceDescriptor;
	rt_usbDeviceDescriptorSize = curGamepad->deviceDescriptorSize;

	// patch the config descriptor with the HID report descriptor size
	my_usbDescriptorConfiguration[25] = rt_usbHidReportDescriptorSize;
	my_usbDescriptorConfiguration[26] = rt_usbHidReportDescriptorSize >> 8;

	//wdt_enable(WDTO_2S);
	curGamepad->init();

	usbReset();
	usbInit();
	sei();

	for(;;){	/* main event loop */
		wdt_reset();

		// this must be called at each 50 ms or less
		usbPoll();

		if (first_run) {
			curGamepad->update();
			first_run = 0;
		}

		if (mustPollControllers())
		{
			if (!must_report)
			{
				curGamepad->update();
				if (curGamepad->changed()) {
					must_report = 1;
				}
			}

			clrPollControllers();
		}

		if(must_report && usbInterruptIsReady())
		{
			transferGamepadReport();
			must_report = 0;
		}
	}
	return 0;
}
