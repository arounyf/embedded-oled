/*
 * I2C.h — Software bit-banged I2C using libgpiod
 */

#ifndef I2C_H_
#define I2C_H_

#include <stdint.h>
#include <stdbool.h>

/* No. of bytes per transaction */
#define I2C_ONE_BYTE                     1
#define I2C_TWO_BYTES                    2
#define I2C_THREE_BYTES                  3

/* I2C device path (kept for API compat, ignored in bit-bang mode) */
#define I2C_DEV0_PATH                    "/dev/i2c-0"
#define I2C_DEV1_PATH                    "/dev/i2c-1"
#define I2C_DEV2_PATH                    "/dev/i2c-2"

/* I2C device configuration structure */
typedef struct {
    char *i2c_dev_path;
    int fd_i2c;
    unsigned char i2c_slave_addr;
} I2C_DeviceT, *I2C_DevicePtr;

extern I2C_DeviceT I2C_DEV_2;

/* Exposed Generic I2C Functions */
extern int Open_device(char *i2c_dev_path, int *fd);
extern int Close_device(int fd);
extern int Set_slave_addr(int fd, unsigned char slave_addr);
extern int i2c_write(int fd, unsigned char data);
extern int i2c_read(int fd, unsigned char *read_data);
extern int i2c_read_register(int fd, unsigned char read_addr, unsigned char *read_data);
extern int i2c_read_registers(int fd, int num, unsigned char starting_addr, unsigned char *buff_Ptr);
extern void config_i2c_struct(char *i2c_dev_path, unsigned char slave_addr, I2C_DevicePtr i2c_dev);
extern int i2c_multiple_writes(int fd, int num, unsigned char *Ptr_buff);
extern int i2c_write_register(int fd, unsigned char reg_addr_or_cntrl, unsigned char val);
extern int init_i2c_dev(const char *i2c_path, unsigned char slave_address);

#endif /* I2C_H_ */
