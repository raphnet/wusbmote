#ifndef _wusbmote_priv_h__
#define _wusbmote_priv_h__

#include "hidapi.h"

struct wusbmote_list_ctx {
	struct hid_device_info *devs, *cur_dev;
};

#endif
