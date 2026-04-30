#pragma once
#include <stddef.h>

typedef struct {
    char   timestamp[32];
    double bmp_temp_c;
    double bmp_press_hpa;
    double aht_temp_c;
    double aht_hum_pct;
    int    bmp_ok;   /* 0 if value present */
    int    aht_ok;
} tui_sample_t;

/* Returns number of rows read, or -1 on error.
 * Caller must free(*out). hours=0 means all rows. */
int db_read(const char *path, int hours, tui_sample_t **out);
