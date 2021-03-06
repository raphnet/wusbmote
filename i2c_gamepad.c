/* wusbmote: Wiimote accessory to USB Adapter
 * Copyright (C) 2012-2014 Rapha�l Ass�nat
 *
 * Based on:
 *   VBoy2USB: Virtual Boy to USB adapter
 *   Copyright (C) 2009 Rapha�l Ass�nat
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

#define REPORT_SIZE		8

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

#define I2C_STANDARD_ADDRESS	0x52
#define I2C_W2I_MPLUS_ADDRESS	0x53

// report matching the most recent bytes from the controller
static unsigned char last_read_controller_bytes[REPORT_SIZE];

// the most recently reported bytes
static unsigned char last_reported_controller_bytes[REPORT_SIZE];

#define DEBUGLOW()		PORTC &= 0xFE
#define DEBUGHIGH()		PORTC |= 0x01

#define W2I_REG_REPORT		0x00
#define W2I_REG_UNKNOWN_F0	0xF0
#define W2I_REG_UNKNOWN_FB	0xFB
#define W2I_REG_ID_L		0xFE
#define W2I_REG_ID_H		0xFF

#define STATE_INIT		0
#define STATE_READ_DATA	1

static char state = STATE_INIT;

#define FLAG_NO_ANALOG_SLIDERS		1
#define FLAG_NUNCHUK_Z_DISABLED	2
static unsigned char current_flags = FLAG_NO_ANALOG_SLIDERS;

// Based on reading 0xFE and 0xFF. This might be wrong...
#define ID_NUNCHUK	0x0000
#define ID_CLASSIC	0x0101
#define ID_MPLUS	0x0405

static unsigned short peripheral_id = ID_NUNCHUK;

static char w2i_reg_writeByte(unsigned char i2c_addr, unsigned char reg_addr, unsigned char value)
{
	unsigned char tmpbuf[2];
	char res;

	tmpbuf[0] = reg_addr;
	tmpbuf[1] = value;

	res = i2c_transaction(i2c_addr, 2, tmpbuf, 0, NULL, 0);
	_delay_us(400);

	return res;
}

static char w2i_reg_readBlock(unsigned char reg_addr, unsigned char *dst, int len)
{
	char res;

	res = i2c_transaction(I2C_STANDARD_ADDRESS, 1, &reg_addr, 0, NULL, 0);
	if (res)
		return res;

	_delay_us(400);

	res = i2c_transaction(I2C_STANDARD_ADDRESS, 0, NULL, len, dst, 0);
	if (res)
		return res;

	_delay_us(400);

	return 0;
}

static void setLastValues(unsigned char x, unsigned char y, unsigned short rx, unsigned short ry, unsigned short rz, unsigned char btns_l, unsigned char btns_h)
{
	last_read_controller_bytes[0] = x;
	last_read_controller_bytes[1] = y;
	last_read_controller_bytes[2] = rx;
	last_read_controller_bytes[3] = rx >> 8;
	last_read_controller_bytes[3] |= ry << 2;
	last_read_controller_bytes[4] = ry >> 6;
	last_read_controller_bytes[4] |= rz << 4;
	last_read_controller_bytes[5] = rz >> 4;
	last_read_controller_bytes[6] = btns_l;
	last_read_controller_bytes[7] = btns_h;
}

static void i2cGamepad_Update(void)
{
	unsigned char buf[16];
	char res;
	unsigned char x=0x80,y=0x80;
	unsigned char btns_l=0, btns_h=0;
	unsigned short rx=0x200,ry=0x200,rz=0x200;
	static char mplus_cal = 0;
	static char device_changed = 0;
	static int home_count = 0;

	switch (state)
	{

		case STATE_INIT:
			mplus_cal =0;

			// For now, we consider everything answering at this address to be the motion plus.
			// This switches the mplus to the standard address.
			res = w2i_reg_writeByte(I2C_W2I_MPLUS_ADDRESS, 0xFE, 0x04);
			// ignore failure

			//
			// Init sequence from:
			//
			// http://wiibrew.org/wiki/Wiimote/Extension_Controllers
			//
			res = w2i_reg_writeByte(I2C_STANDARD_ADDRESS, W2I_REG_UNKNOWN_F0, 0x55);
			if (res)
				return;

			res = w2i_reg_writeByte(I2C_STANDARD_ADDRESS, W2I_REG_UNKNOWN_FB, 0x00);
			if (res)
				return;

			res = w2i_reg_readBlock(W2I_REG_ID_L, buf, 2);
			if (res)
				return;

			peripheral_id = buf[1] | buf[0]<<8;

			state = STATE_READ_DATA;
			device_changed = 1;
			_delay_ms(1000);
			break;

			// fallthrough
		case STATE_READ_DATA:
			res = w2i_reg_readBlock(W2I_REG_REPORT, buf, 6);
			if (res) {
				state = STATE_INIT;
				return;
			}

			switch (peripheral_id)
			{
				default:
				case ID_NUNCHUK:
					// Source: http://wiibrew.org/wiki/Wiimote/Extension_Controllers/Nunchuck
					//
					//     7   6    5   4    3   2     1   0
					// 0   SX<7:0>
					// 1   SY<7:0>
					// 2   AX<9:2>
					// 3   AY<9:2>
					// 4   AZ<9:2>
					// 5   AZ<1:0>  AY<1:0>  AX<1:0>   BC  BZ
					//
					x = buf[0];
					y = buf[1] ^ 0xff;
					rx = ((buf[5] & 0x0C) >> 2)	| (buf[2] << 2);
					ry = ((buf[5] & 0x30) >> 4)	| (buf[3] << 2);
					rz = ((buf[5] & 0xC0) >> 6)	| (buf[4] << 2);

					if (!(buf[5]&0x01)) btns_l |= 0x01;
					if (!(buf[5]&0x02)) btns_l |= 0x02;

					if (device_changed) {
						device_changed = 0;

						// Holding both buttons at startup/connection
						// disables the Z axis (The gravity offset makes
						// it tricky to map buttons in many emulators)
						if ((btns_l & 0x03) == 0x03) { // HOME
							current_flags |= FLAG_NUNCHUK_Z_DISABLED;
						} else {
							current_flags &= ~FLAG_NUNCHUK_Z_DISABLED;
						}
					}

					if (current_flags & FLAG_NUNCHUK_Z_DISABLED) {
						rz = 0x200;
					}

					break;

				case ID_CLASSIC:
					// Source: http://wiibrew.org/wiki/Wiimote/Extension_Controllers/Classic_Controller
					//
					//     7        6     5    4    3     2     1     0
					// 0   RX<4:3>        LX<5:0>
					// 1   RX<2:1>        LY<5:0>
					// 2   RX<0>    LT<4:3>    RY<4:0>
					// 3   LT<2:0>             RT<4:0>
					// 4   BDR      BDD   BLT  B-   BH    B+    BRT   1
					// 5   BZL      BB    BY   BA   BX    BZR   BDL   BDU
					//
					x = buf[0] << 2;
					y = (buf[1] << 2) ^ 0xFF;
					rx = ((buf[2]>>7) | ((buf[1]&0xC0)>>5) | ((buf[0]&0xC0)>>3)) << 5;
					ry = ((buf[2]&0x1f) << 5) ^ 0x3FF;
					rz = ((buf[3]>>5) | ((buf[2] & 0x60) >> 2) ) << 5;
					rz ^= 0xffff;

					// The fist 12 USB button IDs follow the assignments of my Gamecube to USB adapter project.
					if (!(buf[4] & 0x04)) btns_l |= 0x01; // +/Start
					if (!(buf[5] & 0x20)) btns_l |= 0x02; // Y
					if (!(buf[5] & 0x08)) btns_l |= 0x04; // X
					if (!(buf[5] & 0x40)) btns_l |= 0x08; // B
					if (!(buf[5] & 0x10)) btns_l |= 0x10; // A
					if (!(buf[4] & 0x20)) btns_l |= 0x20; // L trig
					if (!(buf[4] & 0x02)) btns_l |= 0x40; // R trig
					if (!(buf[5] & 0x04)) btns_l |= 0x80; // Zr
					if (!(buf[5] & 0x01)) btns_h |= 0x01; // UP
					if (!(buf[4] & 0x40)) btns_h |= 0x02; // DOWN
					if (!(buf[4] & 0x80)) btns_h |= 0x04; // RIGHT
					if (!(buf[5] & 0x02)) btns_h |= 0x08; // LEFT

					if (!(buf[5] & 0x80)) btns_h |= 0x10; // Zl
					if (!(buf[4] & 0x10)) btns_h |= 0x20; // SELECT
					if (!(buf[4] & 0x08)) btns_h |= 0x40; // HOME

					if (device_changed) {
						device_changed = 0;

						// Holding the HOME button enables the troublesome L slider
						if (btns_h & 0x40) { // HOME
							current_flags &= ~FLAG_NO_ANALOG_SLIDERS;
						} else {
							current_flags |= FLAG_NO_ANALOG_SLIDERS;
						}
					}
#define HOME_HOLD_COUNT	180
					// Holding HOME for 3 seconds toggles the enabled state of the L slider
					if (btns_h & 0x40) {
						if (home_count < HOME_HOLD_COUNT) { // approx. 3 sec.
							home_count++;
						} else if (home_count==HOME_HOLD_COUNT) {
							current_flags ^= FLAG_NO_ANALOG_SLIDERS;
							home_count++;
						}
					} else {
						home_count=0;
					}

					if (current_flags & FLAG_NO_ANALOG_SLIDERS) {
						rz = 0x200;
					}

					break;

				case ID_MPLUS:
					{
						static short cal_rrx, cal_rry, cal_rrz;
						short rrx, rry, rrz;

#define SAT_10BIT_SIGNED(v)	do { if ((v) > 0x1FF)  (v) = 0x1ff; else if ((v) < -0x1FF) (v)= -0x1ff; } while(0)

						rrx = (buf[0] | ((buf[3]&0xFC)<<6)) << 2;
						rry = (buf[1] | ((buf[4]&0xFC)<<6)) << 2;
						rrz = (buf[2] | ((buf[5]&0xFC)<<6)) << 2;

						// convert to signed value
						rrx ^= 0x8000;
						rry ^= 0x8000;
						rrz ^= 0x8000;

						// zero values on origin
						if (mplus_cal < 10) {
							mplus_cal++;
							cal_rrx = rrx;
							cal_rry = rry;
							cal_rrz = rrz;
						}

						rrx -= cal_rrx;
						rry -= cal_rry;
						rrz -= cal_rrz;

						// We have a 14 bit value to fit in a 10 bit report.
						//
						// A shift of 6 keeps the high order bits (detects stronger motions)
						// A shift of 0 keeps the low order bits (detects very small motions)
						rrx >>= 5;
						rry >>= 5;
						rrz >>= 5;

						SAT_10BIT_SIGNED(rrx);
						SAT_10BIT_SIGNED(rry);
						SAT_10BIT_SIGNED(rrz);

						rx = rrx ^ 0x200;
						ry = rry ^ 0x200;
						rz = rrz ^ 0x200;

						if (!(buf[3] & 0x02)) btns_l |= 0x01;
						if (!(buf[3] & 0x01)) btns_l |= 0x02;
						if (!(buf[4] & 0x02)) btns_l |= 0x04;
						if ((buf[4] & 0x01)) btns_l |= 0x08; // extension connected
					}

					break;
			} // switch peripheral_id

			break; // STATE
	}

	setLastValues(x,y,rx,ry,rz,btns_l,btns_h);
}

static void i2cGamepad_Init(void)
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

	i2cGamepad_Update();

	DDRC |= 0x01;
	DEBUGLOW();
}

static char i2cGamepad_Changed(void)
{
	static int first = 1;
	if (first) { first = 0;  return 1; }

	return memcmp(last_read_controller_bytes,
					last_reported_controller_bytes, REPORT_SIZE);
}

static void i2cGamepad_BuildReport(unsigned char *reportBuffer)
{
	if (reportBuffer != NULL)
	{
		memcpy(reportBuffer, last_read_controller_bytes, REPORT_SIZE);
	}
	memcpy(last_reported_controller_bytes,
			last_read_controller_bytes,
			REPORT_SIZE);
}

#define USBDESCR_DEVICE         1

static const char usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
    USB_CFG_VENDOR_ID,  /* 2 bytes */
    USB_CFG_DEVICE_ID_JOYSTICK,  /* 2 bytes */
    USB_CFG_DEVICE_VERSION, /* 2 bytes */
	1,
	2,
	3,
