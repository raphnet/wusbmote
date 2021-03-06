#ifndef _wusbmote_requests_h__
#define _wusbmote_requests_h__

#define CFG_MODE_JOYSTICK   0x00
#define CFG_MODE_MOUSE      0x01
#define CFG_MODE_I2C_RAW    0x02

#define RQ_WUSBMOTE_SETSERIAL		0x01
#define RQ_WUSBMOTE_SET_MODE		0x02
#define RQ_WUSBMOTE_SET_DIVISOR		0x03
#define RQ_WUSBMOTE_SET_DEADZONE	0x04
#define RQ_WUSBMOTE_SET_SCROLL_JOYSTICK_INVERT	0x05
#define RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_INVERT	0x06

#define RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_THRESHOLD	0x07
#define RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_STEP		0x08

#define RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_C			0x09
#define RQ_WUSBMOTE_SET_SCROLL_NUNCHUCK_C_THRESHOLD	0x0A

#endif
