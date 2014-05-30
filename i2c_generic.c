/* wusbmote: Wiimote accessory to USB Adapter
 * Copyright (C) 2012-2014 Raphaël Assénat
 *
 * Based on:
 *   VBoy2USB: Virtual Boy to USB adapter
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
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "gamepad.h"
#include "i2c_gamepad.h"
#include "i2c.h"
#include "usbdrv.h"
#include "usbconfig.h"
#include "eeprom.h"
#include "i2c_raw.h"

/* The wiibrew documentation talks about writing to 0x(4)a400xx, reading from 0x(4)a500xx.
 *
 * In binary:
 * 0xa4..  1010 0100
 * 0xa5..  1010 0101
 *
 * It is clear that A4 and A5 is the first 8 bits of the I2C transaction. The least
 * significant bit is the I2C R/W bit.
 *
 * This translates to a 0x52 7bit address (0xa4>>1)
 *
 * Now the Wii motion plus documentation mentions 0x(4)A6000
 * (http://wiibrew.org/wiki/Wiimote/Extension_Controllers/Wii_Motion_Plus)
 *
 * So we have in binary:
 * 0xa6..  1010 0110
 *
 * The wii motion plus I2C 7 bit address is therfore 0x53.
 *
 * The document then explains the wii motion plus can be made to answer at 0xa4
 * by writing 0x04 to register 0xFE.
 */

static const char dummy_gamepad_reportdesc[] PROGMEM = {
	0x06, 0x00, 0xff,	// USAGE_PAGE (Generic Desktop)
	0x09, 0x01,			// USAGE (Vendor Usage 1)
	0xa1, 0x01,			// COLLECTION (Application)
	0x15, 0x00,			//   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,	//   LOGICAL_MAXIMUM (255)
	0x75, 0x08,			//   REPORT_SIZE (8)
	0x95, 0x07,			//   REPORT_COUNT (7)
	0x09, 0x01,			//   USAGE (Vendor defined)
	0xB1, 0x00,			//   FEATURE (Data,Ary,Abs)
	0xc0				// END_COLLECTION
};

static char g_address = 0xFF;

#define DEBUGLOW()		PORTC &= 0xFE
#define DEBUGHIGH()		PORTC |= 0x01

#define W2I_REG_REPORT		0x00
#define W2I_REG_UNKNOWN_F0	0xF0
#define W2I_REG_UNKNOWN_FB	0xFB
#define W2I_REG_ID_L		0xFE
#define W2I_REG_ID_H		0xFF

static char w2i_reg_writeBlock(unsigned char i2c_addr, unsigned char reg_addr, const unsigned char *data, int len)
{
	unsigned char tmpbuf[1 + len];
	char res;

	tmpbuf[0] = reg_addr;
	memcpy(tmpbuf + 1, data, len);

	res = i2c_transaction(i2c_addr, len + 1, tmpbuf, 0, NULL, 0);
	_delay_us(400);

	return res;
}

static char w2i_reg_readBlock(unsigned char i2c_addr, unsigned char reg_addr, unsigned char *dst, int len)
{
	char res;

	res = i2c_transaction(i2c_addr, 1, &reg_addr, 0, NULL, 0);
	if (res)
		return res;

	_delay_us(400);

	res = i2c_transaction(i2c_addr, 0, NULL, len, dst, 0);
	if (res)
		return res;

	_delay_us(400);

	return 0;
}

//
// resultBuf[0] : Result type
//   0x00: None
//   0x01: Success
//   0x02: Read data
//   0xFF: Error
//
static unsigned char resultBuf[7];

