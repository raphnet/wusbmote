/* wusbmote: Wiimote accessory to USB Adapter
 * Copyright (C) 2012-2014 Raphaël Assénat
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
#include <string.h>
#include "eeprom.h"
#include "usbdrv.h"

#include "wusbmote_requests.h"

struct eeprom_data_struct g_eeprom_data;

int usbDescriptorStringSerialNumber[]  = {
 	USB_STRING_DESCRIPTOR_HEADER(4),
	'2','0','0','0'
};

/* Called by the eeprom driver if the content
 * was invalid and it needs to write defaults
 * values.  */
void eeprom_app_write_defaults(void)
{
	const char *default_serial = "1001";

	memcpy(g_eeprom_data.cfg.serial, default_serial, 4);
	g_eeprom_data.cfg.mode = CFG_MODE_JOYSTICK;
	g_eeprom_data.cfg.mouse_divisor = 4;
	g_eeprom_data.cfg.mouse_deadzone = 5;
	g_eeprom_data.cfg.scroll_joystick_invert = 0;
	g_eeprom_data.cfg.scroll_nunchuck_threshold = 0x80;
	g_eeprom_data.cfg.scroll_nunchuck_step = 0; // 0 is off. 5 is not bad.
	g_eeprom_data.cfg.scroll_nunchuck_invert = 0;
	g_eeprom_data.cfg.scroll_nunchuck_c = 1;
	g_eeprom_data.cfg.scroll_nunchuck_c_threshold = 64;
}

/* Called by the eeprom driver once the content
 * is sucessfully loaded (or initialized).
 */
void eeprom_app_ready(void)
{
	int i;

	// Update the dynamic string descriptor.
	for (i=0; i<4; i++) {
		usbDescriptorStringSerialNumber[i+1] = g_eeprom_data.cfg.serial[i];
	}
}

static void config_set_serial(char serial[4])
{
	memcpy(g_eeprom_data.cfg.serial, serial, 4);
	eeprom_commit();
}

/* Return 0 for unknown commands.
 * On success, copy at least CMD to dst[0] and return 1.
 * To return n extra data, append starting at dst[1] and return 1+n
 */
unsigned char config_handleCommand(unsigned char cmd, const unsigned char rqdata[4], unsigned char dst[8])
{
	switch (cmd)
	{
		case RQ_WUSBMOTE_SETSERIAL:
			config_set_serial((char*)rqdata);
			return 1;
		case RQ_WUSBMOTE_SET_MODE:
			g_eeprom_data.cfg.mode = rqdata[0];
			break;
		case RQ_WUSBMOTE_SET_DIVISOR:
			g_eeprom_data.cfg.mouse_divisor = rqdata[0];
			break;
		case RQ_WUSBMOTE_SET_DEADZONE:
			g_eeprom_data.cfg.mouse_deadzone = rqdata[0];
			break;
		case RQ_WUSBMOTE_SET_SCROLL_JOYSTICK_INVERT:
			g_eeprom_data.cfg.scroll_joystick_invert = rqdata[0];
			break;
		case RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_INVERT:
			g_eeprom_data.cfg.scroll_nunchuck_invert = rqdata[0];
			break;
		case RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_THRESHOLD:
			g_eeprom_data.cfg.scroll_nunchuck_threshold = rqdata[0];
			break;
		case RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_STEP:
			g_eeprom_data.cfg.scroll_nunchuck_step = rqdata[0];
			break;
		case RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_C:
			g_eeprom_data.cfg.scroll_nunchuck_c = rqdata[0];
			break;
		case RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_C_THRESHOLD:
			g_eeprom_data.cfg.scroll_nunchuck_c_threshold = rqdata[0];
			break;

		default:
			return 0;
	}

	/* acknowledge command by replying with at least the command ID. */
	dst[0] = cmd;
	eeprom_commit();
	return 1;
}

