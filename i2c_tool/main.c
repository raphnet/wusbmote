#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "hidapi.h"
#include "../i2c_raw.h"

#define OUR_VENDOR_ID	0x289B

static void dumphex(const char *label, const unsigned char *data, unsigned int len)
{
	int i;
	printf("%s: ", label);
	for (i=0; i<len; i++) {
		printf("%02x ", data[i]);
	}
	printf("\n");
}

int readTest(hid_device *hdl)
{
	unsigned char buffer[8];
	int n;

	memset(buffer, 0, sizeof(buffer));

	n = hid_get_feature_report(hdl, buffer, 8);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdl));
		return -1;
	}
	dumphex("receive", buffer, 8);
	printf("ret: %d\n", n);
	return 0;
}

int pingTest(hid_device *hdl)
{
	unsigned char buffer[8];
	int n;

	buffer[0] = 0x00; // report ID (set to zero when none)
	buffer[1] = I2C_RAW_ECHO_RQ;
	buffer[2] = 'A';
	buffer[3] = 'B';
	buffer[4] = 'C';
	buffer[5] = 'D';
	buffer[6] = 'E';
	buffer[7] = 'F';

	dumphex("send", buffer, 8);
	n = hid_send_feature_report(hdl, buffer, 8);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdl));
		return -1;
	}
	printf("ret: %d\n", n);

	memset(buffer, 0, sizeof(buffer));

	n = hid_get_feature_report(hdl, buffer, 8);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdl));
		return -1;
	}
	dumphex("receive", buffer, 8);
	printf("ret: %d\n", n);

	return 0;
}

int rawi2c_readReg(hid_device *hdl, unsigned char reg, unsigned char len, unsigned char *dst)
{
	unsigned char buffer[8];
	int n;

	if (len < 1 || len > 7) {
		return -1;
	}

	memset(buffer, 0, sizeof(buffer));

	buffer[0] = 0x00; // report ID (set to zero when none)
	buffer[1] = I2C_RAW_READ_REG1 + (len-1);
	buffer[2] = reg;

//	dumphex("send", buffer, 8);
	n = hid_send_feature_report(hdl, buffer, 8);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdl));
		return -1;
	}

	n = hid_get_feature_report(hdl, buffer, 8);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdl));
		return -1;
	}
//	dumphex("receive", buffer, 8);

	switch (buffer[1])
	{
		case I2C_RAW_TIMEOUT:
			fprintf(stderr, "tiemout\n");
			return -1;
		case I2C_RAW_ERROR:
			fprintf(stderr, "error\n");
			return -1;
		case I2C_RAW_READ_REG1:
		case I2C_RAW_READ_REG2:
		case I2C_RAW_READ_REG3:
		case I2C_RAW_READ_REG4:
		case I2C_RAW_READ_REG5:
		case I2C_RAW_READ_REG6:
		case I2C_RAW_READ_REG7:
			n = buffer[1] - I2C_RAW_READ_REG1 + 1;
			break;

		default:
			fprintf(stderr, "read reg return not understood\n");
			return -1;
	}

//	printf("I2C reg read returned %d bytes\n", n);

	memcpy(dst, buffer + 2, n);

	return n;
}

int rawi2c_setAddress(hid_device *hdl, unsigned char addr)
{
	unsigned char buffer[8];
	int n;

	memset(buffer, 0, sizeof(buffer));

	buffer[0] = 0x00; // report ID (set to zero when none)
	buffer[1] = I2C_RAW_SET_ADDRESS;
	buffer[2] = addr;

//	dumphex("send", buffer, 8);
	n = hid_send_feature_report(hdl, buffer, 8);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdl));
		return -1;
	}

	n = hid_get_feature_report(hdl, buffer, 8);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdl));
		return -1;
	}

//	dumphex("receive", buffer, 8);
	if (buffer[1] == I2C_RAW_OK) {
		printf("I2C address set to 0x%02x\n", buffer[2]);
		return 0;
	}

	fprintf(stderr, "Failed to set I2C address: code 0x%02x\n", buffer[2]);

	return -1;
}

int rawi2c_writeReg(hid_device *hdl, unsigned char reg, unsigned char len, const unsigned char *data)
{
	unsigned char buffer[8];
	int n;

	if (len < 1 || len > 7) {
		fprintf(stderr, "invalid write reg length\n");
		return -1;
	}

	memset(buffer, 0, sizeof(buffer));

	buffer[0] = 0x00; // report ID (set to zero when none)
	buffer[1] = I2C_RAW_WRITE_REG1 + len - 1;
	buffer[2] = reg;
	memcpy(buffer + 3, data, len);

//	dumphex("send", buffer, 8);
	n = hid_send_feature_report(hdl, buffer, 8);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdl));
		return -1;
	}

	n = hid_get_feature_report(hdl, buffer, 8);
	if (n < 0) {
		fprintf(stderr, "Could not send feature report (%ls)\n", hid_error(hdl));
		return -1;
	}

//	dumphex("receive", buffer, 8);
	if (buffer[1] == I2C_RAW_OK) {
		return 0;
	}

	fprintf(stderr, "Failed register write. code 0x%02x\n", buffer[2]);
	return -1;
}

void initWiimoteAccessory(hid_device *hdl, unsigned short *id)
{
	unsigned char tmp;
	int res;
	unsigned char idbytes[2];

	/* Standard wii accessory address (nunchuk, classic controller) */
	res = rawi2c_setAddress(hdl, 0x52);

	/* Do the magic writes known to disable scrambling */
	tmp = 0x55;
	res = rawi2c_writeReg(hdl, 0xF0, 1, &tmp);
	if (res)
		return;

	tmp = 0x00;
	res = rawi2c_writeReg(hdl, 0xFB, 1, &tmp);
	if (res)
		return;

	res = rawi2c_readReg(hdl, 0xFE, 2, idbytes);
	if (res != 2)
		return;

	dumphex("ID: ", idbytes, 2);

	if (id)
		*id = idbytes[0]<<8 | idbytes[1];
}

