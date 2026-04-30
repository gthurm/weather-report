#pragma once
#include <stdint.h>
#include <stddef.h>

/* RGBA pixel buffer */
typedef struct {
    uint32_t *px;   /* width * height RGBA pixels, row-major */
    int       w, h;
} image_t;

image_t *image_new(int w, int h);
void     image_free(image_t *img);

/* Draw a line chart into img.
 * values/n: data points (NaN = missing).
 * color: 0xRRGGBB */
void chart_draw(image_t *img, const double *values, int n,
                double y_min, double y_max,
                uint32_t color, const char *title);
