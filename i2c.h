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
#ifndef _i2c_h__
#define _i2c_h__

#define I2C_FLAG_INTERNAL_PULLUP	0x1
#define I2C_FLAG_EXTERNAL_PULLUP	0x0
void i2c_init(int flags, unsigned char twbr);

#define I2C_FLAG_NO_NACK			0x02 // For reading less bytes 
int i2c_transaction(unsigned char addr, int wr_len, unsigned char *wr_data, 
								int rd_len, unsigned char *rd_data, unsigned char flags);

#endif // _i2c_h__


