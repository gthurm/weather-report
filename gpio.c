#include "gpio.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

int gpio_open(const char *chip_path, unsigned int line)
{
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) {
        fprintf(stderr, "gpio open %s: %s\n", chip_path, strerror(errno));
        return -1;
    }

    struct gpiohandle_request req = {
        .lineoffsets  = { line },
        .lines        = 1,
        .flags        = GPIOHANDLE_REQUEST_INPUT,
        .consumer_label = "weather-report",
    };

    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        fprintf(stderr, "gpio get line %u: %s\n", line, strerror(errno));
        close(chip_fd);
        return -1;
    }

    close(chip_fd);
    return req.fd;
}

void gpio_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

int gpio_read(int fd)
{
    struct gpiohandle_data data;
    if (ioctl(fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        fprintf(stderr, "gpio read: %s\n", strerror(errno));
        return -1;
    }
    return data.values[0];
}
