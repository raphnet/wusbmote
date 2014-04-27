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
#include <string.h>
#include <usb.h>
#include "wusbmote.h"
#include "wusbmote_priv.h"
#include "../wusbmote_requests.h"

static int dusbr_verbose = 0;

#define IS_VERBOSE()	(dusbr_verbose)

int wusbmote_init(int verbose)
{
	usb_init();
	dusbr_verbose = verbose;
	return 0;
}

void wusbmote_shutdown(void)
{
}
/*
static void wusbmote_initListCtx(struct wusbmote_list_ctx *ctx)
{
	memset(ctx, 0, sizeof(struct wusbmote_list_ctx));
}
*/
struct wusbmote_list_ctx *wusbmote_allocListCtx(void)
{
	return calloc(1, sizeof(struct wusbmote_list_ctx));
}

void wusbmote_freeListCtx(struct wusbmote_list_ctx *ctx)
{
	if (ctx) {
		free(ctx);
	}
}

/**
 * \brief List instances of our rgbleds device on the USB busses.
 * \param dst Destination buffer for device serial number/id. 
 * \param dstbuf_size Destination buffer size.
 */
wusbmote_device_t wusbmote_listDevices(struct wusbmote_info *info, struct wusbmote_list_ctx *ctx)
{
	struct usb_bus *start_bus;
	struct usb_device *start_dev;

	memset(info, 0, sizeof(struct wusbmote_info));

	if (ctx->dev && ctx->bus)
		goto jumpin;

	if (IS_VERBOSE()) {
		printf("Start listing\n");
	}

	usb_find_busses();
	usb_find_devices();

	start_bus = usb_get_busses();

	if (start_bus == NULL) {
		if (IS_VERBOSE()) {
			printf("No USB busses found!\n");
		}
		return NULL;
	}

	for (ctx->bus = start_bus; ctx->bus; ctx->bus = ctx->bus->next) {

		start_dev = ctx->bus->devices;
		for (ctx->dev = start_dev; ctx->dev; ctx->dev = ctx->dev->next) {
			if (IS_VERBOSE()) {
				printf("Considering USB 0x%04x:0x%04x\n", ctx->dev->descriptor.idVendor, ctx->dev->descriptor.idProduct);
			}
			if (ctx->dev->descriptor.idVendor == OUR_VENDOR_ID) {
				if (ctx->dev->descriptor.idProduct == OUR_PRODUCT_ID) {
					usb_dev_handle *hdl;

					if (IS_VERBOSE()) {
						printf("Recognized 0x%04x:0x%04x\n", ctx->dev->descriptor.idVendor, ctx->dev->descriptor.idProduct);
					}

					info->minor = ctx->dev->descriptor.bcdDevice & 0xff;
					info->major = (ctx->dev->descriptor.bcdDevice & 0xff00) >> 8;
					info->num_relays = 2;

					// Try to read serial number
					hdl = usb_open(ctx->dev);
					if (hdl) {
						info->access = 1;

						if (0 >= usb_get_string_simple(hdl, ctx->dev->descriptor.iProduct, info->str_prodname, 256)) {
							info->access = 0;
						}
						if (0 >= usb_get_string_simple(hdl, ctx->dev->descriptor.iSerialNumber, info->str_serial, 256)) {
							info->access = 0;
						}
						usb_close(hdl);
					}

					return ctx->dev;
				}
			}

jumpin:
			// prevent 'error: label at end of compound statement'
			continue;
		}
	}

	return NULL;
}

wusbmote_hdl_t wusbmote_openDevice(wusbmote_device_t dusb_dev)
{
	struct usb_dev_handle *hdl;
	int res;
	int detach_attempted = 0;

	hdl = usb_open(dusb_dev);
	if (!hdl)
		return NULL;

retry:
	res = usb_claim_interface(hdl, 0);
	if (res<0) {
		if (!detach_attempted) {
			printf("Attempting to detach kernel driver...\n");
			usb_detach_kernel_driver_np(hdl, 0);
			goto retry;
		}

		usb_close(hdl);
		return NULL;
	}

	return hdl;
}

void wusbmote_closeDevice(wusbmote_hdl_t hdl)
{
	usb_release_interface(hdl, 0);
	usb_close(hdl);
}

int wusbmote_cmd(wusbmote_hdl_t hdl, const unsigned char cmd[5], unsigned char dst[8])
{
	unsigned char buffer[8];
	int n;
	int datlen;

	n =	usb_control_msg(hdl,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, /* requesttype */
		cmd[0], 				/* bRequest*/
		cmd[2]<<8 | cmd[1], 	/* wValue */
		cmd[4]<<8 | cmd[3], 	/* wIndex */
		(char*)buffer, sizeof(buffer), 5000);

//	if (trace) {
//		printf("req: 0x%02x, val: 0x%02x, idx: 0x%02x <> %d: ",
//			cmd, id, 0, n);
//		if (n>0) {
//			for (i=0; i<n; i++) {
//				printf("%02x ", buffer[i]);
//			}
//		}
//		printf("\n");
//	}

	/* Validate size first */
	if (n>8) {
		fprintf(stderr, "Too much data received! (%d)\n", n);
		return -3;
	} else if (n<2) {
		fprintf(stderr, "Not enough data received! (%d)\n", n);
		return -4;
	}

	/* dont count command and xor */
	datlen = n - 2;

	/* Check if reply is for this command */
	if (buffer[0] != cmd[0]) {
		fprintf(stderr, "Invalid reply received (0x%02x)\n", buffer[0]);
		return -5;
	}

	if (datlen && dst) {
		memcpy(dst, buffer+1, datlen);
	}

	return datlen;
}
