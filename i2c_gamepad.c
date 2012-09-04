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

#define REPORT_SIZE		7

#define I2C_ADDRESS		0x52

// report matching the most recent bytes from the controller
static unsigned char last_read_controller_bytes[REPORT_SIZE];

// the most recently reported bytes
static unsigned char last_reported_controller_bytes[REPORT_SIZE];

#define DEBUGLOW()		PORTC &= 0xFE
#define DEBUGHIGH()		PORTC |= 0x01


#define STATE_INIT		0
#define STATE_READ_DATA	1

static char state = STATE_INIT;

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

static void i2cGamepad_Update(void)
{
	unsigned char buf[16];
	char res;
	int i;

	switch (state)
	{

		case STATE_INIT:
			buf[0] = 0xF0; // Accessory register address
			buf[1] = 0x55; // Value
			
			res = i2c_transaction(I2C_ADDRESS, 2, buf, 0, NULL, 0);
			if (res)
				return;
		
			_delay_us(100);
			_delay_ms(10);
			
			buf[0] = 0xFB; // Accessory register address
			buf[1] = 0x00; // Value
			res = i2c_transaction(I2C_ADDRESS, 2, buf, 0, NULL, 0);
			if (res)
				return;

			state = STATE_READ_DATA;
			_delay_ms(1000);
			break;

			// fallthrough
		case STATE_READ_DATA:
			buf[0] = 0x00;
			res = i2c_transaction(I2C_ADDRESS, 1, buf, 0, NULL, 0);
			if (res) {
				state = STATE_INIT;
				return;
			}
			_delay_us(400);
			
			res = i2c_transaction(I2C_ADDRESS, 0, NULL, 6, buf, 0);
			_delay_us(400);

			last_read_controller_bytes[0] = buf[0];
			last_read_controller_bytes[1] = buf[1] ^ 0xff;


			last_read_controller_bytes[2] = (buf[5] & 0x0C) >> 2;
			last_read_controller_bytes[2] |= buf[2] << 2;
			last_read_controller_bytes[3] = buf[2] >> 6;

			last_read_controller_bytes[3] |= (buf[5] & 0x30) << 2;
			last_read_controller_bytes[3] |= (buf[3] << 4);
			last_read_controller_bytes[4] = buf[3] >> 4;

			last_read_controller_bytes[4] |= (buf[5] & 0xC0) << 4;
			last_read_controller_bytes[4] |= buf[4] << 6;
			last_read_controller_bytes[5] = buf[4] >> 2;

			
			last_read_controller_bytes[6] = (buf[5] & 0x03) ^ 0x03;

			break;
	}

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

