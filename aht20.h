#pragma once

#define AHT20_ADDR  0x38

/* Returns 0 on success, -1 on error.
 * temp_c   — temperature in degrees Celsius
 * hum_pct  — relative humidity in percent */
int aht20_sample(int i2c_fd, double *temp_c, double *hum_pct);
