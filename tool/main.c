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
#include <wchar.h>

#include "version.h"
#include "wusbmote.h"
#include "../wusbmote_requests.h"

static void printUsage(void)
{
	printf("./wusbmote_ctl [OPTION]... [COMMAND]....\n");
	printf("Control tool for WUSBmote adapter. Version %s\n", VERSION_STR);
	printf("\n");
	printf("Options:\n");
	printf("  -h, --help   Print help\n");
	printf("  -l, --list   List devices\n");
	printf("  -s serial    Operate on specified device (required unless -f is specified)\n");
	printf("  -f, --force  If no serial is specified, use first device detected.\n");
	printf("\n");
	printf("Configuratin commands:\n");
	printf("  --set_serial serial                Assign a new device serial number\n");
	printf("  --mouse_mode                       Put the device in mouse mode\n");
	printf("  --joystick_mode                    Put the device in joystick mode\n");
	printf("  --mouse_divisor val                Set the mouse rate divisor (Higher = slower). Typ: 4\n");
	printf("  --mouse_deadzone val               Set the deadzone for mouse mode. Typ: 5\n");
	printf("  --scroll_joystick_invert val       Invert joystick scrolling direction. (0 = normal, 1 = inverted)\n");
	printf("  --scroll_nunchuck_invert val       Invert scroll direction (0 = normal, 1 = inverted)\n");
	printf("  --scroll_nunchuck_threshold val    Set the nunchuck roll threshold for scrolling. Typ: 127\n");
	printf("  --scroll_nunchuck_step val         Set the scroll step size (Higher = more scrolling). Typ: 5\n");
	printf("  --scroll_nunchuck_c val            Enable/disable scrolling by move + C. (1 = enable, 0 = disable)\n");
	printf("  --scroll_nunchuck_c_threshold val  Stick deflection threshold for scrolling. (Typ: 64)\n");
	printf("\n");
	printf("Advanced:\n");
	printf("  --i2c_raw_mode                     Put the device in raw i2c mode (not joystick, not mouse)\n");
}

#define OPT_SET_SERIAL				257
#define OPT_MOUSE_MODE 				258
#define OPT_JOYSTICK_MODE			259
#define OPT_MOUSE_DIV				260
#define OPT_MOUSE_DZ				261
#define OPT_SCRL_JOY_INVERT			262
#define OPT_SCRL_NUNCHUCK_THRES		263
#define OPT_SCRL_NUNCHUCK_STEP		264
#define OPT_SCRL_NUNCHUCK_INVERT	265
#define OPT_SCRL_NUNCHUCK_C			266
#define OPT_SCRL_NUNCHUCK_C_THRES	267
#define OPT_I2C_RAW_MODE			268

struct option longopts[] = {
	{ "help", 0, NULL, 'h' },
	{ "list", 0, NULL, 'l' },
	{ "force", 0, NULL, 'f' },
	{ "set_serial", 1, NULL, OPT_SET_SERIAL },
	{ "mouse_mode", 0, NULL, OPT_MOUSE_MODE },
	{ "joystick_mode", 0, NULL, OPT_JOYSTICK_MODE },
	{ "mouse_divisor", 1, NULL, OPT_MOUSE_DIV },
	{ "mouse_deadzone", 1, NULL, OPT_MOUSE_DZ },
	{ "scroll_joystick_invert", 1, NULL, OPT_SCRL_JOY_INVERT },
	{ "scroll_nunchuck_invert", 1, NULL, OPT_SCRL_NUNCHUCK_INVERT },
	{ "scroll_nunchuck_threshold", 1, NULL, OPT_SCRL_NUNCHUCK_THRES },
	{ "scroll_nunchuck_step", 1, NULL, OPT_SCRL_NUNCHUCK_STEP },
	{ "scroll_nunchuck_c", 1, NULL, OPT_SCRL_NUNCHUCK_C },
	{ "scroll_nunchuck_c_threshold", 1, NULL, OPT_SCRL_NUNCHUCK_C_THRES },
	{ "i2c_raw_mode", 0, NULL, OPT_I2C_RAW_MODE },
	{ },
};

static int listDevices(void)
{
	int n_found = 0;
	struct wusbmote_list_ctx *listctx;
	struct wusbmote_info inf;

	listctx = wusbmote_allocListCtx();
	if (!listctx) {
		fprintf(stderr, "List context could not be allocated\n");
		return -1;
	}
	while (wusbmote_listDevices(&inf, listctx))
	{
		n_found++;
		printf("Found device '%ls', serial '%ls'\n", inf.str_prodname, inf.str_serial);
	}
	wusbmote_freeListCtx(listctx);
	printf("%d device(s) found\n", n_found);

	return n_found;
}

