#ifndef _config_h__
#define _config_h__

#define CFG_MODE_JOYSTICK	0x00
#define CFG_MODE_MOUSE		0x01

struct eeprom_cfg {
	uint8_t serial[4];
	uint8_t mode;
	uint8_t mouse_divisor;
	uint8_t mouse_deadzone;
	uint8_t scroll_joystick_invert;
	uint8_t scroll_nunchuck_invert;

	/* Scrolling by tilting the nunchuck */
	uint8_t scroll_nunchuck_threshold;
	uint8_t scroll_nunchuck_step;

	/* Scrolling by pressing C while moving */
	uint8_t scroll_nunchuck_c; // on/off
	uint8_t scroll_nunchuck_c_threshold;
};

void eeprom_app_write_defaults(void);
void eeprom_app_ready(void);

unsigned char config_handleCommand(unsigned char cmd, const unsigned char rqdata[4], unsigned char dst[8]);

#endif
