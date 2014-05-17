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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wusbmote.h"
#include "wusbmote_priv.h"
#include "../wusbmote_requests.h"

#include "hidapi.h"

static int dusbr_verbose = 0;


#define IS_VERBOSE()	(dusbr_verbose)

int wusbmote_init(int verbose)
{
	dusbr_verbose = verbose;
	hid_init();
	return 0;
}

void wusbmote_shutdown(void)
{
	hid_exit();
}

static char isProductIdHandled(unsigned short pid, int interface_number)
{
	switch (pid)
	{
		case 0x0010: // WUSBMote v1.2 (Joystick mode)
		case 0x0011: // WUSBMote v1.2 (Mouse mode)
		case 0x0012: // WUSBMote v1.2.1 (Joystick)
		case 0x0013: // WUSBMote v1.2.1 (Mouse)
			return 1;
		case 0x0014: // WUSBMote v1.3 (Joystick)
		case 0x0015: // WUSBMote v1.3 (Mouse)
			if (interface_number == 1)
				return 1;
			break;
	}
	return 0;
}

struct wusbmote_list_ctx *wusbmote_allocListCtx(void)
{
	struct wusbmote_list_ctx *ctx;
	ctx = calloc(1, sizeof(struct wusbmote_list_ctx));
	return ctx;
}

void wusbmote_freeListCtx(struct wusbmote_list_ctx *ctx)
{
	if (ctx) {
		if (ctx->devs) {
			hid_free_enumeration(ctx->devs);
		}
		free(ctx);
	}
}

/**
 * \brief List instances of our rgbleds device on the USB busses.
 * \param dst Destination buffer for device serial number/id.
 * \param dstbuf_size Destination buffer size.
 */
struct wusbmote_info *wusbmote_listDevices(struct wusbmote_info *info, struct wusbmote_list_ctx *ctx)
{
	memset(info, 0, sizeof(struct wusbmote_info));

	if (!ctx) {
		fprintf(stderr, "wusbmote_listDevices: Passed null context\n");
		return NULL;
	}

	if (ctx->devs)
		goto jumpin;

	if (IS_VERBOSE()) {
		printf("Start listing\n");
	}

	ctx->devs = hid_enumerate(OUR_VENDOR_ID, 0x0000);
	if (!ctx->devs) {
		printf("Hid enumerate returned NULL\n");
		return NULL;
	}

	for (ctx->cur_dev = ctx->devs; ctx->cur_dev; ctx->cur_dev = ctx->cur_dev->next)
	{
		if (IS_VERBOSE()) {
			printf("Considering 0x%04x:0x%04x\n", ctx->cur_dev->vendor_id, ctx->cur_dev->product_id);
		}
		if (isProductIdHandled(ctx->cur_dev->product_id, ctx->cur_dev->interface_number))
		{
				wcsncpy(info->str_prodname, ctx->cur_dev->product_string, PRODNAME_MAXCHARS-1);
				wcsncpy(info->str_serial, ctx->cur_dev->serial_number, SERIAL_MAXCHARS-1);
				strncpy(info->str_path, ctx->cur_dev->path, PATH_MAXCHARS-1);
				return info;
		}

		jumpin:
		// prevent 'error: label at end of compound statement'
		continue;
	}

	return NULL;
}

wusbmote_hdl_t wusbmote_openDevice(struct wusbmote_info *dev)
{
	hid_device *hdev;

	if (!dev)
		return NULL;

	if (IS_VERBOSE()) {
		printf("Opening device path: '%s'\n", dev->str_path);
	}

	hdev = hid_open_path(dev->str_path);
	if (!hdev)
		return NULL;

	return hdev;
}

void wusbmote_closeDevice(wusbmote_hdl_t hdl)
{
	hid_device *hdev = (hid_device*)hdl;
	if (hdev) {
		hid_close(hdev);
	}
}

int wusbmote_send_cmd(wusbmote_hdl_t hdl, const unsigned char cmd[5])
{
	hid_device *hdev = (hid_device*)hdl;
	unsigned char buffer[6];
	int n;

	buffer[0] = 0x00; // request ID set to 0 (device has only one)
	memcpy(buffer + 1, cmd, 5);

	n = hid_send_feature_report(hdev, buffer, 6);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdev));
		return -1;
	}

	return 0;
}