static char rawi2c_setFeatureReport(unsigned char *data, unsigned char len)
{
	int res;

	if (len != 7) {
		return -1;
	}

	memset(resultBuf, 0, sizeof(resultBuf));

	switch (data[0])
	{
		case I2C_RAW_SET_ADDRESS:
			g_address = data[1];
			resultBuf[0] = I2C_RAW_OK;
			resultBuf[1] = data[1];
			break;

		case I2C_RAW_WRITE_REG1: // register write
		case I2C_RAW_WRITE_REG2:
		case I2C_RAW_WRITE_REG3:
		case I2C_RAW_WRITE_REG4:
		case I2C_RAW_WRITE_REG5:
		case I2C_RAW_WRITE_REG6:
		case I2C_RAW_WRITE_REG7:
			// data[1] REG
			// data[2-6] DATA (length based on command)
			res = w2i_reg_writeBlock(g_address, data[1], data + 2, data[0] - I2C_RAW_WRITE_REG1 + 1);
			if (res) {
				resultBuf[0] = I2C_RAW_TIMEOUT;
			} else {
				resultBuf[0] = I2C_RAW_OK;
			}

			break;

		case I2C_RAW_READ_REG1:
		case I2C_RAW_READ_REG2:
		case I2C_RAW_READ_REG3:
		case I2C_RAW_READ_REG4:
		case I2C_RAW_READ_REG5:
		case I2C_RAW_READ_REG6:
		case I2C_RAW_READ_REG7:
			// data[1] REG
			res = w2i_reg_readBlock(g_address, data[1], resultBuf + 1, data[0] - I2C_RAW_READ_REG1 + 1);
			if (res) {
				resultBuf[0] = I2C_RAW_TIMEOUT;
			} else {
				resultBuf[0] = data[0];
			}
			break;

		case I2C_RAW_ECHO_RQ:
			memcpy(resultBuf, data, 7);
			resultBuf[0] = I2C_RAW_ECHO_REPLY;
			break;
	}

	return 1;
}

static unsigned char rawi2c_getFeatureReport(unsigned char *dst)
{
	//w2i_reg_readBlock(g_address, 0x00, dst, 1);
	memcpy(dst, resultBuf, 7);
	return 7;
}

static void rawi2c_update(void)
{
}

static void rawi2c_init(void)
{
	//
	// TWPS = 0
	// CPU FREQ = 12000000
	// TARGET SCL FREQ = 400000
	//
	//                   CPU FREQ
	//             --------------------
	// SCL freq =  16 + 2*TWBR * 4^TWPS
	//
	// TWBR = (((12000000 / 400000) - 16) / 1) / 2 = 7
	//
	//i2c_init(I2C_FLAG_EXTERNAL_PULLUP, 15);

	// In fact, 100khz is stable.
	i2c_init(I2C_FLAG_EXTERNAL_PULLUP, 52);

	memset(resultBuf, 0, sizeof(resultBuf));
}

static char rawi2c_changed(void)
{
	return 0;
}

static void rawi2c_buildreport(unsigned char *reportBuffer)
{
}

#define USBDESCR_DEVICE         1

static const char rawi2c_device_descriptor[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
    USB_CFG_VENDOR_ID,  /* 2 bytes */
    USB_CFG_DEVICE_ID_RAW_I2C,  /* 2 bytes */
    USB_CFG_DEVICE_VERSION, /* 2 bytes */
	1,	/* Manufacturer string */
	2,	/* Product string */
	3,	/* Serial number string */
    1,	/* number of configurations */
};


static Gamepad dummyGamepad = {
	report_size: 		0,
	reportDescriptor:	dummy_gamepad_reportdesc,
	reportDescriptorSize:	sizeof(dummy_gamepad_reportdesc),
	deviceDescriptor:	rawi2c_device_descriptor,
	deviceDescriptorSize:	sizeof(rawi2c_device_descriptor),
	init: 			rawi2c_init,
	update: 		rawi2c_update,
	changed:		rawi2c_changed,
	buildReport:	rawi2c_buildreport,
	setFeatureReport:	rawi2c_setFeatureReport,
	getFeatureReport:	rawi2c_getFeatureReport,
};

Gamepad *rawi2c_GetGamepad(void)
{
	return &dummyGamepad;
}
