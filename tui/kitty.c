#include "kitty.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Base64 alphabet */
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_write(FILE *f, const uint8_t *src, size_t len)
{
    size_t i;
    for (i = 0; i + 2 < len; i += 3) {
        uint32_t v = ((uint32_t)src[i]<<16)|((uint32_t)src[i+1]<<8)|src[i+2];
        fputc(B64[(v>>18)&63], f);
        fputc(B64[(v>>12)&63], f);
        fputc(B64[(v>> 6)&63], f);
        fputc(B64[(v    )&63], f);
    }
    if (i < len) {
        uint32_t v = (uint32_t)src[i] << 16;
        if (i+1 < len) v |= (uint32_t)src[i+1] << 8;
        fputc(B64[(v>>18)&63], f);
        fputc(B64[(v>>12)&63], f);
        fputc(i+1 < len ? B64[(v>>6)&63] : '=', f);
        fputc('=', f);
    }
}

/* Emit one Kitty APC chunk: \x1b_G<keys>;<base64>\x1b\\ */
static void kitty_chunk(FILE *f, const char *keys,
                         const uint8_t *data, size_t len, int more)
{
    /* encode chunk into a temporary buffer so we can split at 4096 chars */
    fprintf(f, "\x1b_G%s,m=%d;", keys, more);
    if (data && len) b64_write(f, data, len);
    fprintf(f, "\x1b\\");
}

void kitty_display(const image_t *img, int id)
{
    const size_t CHUNK = 3072; /* bytes of raw data per chunk (base64 -> 4096) */
    const uint8_t *raw = (const uint8_t *)img->px;
    size_t total = (size_t)img->w * img->h * 4;
    size_t offset = 0;

    char first_keys[128];
    /* a=T transmit+display, f=32 RGBA, i=id, q=2 suppress response */
    snprintf(first_keys, sizeof(first_keys),
             "a=T,f=32,i=%d,q=2,s=%d,v=%d", id, img->w, img->h);

    int first = 1;
    while (offset < total) {
        size_t n = total - offset;
        if (n > CHUNK) n = CHUNK;
        int more = (offset + n < total) ? 1 : 0;
        if (first) {
            kitty_chunk(stdout, first_keys, raw + offset, n, more);
            first = 0;
        } else {
            char cont[32];
            snprintf(cont, sizeof(cont), "i=%d,q=2", id);
            kitty_chunk(stdout, cont, raw + offset, n, more);
        }
        offset += n;
    }
    fflush(stdout);
}

void term_move(int row, int col)
{
    printf("\x1b[%d;%dH", row, col);
}

void term_init(void)
{
    printf("\x1b[2J");    /* clear screen */
    printf("\x1b[?25l");  /* hide cursor */
    fflush(stdout);
}

void term_reset(void)
{
    printf("\x1b[?25h");  /* show cursor */
    printf("\x1b[0m");
    fflush(stdout);
}

void term_size(int *cols, int *rows, int *px_w, int *px_h)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        *px_w = ws.ws_xpixel;
        *px_h = ws.ws_ypixel;
    } else {
        *cols = 220; *rows = 50;
        *px_w = 0;   *px_h = 0;
    }
}
