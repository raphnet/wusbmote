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

#define REPORT_SIZE		4
/*
 * [0] Mouse buttons
 * [1] Mouse X
 * [2] Mouse Y
 * [3] Mouse Wheel
 */
static const char mouse_report_descriptor[] PROGMEM = {
	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x04,                    //     USAGE_MAXIMUM (Button 4)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x04,                    //     REPORT_COUNT (4)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x04,                    //     REPORT_SIZE (4)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)

    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)

    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
	0x35, 0x00,				// PHYSICAL MIN 0
	0x45, 0x00,				// PHYSICAL MAX 0
	0x09, 0x38,						// 	  USAGE (Wheel)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)

    0xc0,                          //   END_COLLECTION
    0xc0,                          // END_COLLECTION
};


// report matching the most recent bytes from the controller
static unsigned char last_read_controller_bytes[REPORT_SIZE];

// the most recently reported bytes
static unsigned char last_reported_controller_bytes[REPORT_SIZE];

static char g_active = 0;

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

// Based on reading 0xFE and 0xFF. This might be wrong...
#define ID_NUNCHUK	0x0000
#define ID_CLASSIC	0x0101

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

static void setLastValues(unsigned char x, unsigned char y, unsigned short rx, unsigned short ry, unsigned short rz, unsigned char btns, unsigned char orig_x, unsigned char orig_y)
{
	int X,Y,XO,YO;;
	int W;
	static int w_active;
	static unsigned char wvalue = 0;

	X = x - 0x80;
	Y = y - 0x80;
	XO = orig_x - 0x80;
	YO = orig_y - 0x80;

	X -= XO;
	Y -= YO;

	W = ry - 0x10;
w_active = 0;
#define WTHR 8
	if (W > WTHR) {
		if (!w_active) {
			W = 1;
			w_active = 1;
		} else {
			W = 0;
		}
	} else if (W < -WTHR) {
		if (!w_active) {
			W = -1;
			w_active = 1;
		} else {
			W = 0;
		}
	} else {
		W = 0;
		w_active = 0;
	}

	wvalue = W;

#define THR	g_eeprom_data.cfg.mouse_deadzone
	if (X > THR) {
		X -= THR;
	} else if (X < -THR) {
		X += THR;
	} else {
		X = 0;
	}
	if (Y > THR) {
		Y -= THR;
	} else if (Y < -THR) {
		Y += THR;
	} else {
		Y = 0;
	}


	if (X || Y || W || (btns & 0xf0)) {
		g_active = 1;
	} else {
		g_active = 0;
	}

	X /= g_eeprom_data.cfg.mouse_divisor;
	Y /= g_eeprom_data.cfg.mouse_divisor;

	if (btns & 0x10) Y = -1;
	if (btns & 0x20) Y = 1;
	if (btns & 0x40) X = 1;
	if (btns & 0x80) X = -1;

	if (X < -127)
		X = -127;
	if (X > 127)
		X = 127;
	if (Y < -127)
		Y = -127;
	if (Y > 127)
		Y = 127;

	last_read_controller_bytes[0] = btns;
	last_read_controller_bytes[1] = X & 0xff;
	last_read_controller_bytes[2] = Y & 0xff;
	last_read_controller_bytes[3] = wvalue;
}

static void i2cMouse_Update(void)
{
	unsigned char buf[16];
	char res;
	unsigned char x=0x80,y=0x80;
	static unsigned char orig_x=0x80, orig_y=0x80;
	unsigned char btns=0;
	unsigned short rx=0x200,ry=0x10,rz=0x200;
	static char device_changed = 0;

	switch (state)
	{

		case STATE_INIT:
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
					ry = 0; // ((buf[5] & 0x30) >> 4)	| (buf[3] << 2);
					rz = ((buf[5] & 0xC0) >> 6)	| (buf[4] << 2);

					if (!(buf[5]&0x01)) btns |= 0x01;
					if (!(buf[5]&0x02)) btns |= 0x02;

					if (device_changed) {
						device_changed = 0;

						orig_x = x;
						orig_y = y;
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
					ry = buf[2]&0x1f;
					rz = ((buf[3]>>5) | ((buf[2] & 0x60) >> 2) ) << 5;
					rz ^= 0xffff;

					if (!(buf[4] & 0x04)) btns |= 0x04; // +/Start
					if (!(buf[5] & 0x20)) btns |= 0x02; // Y
					if (!(buf[5] & 0x08)) btns |= 0x02; // X
					if (!(buf[5] & 0x40)) btns |= 0x01; // B
					if (!(buf[5] & 0x10)) btns |= 0x01; // A
/*
					if (!(buf[4] & 0x20)) btns_l |= 0x20; // L trig
					if (!(buf[4] & 0x02)) btns_l |= 0x40; // R trig
					if (!(buf[5] & 0x04)) btns_l |= 0x80; // Zr

						*/

					if (!(buf[5] & 0x01)) btns |= 0x10; // UP
					if (!(buf[4] & 0x40)) btns |= 0x20; // DOWN
					if (!(buf[4] & 0x80)) btns |= 0x40; // RIGHT
					if (!(buf[5] & 0x02)) btns |= 0x80; // LEFT
/*
					if (!(buf[5] & 0x80)) btns_h |= 0x10; // Zl
					if (!(buf[4] & 0x10)) btns_h |= 0x20; // SELECT
					if (!(buf[4] & 0x08)) btns_h |= 0x40; // HOME
*/
					if (device_changed) {
						device_changed = 0;

						orig_x = x;
						orig_y = y;
					}
					break;

			} // switch peripheral_id

			break; // STATE
	}

	setLastValues(x,y,rx,ry,rz,btns,orig_x,orig_y);
}

static void i2cMouse_Init(void)
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

	i2cMouse_Update();

	DDRC |= 0x01;
	DEBUGLOW();
}

static char i2cMouse_Changed(void)
{
	static char was_moving = 1;

	if (g_active)
		return 1;

	if (last_read_controller_bytes[0] != last_reported_controller_bytes[0])
		return 1;

	if (last_read_controller_bytes[3] != last_reported_controller_bytes[3])
		return 1;

	if (was_moving) {
		was_moving = 0;
		return 1;
	}

	return 0;
}

static void i2cMouse_BuildReport(unsigned char *reportBuffer)
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

static const char mouse_device_descriptor[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
    USB_CFG_VENDOR_ID,  /* 2 bytes */
    USB_CFG_DEVICE_ID_MOUSE,  /* 2 bytes */
    USB_CFG_DEVICE_VERSION, /* 2 bytes */
	1,	/* Manufacturer string */
	2,	/* Product string */
	3,	/* Serial number string */
    1,	/* number of configurations */
};


Gamepad i2cMouse_Gamepad = {
	report_size: 		REPORT_SIZE,
	reportDescriptor:	mouse_report_descriptor,
	reportDescriptorSize:	sizeof(mouse_report_descriptor),
	deviceDescriptor:	mouse_device_descriptor,
	deviceDescriptorSize:	sizeof(mouse_device_descriptor),
	init: 			i2cMouse_Init,
	update: 		i2cMouse_Update,
	changed:		i2cMouse_Changed,
	buildReport:		i2cMouse_BuildReport
};

Gamepad *i2cMouse_GetGamepad(void)
{
	return &i2cMouse_Gamepad;
}
