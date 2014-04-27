#ifndef _config_h__
#define _config_h__

struct eeprom_cfg {
	uint8_t serial[4];
};

void eeprom_app_write_defaults(void);
void eeprom_app_ready(void);

void config_set_serial(char serial[4]);

#endif
