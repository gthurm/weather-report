#include "bmp280.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define REG_CALIB  0x88
#define REG_CTRL   0xF4
#define REG_DATA   0xF7

typedef struct {
    uint16_t T1;
    int16_t  T2, T3;
    uint16_t P1;
    int16_t  P2, P3, P4, P5, P6, P7, P8, P9;
} calib_t;

struct bmp280 {
    int     fd;
    calib_t cal;
};

static int write_reg(int fd, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    if (write(fd, buf, 2) != 2) {
        fprintf(stderr, "bmp280 write reg 0x%02x: %s\n", reg, strerror(errno));
        return -1;
    }
    return 0;
}

static int read_reg(int fd, uint8_t reg, uint8_t *buf, int len)
{
    if (write(fd, &reg, 1) != 1) {
        fprintf(stderr, "bmp280 set reg 0x%02x: %s\n", reg, strerror(errno));
        return -1;
    }
    if (read(fd, buf, len) != len) {
        fprintf(stderr, "bmp280 read: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static int read_calib(int fd, calib_t *c)
{
    uint8_t b[24];
    if (read_reg(fd, REG_CALIB, b, 24) < 0) return -1;

    c->T1 = (uint16_t)(b[1]  << 8 | b[0]);
    c->T2 = (int16_t) (b[3]  << 8 | b[2]);
    c->T3 = (int16_t) (b[5]  << 8 | b[4]);
    c->P1 = (uint16_t)(b[7]  << 8 | b[6]);
    c->P2 = (int16_t) (b[9]  << 8 | b[8]);
    c->P3 = (int16_t) (b[11] << 8 | b[10]);
    c->P4 = (int16_t) (b[13] << 8 | b[12]);
    c->P5 = (int16_t) (b[15] << 8 | b[14]);
    c->P6 = (int16_t) (b[17] << 8 | b[16]);
    c->P7 = (int16_t) (b[19] << 8 | b[18]);
    c->P8 = (int16_t) (b[21] << 8 | b[20]);
    c->P9 = (int16_t) (b[23] << 8 | b[22]);
    return 0;
}

/* BMP280 datasheet section 4.2.3 compensation formulas (double precision) */

static double comp_temp(int32_t raw, const calib_t *c, int32_t *t_fine)
{
    double v1 = (raw / 16384.0  - c->T1 / 1024.0) * c->T2;
    double v2 =  raw / 131072.0 - c->T1 / 8192.0;
    v2 = v2 * v2 * c->T3;
    *t_fine = (int32_t)(v1 + v2);
    return (v1 + v2) / 5120.0;
}

static double comp_press(int32_t raw, const calib_t *c, int32_t t_fine)
{
    double v1 = t_fine / 2.0 - 64000.0;
    double v2 = v1 * v1 * c->P6 / 32768.0 + v1 * c->P5 * 2.0;
    v2 = v2 / 4.0 + (double)c->P4 * 65536.0;
    v1 = (c->P3 * v1 * v1 / 524288.0 + c->P2 * v1) / 524288.0;
    v1 = (1.0 + v1 / 32768.0) * c->P1;
    if (v1 == 0.0) return 0.0;
    double p = (1048576.0 - raw - v2 / 4096.0) * 6250.0 / v1;
    v1 = c->P9 * p * p / 2147483648.0;
    v2 = p * c->P8 / 32768.0;
    return p + (v1 + v2 + c->P7) / 16.0;
}

bmp280_t *bmp280_init(int i2c_fd)
{
    if (ioctl(i2c_fd, I2C_SLAVE, BMP280_ADDR) < 0) {
        fprintf(stderr, "bmp280 ioctl: %s\n", strerror(errno));
        return NULL;
    }
    bmp280_t *d = malloc(sizeof(*d));
    if (!d) return NULL;
    d->fd = i2c_fd;
    if (read_calib(i2c_fd, &d->cal) < 0) {
        free(d);
        return NULL;
    }
    return d;
}

void bmp280_close(bmp280_t *d)
{
    free(d);
}

int bmp280_sample(bmp280_t *d, double *temp_c, double *press_pa)
{
    if (ioctl(d->fd, I2C_SLAVE, BMP280_ADDR) < 0) {
        fprintf(stderr, "bmp280 ioctl: %s\n", strerror(errno));
        return -1;
    }

    /* osrs_t=1x, osrs_p=1x, forced mode — 0b00100101 */
    if (write_reg(d->fd, REG_CTRL, 0x25) < 0) return -1;
    usleep(10000); /* max measurement time at 1x oversampling: 6.4 ms */

    uint8_t b[6];
    if (read_reg(d->fd, REG_DATA, b, 6) < 0) return -1;

    int32_t raw_p = (int32_t)b[0] << 12 | (int32_t)b[1] << 4 | b[2] >> 4;
    int32_t raw_t = (int32_t)b[3] << 12 | (int32_t)b[4] << 4 | b[5] >> 4;

    int32_t t_fine;
    *temp_c   = comp_temp(raw_t, &d->cal, &t_fine);
    *press_pa = comp_press(raw_p, &d->cal, t_fine);
    return 0;
}
