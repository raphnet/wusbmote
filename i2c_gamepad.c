/* VBoy2USB: Virtual Boy to USB adapter
 * Copyright (C) 2009 Raphaël Assénat
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
 * by wrigin 0x04 to register 0xFE. 
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

// Based on reading 0xFE and 0xFF. This might be wrong...
#define ID_NUNCHUCK	0x0000
#define ID_CLASSIC	0x0101
#define ID_MPLUS	0x0405

static unsigned short peripheral_id = ID_NUNCHUCK;

static void pulseres(int res)
{
	int i;
	
	DEBUGLOW();
	_delay_us(24);
		
	for (i=0; i<res; i++)  {
		DEBUGHIGH();
		_delay_us(5);
		DEBUGLOW();
		_delay_us(5);
	}
}

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
				case ID_NUNCHUCK:
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
					break;
				
				case ID_MPLUS:
					// not very useful now, we are missing bits.
					rx = (buf[0] | (buf[3]<<8));
					ry = (buf[1] | (buf[4]<<8));
					rz = (buf[2] | (buf[5]<<8));
					break;
			}
			break;
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
	// TODO: Add alt mappings test here

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

#include "report_desc_5axes_16btns.c"

Gamepad i2cGamepad_Gamepad = {
	report_size: 		REPORT_SIZE,
	reportDescriptorSize:	sizeof(usbHidReportDescriptor_5axes_16btns),
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