//	USB_CFG_DESCR_PROPS_STRING_VENDOR != 0 ? 1 : 0,         /* manufacturer string index */
//	USB_CFG_DESCR_PROPS_STRING_PRODUCT != 0 ? 2 : 0,        /* product string index */
//	USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER != 0 ? 3 : 0,  /* serial number string index */
    1,          /* number of configurations */
};

/*
 * [0] X		// 8 bit
 * [1] Y		// 8 bit
 * [2] RX		// 10 bit
 * [3] RX,RY	// 10 bit
 * [4] RY,RZ	// 10 bit
 * [5] RZ		
 * [6] Btn 0-7
 * [7] Btn 8-15
 * 
 *
 */

static const char usbHidReportDescriptor_5axes_16btns[] PROGMEM = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)

	0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x02,                    //   REPORT_COUNT (2)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)

	0x09, 0x33,						// USAGE (Rx)
	0x09, 0x34,						// USAGE (Ry)
	0x09, 0x35,						// USAGE (Rz) // TODO : CHECK IF THIS REALLY IS 0x35

	0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x03,              //     LOGICAL_MAXIMUM (1024)
    0x75, 0x0A,                    //   REPORT_SIZE (8)
    0x95, 0x03,                    //   REPORT_COUNT (3)

    0x81, 0x02,                    //   INPUT (Data,Var,Abs)

	/* Padding.*/
	0x75, 0x01,                    //     REPORT_SIZE (1)
	0x95, 0x02,                    //     REPORT_COUNT (2)
	0x81, 0x03,                    //     INPUT (Constant,Var,Abs)

    0xc0,                          // END_COLLECTION

    0x05, 0x09,                    // USAGE_PAGE (Button)
    0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
    0x29, 16,                    //   USAGE_MAXIMUM (Button 16)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    // REPORT_SIZE (1)
    0x95, 16,                    // REPORT_COUNT (16)
    0x81, 0x02,                    // INPUT (Data,Var,Abs)

