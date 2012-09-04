/* VBoy2USB: Virtual Boy controller to USB
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

    0xc0                           // END_COLLECTION
};



