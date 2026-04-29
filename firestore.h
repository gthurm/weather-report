#pragma once

typedef struct firestore firestore_t;

/* Reads FIRESTORE_PROJECT_ID, FIRESTORE_API_KEY, FIRESTORE_COLLECTION (optional,
 * default "samples") from the environment. Returns NULL on error. */
firestore_t *firestore_open(void);
void         firestore_close(firestore_t *fs);

/* Pass NULL for any sensor that failed. Returns 0 on success, -1 on error. */
int firestore_insert(firestore_t *fs,
                     const double *bmp_temp_c, const double *bmp_press_hpa,
                     const double *aht_temp_c, const double *aht_hum_pct);