#if 0
	//// Vendor defined feature report
	0xA1, 0x02,				// COLLECTION (Logical)
		0x06, 0x00, 0xFF,	// USAGE_PAGE(Vendor defined page 1)
		0x09, 0x01,			// USAGE (Vendor defined usage)
		0x15, 0x00,			//   LOGICAL_MINIMUM (0)
	    0x26, 0xff, 0x00,	//     LOGICAL_MAXIMUM (255)
		0x75, 0x08,			// REPORT_SIZE (8)
		0x95, 0x05,			// REPORT_COUNT (5)
		0xB1, 0x00,			// FEATURE (Data,Ary,Abs)
	0xC0,					// END COLLECTION
#endif

	0xc0,                           // END_COLLECTION
};


Gamepad i2cGamepad_Gamepad = {
	report_size: 		REPORT_SIZE,
	reportDescriptorSize:	sizeof(usbHidReportDescriptor_5axes_16btns),
	deviceDescriptor:	usbDescrDevice,
	deviceDescriptorSize:	sizeof(usbDescrDevice),
	init: 			i2cGamepad_Init,
	update: 		i2cGamepad_Update,
	changed:		i2cGamepad_Changed,
	buildReport:		i2cGamepad_BuildReport
};

Gamepad *i2cGamepad_GetGamepad(void)
{
	i2cGamepad_Gamepad.reportDescriptor = (void*)usbHidReportDescriptor_5axes_16btns;

	return &i2cGamepad_Gamepad;
}

