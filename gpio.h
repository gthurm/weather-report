#pragma once

/* Returns a line handle fd on success, -1 on error. */
int gpio_open(const char *chip_path, unsigned int line);
void gpio_close(int fd);

/* Returns 0 or 1 on success, -1 on error. */
int gpio_read(int fd);
