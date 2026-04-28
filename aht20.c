#include "aht20.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define CMD_INIT     0xBE
#define CMD_MEASURE  0xAC

int aht20_sample(int fd, double *temp_c, double *hum_pct)
{
    if (ioctl(fd, I2C_SLAVE, AHT20_ADDR) < 0) {
        fprintf(stderr, "aht20 ioctl: %s\n", strerror(errno));
        return -1;
    }

    /* Check calibration bit (bit 3 of status byte); initialise if unset */
    uint8_t status;
    if (read(fd, &status, 1) != 1) {
        fprintf(stderr, "aht20 status read: %s\n", strerror(errno));
        return -1;
    }
    if (!(status & 0x08)) {
        uint8_t init[] = { CMD_INIT, 0x08, 0x00 };
        if (write(fd, init, sizeof(init)) != sizeof(init)) {
            fprintf(stderr, "aht20 init: %s\n", strerror(errno));
            return -1;
        }
        usleep(10000);
    }

    uint8_t meas[] = { CMD_MEASURE, 0x33, 0x00 };
    if (write(fd, meas, sizeof(meas)) != sizeof(meas)) {
        fprintf(stderr, "aht20 trigger: %s\n", strerror(errno));
        return -1;
    }
    usleep(80000); /* datasheet: 80 ms measurement time */

    uint8_t d[6];
    if (read(fd, d, 6) != 6) {
        fprintf(stderr, "aht20 data read: %s\n", strerror(errno));
        return -1;
    }
    if (d[0] & 0x80) {
        fprintf(stderr, "aht20 still busy after 80 ms\n");
        return -1;
    }

    uint32_t raw_h = (uint32_t)d[1] << 12 | (uint32_t)d[2] << 4 | d[3] >> 4;
    uint32_t raw_t = (uint32_t)(d[3] & 0x0F) << 16 | (uint32_t)d[4] << 8 | d[5];

    *hum_pct = raw_h / 1048576.0 * 100.0;
    *temp_c  = raw_t / 1048576.0 * 200.0 - 50.0;
    return 0;
}
