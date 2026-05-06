/*
 * I2C.c - Software I2C using libgpiod + busy-wait delay
 * SDA=pin24(gpiochip0:24), SCL=pin25(gpiochip0:25)
 * usleep() has 3.3ms granularity (HZ=300), so we use busy-wait.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gpiod.h>
#include "I2C.h"

/* Each NOP loop ~0.4ns on 1.8GHz Cortex-A55, calibrate ~2500 loops = 1us */
#define BUSY_LOOPS_PER_US  2500

static void busy_udelay(int us)
{
    if (us <= 0) return;
    volatile unsigned int n = us * BUSY_LOOPS_PER_US;
    while (n--) __asm__ __volatile__("nop");
}

#define SDA_PIN   24
#define SCL_PIN   25
#define CHIP_NAME "gpiochip0"
#define I2C_HD    1    /* half-bit busy-wait in us (~333kHz I2C clock) */

static struct gpiod_chip *chip = NULL;
static struct gpiod_line *sda_line = NULL;
static struct gpiod_line *scl_line = NULL;
static unsigned char      slave_addr = 0;

I2C_DeviceT I2C_DEV_2;

static inline void sda_set(int v) { gpiod_line_set_value(sda_line, v); }
static inline void scl_set(int v) { gpiod_line_set_value(scl_line, v); }

static void i2c_start(void)
{
    sda_set(1); busy_udelay(I2C_HD);
    scl_set(1); busy_udelay(I2C_HD);
    sda_set(0); busy_udelay(I2C_HD);
    scl_set(0); busy_udelay(I2C_HD);
}

static void i2c_stop(void)
{
    sda_set(0); busy_udelay(I2C_HD);
    scl_set(1); busy_udelay(I2C_HD);
    sda_set(1); busy_udelay(I2C_HD);
}

static void i2c_write_byte_fast(unsigned char byte)
{
    for (int i = 7; i >= 0; i--) {
        sda_set((byte >> i) & 1);
        busy_udelay(I2C_HD);
        scl_set(1); busy_udelay(I2C_HD);
        scl_set(0); busy_udelay(I2C_HD);
    }
    /* ACK clock — output only, skip read */
    sda_set(1); busy_udelay(I2C_HD);
    scl_set(1); busy_udelay(I2C_HD);
    scl_set(0); busy_udelay(I2C_HD);
}

/* ================================================================== */

int Open_device(char *i2c_dev_path, int *fd)
{
    (void)i2c_dev_path;
    chip = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip) { perror("gpiod_open"); return -1; }
    sda_line = gpiod_chip_get_line(chip, SDA_PIN);
    scl_line = gpiod_chip_get_line(chip, SCL_PIN);
    if (!sda_line || !scl_line) { perror("gpiod_get"); return -1; }
    if (gpiod_line_request_output(sda_line, "oled-sda", 1) < 0 ||
        gpiod_line_request_output(scl_line, "oled-scl", 1) < 0) {
        perror("gpiod_out"); return -1;
    }
    *fd = 1;
    return 0;
}

int Close_device(int fd)
{
    (void)fd;
    if (sda_line) { sda_set(1); scl_set(1); gpiod_line_release(sda_line); sda_line = NULL; }
    if (scl_line) { gpiod_line_release(scl_line); scl_line = NULL; }
    if (chip)     { gpiod_chip_close(chip); chip = NULL; }
    return 0;
}

int Set_slave_addr(int fd, unsigned char a) { (void)fd; slave_addr = a; return 0; }

int init_i2c_dev(const char *p, unsigned char a)
{
    config_i2c_struct((char *)p, a, &I2C_DEV_2);
    if (Open_device(I2C_DEV_2.i2c_dev_path, &I2C_DEV_2.fd_i2c) == -1) return -1;
    if (Set_slave_addr(I2C_DEV_2.fd_i2c, I2C_DEV_2.i2c_slave_addr) == -1) return -1;
    return 0;
}

int i2c_write(int fd, unsigned char d)
{
    (void)fd;
    i2c_start(); i2c_write_byte_fast((slave_addr<<1)|0x00); i2c_write_byte_fast(d); i2c_stop();
    return 1;
}

int i2c_multiple_writes(int fd, int n, unsigned char *b)
{
    (void)fd;
    i2c_start(); i2c_write_byte_fast((slave_addr<<1)|0x00);
    for (int i=0; i<n; i++) i2c_write_byte_fast(b[i]);
    i2c_stop();
    return n;
}

int i2c_write_register(int fd, unsigned char r, unsigned char v)
{
    (void)fd;
    i2c_start(); i2c_write_byte_fast((slave_addr<<1)|0x00); i2c_write_byte_fast(r); i2c_write_byte_fast(v); i2c_stop();
    return I2C_TWO_BYTES;
}

int i2c_read(int fd, unsigned char *d) { (void)fd; *d=0; return 1; }
int i2c_read_register(int fd, unsigned char r, unsigned char *d) { (void)fd;(void)r; *d=0; return 1; }
int i2c_read_registers(int fd, int n, unsigned char s, unsigned char *b) {
    (void)fd;(void)s; if(b)memset(b,0,n); return n; }

void config_i2c_struct(char *p, unsigned char a, I2C_DevicePtr d)
{
    d->i2c_dev_path = p; d->fd_i2c = 0; d->i2c_slave_addr = a;
}
