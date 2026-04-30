#pragma once
#include <stdint.h>

#define SSD1315_ADDR    0x3C
#define SSD1315_WIDTH   128
#define SSD1315_PAGES   8   /* 8 pages × 8 px = 64 px tall */

/* Opaque handle — initialise with ssd1315_init(), free with ssd1315_close() */
typedef struct ssd1315 ssd1315_t;

ssd1315_t *ssd1315_init(int i2c_fd);
void       ssd1315_close(ssd1315_t *d);
void       ssd1315_clear(ssd1315_t *d);
void       ssd1315_flush(ssd1315_t *d);
void       ssd1315_display(ssd1315_t *d, int on); /* 1 = on (0xAF), 0 = off (0xAE) */

/* Draw a null-terminated string at page row (0-7) and pixel column (0-127).
 * Uses the built-in 5×7 font; characters are 6 px wide including spacing. */
void ssd1315_text(ssd1315_t *d, int page, int col, const char *str);
