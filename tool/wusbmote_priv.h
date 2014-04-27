#ifndef _wusbmote_priv_h__
#define _wusbmote_priv_h__

#ifdef WINDOWS_VERSION
#include "lusb0_usb.h"
#else
#include <usb.h>
#endif

struct wusbmote_list_ctx {
	struct usb_bus *bus;
	struct usb_device *dev;
};

#endif
