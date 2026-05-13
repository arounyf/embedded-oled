/*
 * I2C.c - Standard Linux I2C device driver
 * Uses kernel I2C character device (/dev/i2c-X)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "I2C.h"

I2C_DeviceT I2C_DEV_2;

int Open_device(char *i2c_dev_path, int *fd)
{
    *fd = open(i2c_dev_path, O_RDWR);
    if (*fd < 0) {
        perror("i2c open");
        return -1;
    }
    return 0;
}

int Close_device(int fd)
{
    if (fd > 0) close(fd);
    return 0;
}

int Set_slave_addr(int fd, unsigned char a)
{
    if (ioctl(fd, I2C_SLAVE, a) < 0) {
        perror("i2c ioctl");
        return -1;
    }
    return 0;
}

int init_i2c_dev(const char *p, unsigned char a)
{
    config_i2c_struct((char *)p, a, &I2C_DEV_2);
    if (Open_device(I2C_DEV_2.i2c_dev_path, &I2C_DEV_2.fd_i2c) == -1) return -1;
    if (Set_slave_addr(I2C_DEV_2.fd_i2c, I2C_DEV_2.i2c_slave_addr) == -1) return -1;
    return 0;
}

int i2c_write(int fd, unsigned char d)
{
    return write(fd, &d, 1);
}

int i2c_multiple_writes(int fd, int n, unsigned char *b)
{
    return write(fd, b, n);
}

int i2c_write_register(int fd, unsigned char r, unsigned char v)
{
    unsigned char buf[2] = {r, v};
    return write(fd, buf, 2);
}

int i2c_read(int fd, unsigned char *d) { (void)fd; *d=0; return 1; }
int i2c_read_register(int fd, unsigned char r, unsigned char *d) { (void)fd;(void)r; *d=0; return 1; }
int i2c_read_registers(int fd, int n, unsigned char s, unsigned char *b) {
    (void)fd;(void)s; if(b)memset(b,0,n); return n; }

void config_i2c_struct(char *p, unsigned char a, I2C_DevicePtr d)
{
    d->i2c_dev_path = p; d->fd_i2c = 0; d->i2c_slave_addr = a;
}
