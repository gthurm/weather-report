#pragma once

#define BMP280_ADDR  0x77

typedef struct bmp280 bmp280_t;

/* Returns NULL on error. Reads factory calibration registers once. */
bmp280_t *bmp280_init(int i2c_fd);
void      bmp280_close(bmp280_t *d);

/* Returns 0 on success, -1 on error.
 * temp_c   — temperature in degrees Celsius
 * press_pa — pressure in Pascals */
int bmp280_sample(bmp280_t *d, double *temp_c, double *press_pa);
