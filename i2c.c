/*   USBTenki - Interfacing sensors to USB 
 *   Copyright (C) 2007-2011  Raphaël Assénat <raph@raphnet.net>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <avr/io.h>
#include <util/twi.h>
#include <util/delay.h>

#include "i2c.h"

void i2c_init(int use_int_pullup, unsigned char twbr)
{
	if (use_int_pullup) {
		/* Use internal pullups */
		PORTC |= (1<<5)|(1<<4);
	} else {
		/* External pullups required */
		PORTC &= ~((1<<5)|(1<<4));
	}
	
	TWBR = twbr;
	TWSR &= ~((1<<TWPS1)|(1<<TWPS0));
}

#define DEBUGLOW()      PORTC &= 0xFE
#define DEBUGHIGH()     PORTC |= 0x01

static unsigned char i2cWaitInt(void)
{
	DEBUGHIGH();
	while (!(TWCR & (1<<TWINT))) 
			{ /* do nothing */ }
	DEBUGLOW();
	return TWSR & 0xF8;
}

int i2c_transaction(unsigned char addr, int wr_len, unsigned char *wr_data, 
								int rd_len, unsigned char *rd_data, unsigned char flags)
{
	int ret =0;
	unsigned char twsr;

	if (wr_len==0 && rd_len==0)
		return -1;

	if (wr_len != 0)
	{
		// Send a start condition
		TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);

		twsr = i2cWaitInt();
//	DEBUGLOW();
		if (twsr != TW_START)
			return 1; // Failed

		TWDR = (addr<<1) | 0;	/* Address + write(0) */
		TWCR = (1<<TWINT)|(1<<TWEN);
		

		twsr = i2cWaitInt();
		/* TWSR can be:
		 * TW_MT_SLA_ACK, TW_MT_SLA_NACK or TW_MR_ARB_LOST */
		if (twsr != TW_MT_SLA_ACK) {
			ret = 2;
			goto err;
		}
		
		while (wr_len--)
		{
			TWDR = *wr_data;
			TWCR = (1<<TWINT)|(1<<TWEN);

			twsr = i2cWaitInt();
			if (twsr != TW_MT_DATA_ACK) {
				ret = 3;
				goto err;
			}

			wr_data++;
		}
	} // if (wr_len != 0)

	if (rd_len != 0)
	{
		/* Do a (repeated) start condition */
		TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);

		twsr = i2cWaitInt();
		if ((twsr != TW_REP_START) && (twsr != TW_START) ) {
			ret = 4;
			goto err;
		}

		TWDR = (addr<<1) | 1;	/* Address + read(1) */
		TWCR = (1<<TWINT)|(1<<TWEN);
		
		twsr = i2cWaitInt();
		/* TWSR can be:
		 * TW_MR_SLA_ACK, TW_MR_SLA_NACK or TW_MR_ARB_LOST */
		if (twsr != TW_MR_SLA_ACK) {
			ret = 5;
			goto err;
		}

		while (rd_len--)
		{
			if (rd_len)
				TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
			else
				TWCR = (1<<TWINT)|(1<<TWEN);		
			
			twsr = i2cWaitInt();
			if ((twsr != TW_MR_DATA_ACK) && (twsr != TW_MR_DATA_NACK) ) 
			{
				break;
			}

			*rd_data = TWDR;
			rd_data++;
		}
	} // if (rd_len != 0)

	// Stop
	TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
	
	return 0;

arb_lost:
	return ret;

err:
	switch(twsr)
	{
		case TW_MT_ARB_LOST:
		//case TW_MR_ARB_LOST:
			TWCR = (1<<TWINT)|(1<<TWEN);
			break;

		case TW_NO_INFO:
			break;

		case TW_SR_STOP:
			TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
			break;

		case TW_MT_SLA_NACK:
		case TW_MR_SLA_NACK:

		case TW_MT_DATA_NACK:
		case TW_BUS_ERROR:
		
		default:	
			TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
	}

	return ret;
}

