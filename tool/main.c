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
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include "version.h"
#include "wusbmote.h"

static void printUsage(void)
{
	printf("./wusbmote_ctl [OPTION]... [COMMAND]....\n");
	printf("Control tool for WUSBmote adapter. Version %s\n", VERSION_STR);
	printf("\n");
	printf("Options:\n");
	printf("  -h           Print help\n");
	printf("  -l           List devices\n");
	printf("  -s serial    Operate on specified device (required unless -f is specified)\n");
	printf("  -f           If no serial is specified, use first device detected.\n");
	printf("\n");
}

static int listDevices(void)
{
	int n_found = 0;
	wusbmote_device_t cur_dev = NULL;
	struct wusbmote_list_ctx *listctx;
	struct wusbmote_info inf;
	int had_access_error = 0;

	listctx = wusbmote_allocListCtx();
	while ((cur_dev=wusbmote_listDevices(&inf, listctx)))
	{
		n_found++;
		printf("Found device '%s', serial '%s', firmware %d.%d", inf.str_prodname, inf.str_serial, inf.major, inf.minor);
		if (!inf.access) {
			printf(" (Warning: No access)");
			had_access_error = 1;
		}
		printf("\n");
	}
	wusbmote_freeListCtx(listctx);
	printf("%d device(s) found\n", n_found);

	if (had_access_error) {
		printf("\n** Warning! At least one device was not accessible. Please try as root or configure udev to give appropriate permissions.\n");
	}

	return n_found;
}

int main(int argc, char **argv)
{
	wusbmote_hdl_t hdl;
	wusbmote_device_t cur_dev = NULL, dev = NULL;
	struct wusbmote_list_ctx *listctx;
	int opt, retval = 0;
	struct wusbmote_info inf;
	int verbose = 0, use_first = 0;
	int cmd_list = 0;
	char *target_serial = NULL;

	while((opt = getopt(argc, argv, "hls:vf")) != -1) {
		switch(opt)
		{
			case 's':
				target_serial = optarg;
				break;
			case 'f':
				use_first = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'h':
				printUsage();
				return 0;
			case 'l':
				cmd_list = 1;
				break;
			default:
				fprintf(stderr, "Unrecognized argument. Try -h\n");
				return -1;
		}
	}


	wusbmote_init(verbose);

	if (cmd_list) {
		return listDevices();
	}

	if (!target_serial && !use_first) {
		fprintf(stderr, "A serial number or -f must be used. Try -h for more information.\n");
		return 1;
	}

	listctx = wusbmote_allocListCtx();
	while ((cur_dev=wusbmote_listDevices(&inf, listctx)))
	{
		if (target_serial) {
			if (0 == strcmp(inf.str_serial, target_serial)) {
				dev = cur_dev; // Last found will be used
				break;
			}
		}
		else {
			// use_first == 1
			dev = cur_dev; // Last found will be used
			printf("Will use device '%s' serial '%s' version %d.%d\n", inf.str_prodname, inf.str_serial, inf.major, inf.minor);
			break;
		}
	}
	wusbmote_freeListCtx(listctx);

	if (!dev) {
		if (target_serial) {
			fprintf(stderr, "Device not found\n");
		} else {
			fprintf(stderr, "No device found\n");
		}
		return 1;
	}

	hdl = wusbmote_openDevice(dev);
	if (!hdl) {
		printf("Error opening device. (Do you have permissions?)\n");
		return 1;
	}

	if (1)
	{
#define CMD_SET_SERIAL	1
		unsigned char cmd[5] = { CMD_SET_SERIAL, '9', '8', '7', '6' };
		unsigned char result[8];
		int n;

		n = wusbmote_cmd(hdl, cmd, result);
		printf("res: %d\n", n);
	}

	wusbmote_closeDevice(hdl);
	wusbmote_shutdown();

	return retval;
}
