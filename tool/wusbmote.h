#ifndef _wusbmote_h__
#define _wusbmote_h__

#define OUR_VENDOR_ID 	0x289b
#define OUR_PRODUCT_ID 	0x0010

struct wusbmote_info {
	char str_prodname[256];
	char str_serial[256];
	int major, minor;
	int access; // True unless direct access to read serial/prodname failed due to permissions.
	int num_relays;
};

struct wusbmote_list_ctx;

typedef void* wusbmote_hdl_t; // Cast from usb_dev_handle
typedef void* wusbmote_device_t; // Cast from usb_device

int wusbmote_init(int verbose);
void wusbmote_shutdown(void);

struct wusbmote_list_ctx *wusbmote_allocListCtx(void);
void wusbmote_freeListCtx(struct wusbmote_list_ctx *ctx);
wusbmote_device_t wusbmote_listDevices(struct wusbmote_info *info, struct wusbmote_list_ctx *ctx);

wusbmote_hdl_t wusbmote_openDevice(wusbmote_device_t dusb_dev);
void wusbmote_closeDevice(wusbmote_hdl_t hdl);


int wusbmote_cmd(wusbmote_hdl_t hdl, const unsigned char cmd[5], unsigned char dst[8]);


#endif // _wusbmote_h__

