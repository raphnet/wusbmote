#ifndef _wusbmote_h__
#define _wusbmote_h__

#include <wchar.h>

#define OUR_VENDOR_ID 	0x289b
#define PRODNAME_MAXCHARS	256
#define SERIAL_MAXCHARS		256
#define PATH_MAXCHARS		256

struct wusbmote_info {
	wchar_t str_prodname[PRODNAME_MAXCHARS];
	wchar_t str_serial[SERIAL_MAXCHARS];
	char str_path[PATH_MAXCHARS];
	int major, minor;
	int access; // True unless direct access to read serial/prodname failed due to permissions.
};

struct wusbmote_list_ctx;

typedef void* wusbmote_hdl_t; // Cast from hid_device

int wusbmote_init(int verbose);
void wusbmote_shutdown(void);

struct wusbmote_list_ctx *wusbmote_allocListCtx(void);
void wusbmote_freeListCtx(struct wusbmote_list_ctx *ctx);
struct wusbmote_info *wusbmote_listDevices(struct wusbmote_info *info, struct wusbmote_list_ctx *ctx);

wusbmote_hdl_t wusbmote_openDevice(struct wusbmote_info *dev);
void wusbmote_closeDevice(wusbmote_hdl_t hdl);

int wusbmote_send_cmd(wusbmote_hdl_t hdl, const unsigned char cmd[5]);


#endif // _wusbmote_h__

