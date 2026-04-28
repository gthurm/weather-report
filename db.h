#pragma once

typedef struct db db_t;

/* Returns NULL on error. Creates the table if it doesn't exist. */
db_t *db_open(const char *path);
void  db_close(db_t *db);

/* Pass NULL pointers for sensors that failed. Returns 0 on success, -1 on error. */
int db_insert(db_t *db,
              const double *bmp_temp_c, const double *bmp_press_hpa,
              const double *aht_temp_c, const double *aht_hum_pct);
