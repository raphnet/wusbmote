#ifndef _config_h__
#define _config_h__

#define CFG_MODE_JOYSTICK	0x00
#define CFG_MODE_MOUSE		0x01

struct eeprom_cfg {
	uint8_t serial[4];
	uint8_t mode;
	uint8_t mouse_divisor;
	uint8_t mouse_deadzone;
};

void eeprom_app_write_defaults(void);
void eeprom_app_ready(void);

void config_set_serial(char serial[4]);

#endif
