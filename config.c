#include <string.h>
#include "eeprom.h"
#include "usbdrv.h"

struct eeprom_data_struct g_eeprom_data;

int usbDescriptorStringSerialNumber[]  = {
 	USB_STRING_DESCRIPTOR_HEADER(4),
	'2','0','0','0'
};

/* Called by the eeprom driver if the content
 * was invalid and it needs to write defaults
 * values.  */
void eeprom_app_write_defaults(void)
{
	const char *default_serial = "1001";

	memcpy(g_eeprom_data.cfg.serial, default_serial, 4);
}

/* Called by the eeprom driver once the content
 * is sucessfully loaded (or initialized).
 */
void eeprom_app_ready(void)
{
	int i;

	// Update the dynamic string descriptor.
	for (i=0; i<4; i++) {
		usbDescriptorStringSerialNumber[i+1] = g_eeprom_data.cfg.serial[i];
	}
}

void config_set_serial(char serial[4])
{
	memcpy(g_eeprom_data.cfg.serial, serial, 4);
	eeprom_commit();
}

