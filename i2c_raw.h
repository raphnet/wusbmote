#ifndef _i2c_raw_h__
#define _i2c_raw_h__


#define I2C_RAW_ECHO_REPLY	0x00
#define I2C_RAW_ECHO_RQ		0x01

#define I2C_RAW_SET_ADDRESS	0x02

#define I2C_RAW_WRITE_REG1	0x10
#define I2C_RAW_WRITE_REG2	0x11
#define I2C_RAW_WRITE_REG3	0x12
#define I2C_RAW_WRITE_REG4	0x13
#define I2C_RAW_WRITE_REG5	0x14
#define I2C_RAW_WRITE_REG6	0x15
#define I2C_RAW_WRITE_REG7	0x16

#define I2C_RAW_READ_REG1	0x20
#define I2C_RAW_READ_REG2	0x21
#define I2C_RAW_READ_REG3	0x22
#define I2C_RAW_READ_REG4	0x23
#define I2C_RAW_READ_REG5	0x24
#define I2C_RAW_READ_REG6	0x25
#define I2C_RAW_READ_REG7	0x26

#define I2C_RAW_OK			0xF0
#define I2C_RAW_BAD_PARAM	0xFD
#define I2C_RAW_TIMEOUT		0xFE
#define I2C_RAW_ERROR		0xFF


#endif // _i2c_raw_h__