int main(int argc, char **argv)
{
	wusbmote_hdl_t hdl;
	struct wusbmote_list_ctx *listctx;
	int opt, retval = 0;
	struct wusbmote_info inf;
	struct wusbmote_info *selected_device = NULL;
	int verbose = 0, use_first = 0, serial_specified = 0;
	int cmd_list = 0;
#define TARGET_SERIAL_CHARS 128
	wchar_t target_serial[TARGET_SERIAL_CHARS];
	const char *short_optstr = "hls:vf";

	while((opt = getopt_long(argc, argv, short_optstr, longopts, NULL)) != -1) {
		switch(opt)
		{
			case 's':
				{
					mbstate_t ps;
					memset(&ps, 0, sizeof(ps));
					if (mbsrtowcs(target_serial, (const char **)&optarg, TARGET_SERIAL_CHARS, &ps) < 1) {
						fprintf(stderr, "Invalid serial number specified\n");
						return -1;
					}
					serial_specified = 1;
				}
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
			case '?':
				fprintf(stderr, "Unrecognized argument. Try -h\n");
				return -1;
		}
	}

	wusbmote_init(verbose);

	if (cmd_list) {
		printf("Simply listing the devices...\n");
		return listDevices();
	}

	if (!serial_specified && !use_first) {
		fprintf(stderr, "A serial number or -f must be used. Try -h for more information.\n");
		return 1;
	}

	listctx = wusbmote_allocListCtx();
	while ((selected_device = wusbmote_listDevices(&inf, listctx)))
	{
		if (serial_specified) {
			if (0 == wcscmp(inf.str_serial, target_serial)) {
				break;
			}
		}
		else {
			// use_first == 1
			printf("Will use device '%ls' serial '%ls'\n", inf.str_prodname, inf.str_serial);
			break;
		}
	}
	wusbmote_freeListCtx(listctx);

	if (!selected_device) {
		if (serial_specified) {
			fprintf(stderr, "Device not found\n");
		} else {
			fprintf(stderr, "No device found\n");
		}
		return 1;
	}

	hdl = wusbmote_openDevice(selected_device);
	if (!hdl) {
		printf("Error opening device. (Do you have permissions?)\n");
		return 1;
	}

	printf("Ready.\n");

	optind = 1;
	while((opt = getopt_long(argc, argv, short_optstr, longopts, NULL)) != -1)
	{
		unsigned char cmd[5] = {0,0,0,0,0};
		int n;

		switch (opt)
		{
			case OPT_SET_SERIAL:
				printf("Setting serial...");
				if (strlen(optarg) != 4) {
					fprintf(stderr, "Serial number must be 4 characters\n");
					return -1;
				}
				cmd[0] = RQ_WUSBMOTE_SETSERIAL;
				memcpy(cmd + 1, optarg, 4);
				break;

			case OPT_MOUSE_MODE:
			case OPT_JOYSTICK_MODE:
				printf("Setting mouse/joystick mode...");
				cmd[0] = RQ_WUSBMOTE_SET_MODE;
				cmd[1] = opt == OPT_MOUSE_MODE ? CFG_MODE_MOUSE : CFG_MODE_JOYSTICK;
				break;

			case OPT_I2C_RAW_MODE:
				printf("Enabling I2C raw mode\n");
				cmd[0] = RQ_WUSBMOTE_SET_MODE;
				cmd[1] = CFG_MODE_I2C_RAW;
				break;

			case OPT_MOUSE_DIV:
				printf("Setting mouse divisor...");
				cmd[0] = RQ_WUSBMOTE_SET_DIVISOR;
				cmd[1] = strtol(optarg, NULL, 0);
				break;

			case OPT_MOUSE_DZ:
				printf("Setting dead zone...");
				cmd[0] = RQ_WUSBMOTE_SET_DEADZONE;
				cmd[1] = strtol(optarg, NULL, 0);
				break;

			case OPT_SCRL_JOY_INVERT:
				printf("Setting joystick invert scroll...");
				cmd[0] = RQ_WUSBMOTE_SET_SCROLL_JOYSTICK_INVERT;
				cmd[1] = strtol(optarg, NULL, 0);
				break;

			case OPT_SCRL_NUNCHUCK_INVERT:
				printf("Setting nunchuck inverted scroll-by-rolling...");
				cmd[0] = RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_INVERT;
				cmd[1] = strtol(optarg, NULL, 0);
				break;

			case OPT_SCRL_NUNCHUCK_THRES:
				printf("Setting nunchuck scroll-by-rolling threshold...");
				cmd[0] = RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_THRESHOLD;
				cmd[1] = strtol(optarg, NULL, 0);
				break;

			case OPT_SCRL_NUNCHUCK_STEP:
				printf("Setting nunchuck scroll-by-rolling step...");
				cmd[0] = RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_STEP;
				cmd[1] = strtol(optarg, NULL, 0);
				break;

			case OPT_SCRL_NUNCHUCK_C:
				printf("Enabling/Disabling nunchuck scroll by C-button...");
				cmd[0] = RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_C;
				cmd[1] = strtol(optarg, NULL, 0);
				break;

			case OPT_SCRL_NUNCHUCK_C_THRES:
				printf("Setting nunchuck scroll by C-button threshold...");
				cmd[0] = RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_C_THRESHOLD;
				cmd[1] = strtol(optarg, NULL, 0);
				break;
		}

		if (cmd[0]) {
			n = wusbmote_send_cmd(hdl, cmd);
			printf("command result: %d\n", n);
		}
	}

	wusbmote_closeDevice(hdl);
	wusbmote_shutdown();

	return retval;
}