#define WM_EXP_STATUS		0x00
#define WM_EXP_CALIBRATION	0x20
#define WM_EXP_CALIBRATION2	0x30

void pollWiimoteAccessory(hid_device *hdl)
{
	unsigned char buf[6];
	unsigned char calibration[12];
	unsigned char calibration2[12];
	int res, i;
	unsigned char tmp;
	unsigned short id;

	initWiimoteAccessory(hdl, &id);

	rawi2c_readReg(hdl, WM_EXP_CALIBRATION, 6, calibration);
	rawi2c_readReg(hdl, WM_EXP_CALIBRATION + 6, 6, calibration + 6);
	rawi2c_readReg(hdl, WM_EXP_CALIBRATION + 12, 4, calibration + 12);
	dumphex("calibration", calibration, 16);

	rawi2c_readReg(hdl, WM_EXP_CALIBRATION2, 6, calibration2);
	rawi2c_readReg(hdl, WM_EXP_CALIBRATION2 + 6, 6, calibration2 + 6);
	rawi2c_readReg(hdl, WM_EXP_CALIBRATION2 + 12, 4, calibration2 + 12);
	dumphex("calibration2", calibration, 16);


	printf("Left stick:\n");
	printf("  Max X: 0x%02x\n", calibration[0]);
	printf("  Min X: 0x%02x\n", calibration[1]);
	printf("  Center X: 0x%02x\n", calibration[2]);
	printf("  Max Y: 0x%02x\n", calibration[3]);
	printf("  Min Y: 0x%02x\n", calibration[4]);
	printf("  Center Y: 0x%02x\n", calibration[5]);

	printf("Right stick:\n");
	printf("  Max X: 0x%02x\n", calibration[6]);
	printf("  Min X: 0x%02x\n", calibration[7]);
	printf("  Center X: 0x%02x\n", calibration[8]);
	printf("  Max Y: 0x%02x\n", calibration[9]);
	printf("  Min Y: 0x%02x\n", calibration[10]);
	printf("  Center Y: 0x%02x\n", calibration[11]);

	printf("slider ?: 0x%02x \n", calibration[12]>>3);
	printf("slider ?: 0x%02x \n", calibration[12]&0x1f);
	printf("slider ?: 0x%02x \n", (calibration[12]&0x07) | (calibration[13]>>6));
	printf("slider ?: 0x%02x \n", calibration[13]>>3);
	printf("slider ?: 0x%02x \n", calibration[14]>>3);

	while(1)
	{

		res = rawi2c_readReg(hdl, WM_EXP_STATUS, 6, buf);
		if (res!=6)
			break;

		dumphex("Raw data", buf, 6);

		switch(id)
		{
			case 0x0000: // Nunchuk
				break;

			case 0x0101: // Classic
				{
					unsigned char x,y,rx,ry,lt,rt;

					x = buf[0] << 2;
					y = (buf[1] << 2) ^ 0xFF;
					rx = ((buf[2]>>7) | ((buf[1]&0xC0)>>5) | ((buf[0]&0xC0)>>3));
					ry = buf[2] & 0x1f;

					lt = ((buf[3]>>5) | ((buf[2] & 0x60) >> 2) ) & 0x1f;
					rt = buf[3] & 0x1f;

					printf("X: 0x%02x, Y: 0x%02x, Rx: 0x%02x, Ry: 0x%02x, Lt: 0x%02x, Rt: 0x%02x\n", x,y,rx,ry,lt,rt);
				}
				break;
		}

		usleep(100000);
	}
}

void dumpWiimoteAccessory(hid_device *hdl)
{
	unsigned char regs[256];
	unsigned int reg;
	int res;

	initWiimoteAccessory(hdl, NULL);

	/* Read the register space */
	printf("Reading registers...\n");
	for (reg=0; reg<256; ) {
		int chunk = 6;

		if (reg + chunk > 256) {
			chunk = reg+chunk - 256;
			if (!chunk)
				break;
		}

		res = rawi2c_readReg(hdl, reg, chunk, regs + reg);
		if (res != chunk) {
			fprintf(stderr, "incomplete read (unexpected)\n");
			return;
		}
		reg+= chunk;
	}

	dumphex("Regs: ", regs, 256);
}

int main(int argc, char **argv)
{
	struct hid_device_info *inf, *cur_dev;
	hid_device *dev_handle = NULL;

	hid_init();

	inf = hid_enumerate(OUR_VENDOR_ID, 0x0000);
	if (!inf) {
		printf("Device not found\n");
		return -1;
	}

	for (cur_dev = inf; cur_dev; cur_dev = cur_dev->next)
	{
		printf("Considering 0x%04x:0x%04x Interface %d\n", cur_dev->vendor_id, cur_dev->product_id, cur_dev->interface_number);

		if (cur_dev->product_id == 0x0016 && cur_dev->interface_number == 0) {
			break;
		}
	}

	if (cur_dev) {
		printf("Device found\n");
		dev_handle = hid_open_path(cur_dev->path);

		if (!dev_handle) {
			fprintf(stderr, "Could not open device (check permissions)\n");
			goto cleanup;
		}
	} else {
		goto cleanup;
	}

//	pingTest(dev_handle);
//	dumpWiimoteAccessory(dev_handle);

	pollWiimoteAccessory(dev_handle);

cleanup:
	hid_free_enumeration(inf);
	if (dev_handle) {
		hid_close(dev_handle);
	}

	hid_exit();
	return 0;
}

