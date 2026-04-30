#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>

#include "db.h"
#include "chart.h"
#include "kitty.h"

#define DEFAULT_DB   "../sensors.db"
#define DEFAULT_HRS  24
#define REFRESH_SEC  60

/* RGBA colours matching the webapp palette */
#define COL_BMP_T  0x38bdf8   /* sky blue  */
#define COL_AHT_T  0xc084fc   /* purple    */
#define COL_AVG_T  0x4ade80   /* green     */
#define COL_PRESS  0xfb923c   /* orange    */
#define COL_HUM    0x4ade80   /* green     */

#define RGB32(hex) ( (uint32_t)( \
    (((hex)>>16)&0xff) | \
    ((((hex)>>8)&0xff)<<8) | \
    (((hex)&0xff)<<16) | \
    (0xffu<<24) ) )

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_resize = 0;
static void on_signal(int s) { if (s == SIGWINCH) g_resize = 1; else g_stop = 1; }

static double range_min(const double *v, int n, double fallback)
{
    double m = 1e300;
    for (int i = 0; i < n; i++) if (!isnan(v[i]) && v[i] < m) m = v[i];
    return m > 1e200 ? fallback : m;
}

static double range_max(const double *v, int n, double fallback)
{
    double m = -1e300;
    for (int i = 0; i < n; i++) if (!isnan(v[i]) && v[i] > m) m = v[i];
    return m < -1e200 ? fallback : m;
}

static void pad_range(double *lo, double *hi, double margin)
{
    double d = (*hi - *lo) * margin;
    if (d < 0.5) d = 0.5;
    *lo -= d; *hi += d;
}

static void draw_all(const tui_sample_t *samples, int n, int hours)
{
    int cols, rows, px_w, px_h;
    term_size(&cols, &rows, &px_w, &px_h);

    /* cell pixel size */
    int cell_px_w = px_w > 0 ? px_w / cols : 8;
    int cell_px_h = px_h > 0 ? px_h / rows : 16;

    /* gap between charts in pixels and its equivalent in rows */
    int gap_px   = cell_px_h * 2;
    int gap_rows = 2;

    /* each chart gets an equal share of the remaining vertical space */
    int reserved_px = 2 * cell_px_h     /* header rows */
                    + 1 * cell_px_h     /* footer row  */
                    + 2 * gap_px;       /* two gaps between three charts */
    int pw = (px_w > 0 ? px_w : cols * cell_px_w) * 4 / 5;
    int ph = px_h > 0 ? (px_h - reserved_px) / 3
                      : ((rows - 3) / 3 - gap_rows) * cell_px_h;
    if (pw < 80)  pw = 80;
    if (ph < 60)  ph = 60;

    int cell_h = ph / cell_px_h;  /* chart height in terminal rows */
    if (cell_h < 1) cell_h = 1;

    /* build series arrays */
    double *bmp_t  = malloc(n * sizeof(double));
    double *aht_t  = malloc(n * sizeof(double));
    double *avg_t  = malloc(n * sizeof(double));
    double *press  = malloc(n * sizeof(double));
    double *hum    = malloc(n * sizeof(double));

    for (int i = 0; i < n; i++) {
        bmp_t[i] = samples[i].bmp_ok == 0 ? samples[i].bmp_temp_c   : NAN;
        aht_t[i] = samples[i].aht_ok == 0 ? samples[i].aht_temp_c   : NAN;
        press[i] = samples[i].bmp_ok == 0 ? samples[i].bmp_press_hpa : NAN;
        hum[i]   = samples[i].aht_ok == 0 ? samples[i].aht_hum_pct   : NAN;

        int b = !isnan(bmp_t[i]), a = !isnan(aht_t[i]);
        avg_t[i] = b && a ? (bmp_t[i] + aht_t[i]) / 2.0
                 : b      ? bmp_t[i]
                 : a      ? aht_t[i]
                 :          NAN;
    }

    /* y ranges */
    double t_lo = range_min(avg_t, n, 0.0), t_hi = range_max(avg_t, n, 40.0);
    /* include both sensor ranges */
    double bl = range_min(bmp_t, n, t_lo), bh = range_max(bmp_t, n, t_hi);
    double al = range_min(aht_t, n, t_lo), ah = range_max(aht_t, n, t_hi);
    if (bl < t_lo) t_lo = bl;
    if (bh > t_hi) t_hi = bh;
    if (al < t_lo) t_lo = al;
    if (ah > t_hi) t_hi = ah;
    pad_range(&t_lo, &t_hi, 0.05);

    double p_lo = range_min(press, n, 980.0), p_hi = range_max(press, n, 1040.0);
    pad_range(&p_lo, &p_hi, 0.05);

    double h_lo = range_min(hum, n, 0.0), h_hi = range_max(hum, n, 100.0);
    pad_range(&h_lo, &h_hi, 0.05);

    /* header */
    term_move(1, 1);
    printf("\x1b[1;36m>>Weather Report<<\x1b[0m  "
           "\x1b[2mlast %d h  |  %d samples\x1b[0m", hours, n);

    if (n == 0) {
        term_move(3, 1);
        printf("No data found.");
        fflush(stdout);
        goto cleanup;
    }

    /* latest values */
    const tui_sample_t *last = &samples[n-1];
    double avg_last = NAN;
    if (last->bmp_ok == 0 && last->aht_ok == 0)
        avg_last = (last->bmp_temp_c + last->aht_temp_c) / 2.0;
    else if (last->bmp_ok == 0) avg_last = last->bmp_temp_c;
    else if (last->aht_ok == 0) avg_last = last->aht_temp_c;

    term_move(2, 1);
    printf("\x1b[2mTemp:\x1b[0m ");
    if (!isnan(avg_last)) printf("\x1b[1;36m%.1f °C\x1b[0m  ", avg_last); else printf("—  ");
    printf("\x1b[2mPress:\x1b[0m ");
    if (last->bmp_ok == 0) printf("\x1b[1;33m%.1f hPa\x1b[0m  ", last->bmp_press_hpa); else printf("—  ");
    printf("\x1b[2mHumidity:\x1b[0m ");
    if (last->aht_ok == 0) printf("\x1b[1;32m%.1f %%\x1b[0m  ", last->aht_hum_pct); else printf("—");
    printf("  \x1b[2m@ %s\x1b[0m", last->timestamp);

    /* render temperature chart with all three series */
    {
        image_t *img = image_new(pw, ph);

        /* draw avg first so BMP/AHT overdraw it */
        chart_draw(img, avg_t, n, t_lo, t_hi, RGB32(COL_AVG_T), "Temperature (degC)  --  BMP280 / AHT20 / avg");
        /* overdraw BMP and AHT on same image */
        for (int i = 0; i < n; i++) {
            if (!isnan(bmp_t[i])) {
                int x = 48 + (int)((double)i / (n-1) * (pw - 48 - 12));
                int y = (ph-16) - (int)((bmp_t[i] - t_lo) / (t_hi - t_lo) * (ph - 20 - 16));
                if (y >= 0 && y < ph && x >= 0 && x < pw)
                    img->px[y * pw + x] = RGB32(COL_BMP_T);
            }
            if (!isnan(aht_t[i])) {
                int x = 48 + (int)((double)i / (n-1) * (pw - 48 - 12));
                int y = (ph-16) - (int)((aht_t[i] - t_lo) / (t_hi - t_lo) * (ph - 20 - 16));
                if (y >= 0 && y < ph && x >= 0 && x < pw)
                    img->px[y * pw + x] = RGB32(COL_AHT_T);
            }
        }

        int row1 = 3;
        term_move(row1, 1);
        kitty_display(img, 1);
        image_free(img);
    }

    /* pressure chart */
    {
        image_t *img = image_new(pw, ph);
        chart_draw(img, press, n, p_lo, p_hi, RGB32(COL_PRESS), "Pressure (hPa)");
        int row2 = 3 + cell_h + gap_rows;
        term_move(row2, 1);
        kitty_display(img, 2);
        image_free(img);
    }

    /* humidity chart */
    {
        image_t *img = image_new(pw, ph);
        chart_draw(img, hum, n, h_lo, h_hi, RGB32(COL_HUM), "Humidity (%)");
        int row3 = 3 + (cell_h + gap_rows) * 2;
        term_move(row3, 1);
        kitty_display(img, 3);
        image_free(img);
    }

    fflush(stdout);

cleanup:
    free(bmp_t); free(aht_t); free(avg_t); free(press); free(hum);
}

int main(int argc, char *argv[])
{
    const char *db_path = DEFAULT_DB;
    int hours = DEFAULT_HRS;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 && i+1 < argc)
            hours = atoi(argv[++i]);
        else if (strcmp(argv[i], "-r") == 0 && i+1 < argc)
            hours = atoi(argv[++i]);  /* alias */
        else
            db_path = argv[i];
    }

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGWINCH, on_signal);

    /* put terminal in raw mode so we can quit on keypress */
    struct termios orig, raw;
    tcgetattr(STDIN_FILENO, &orig);
    raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    term_init();

    int elapsed = REFRESH_SEC; /* trigger immediate draw */
    while (!g_stop) {
        if (elapsed >= REFRESH_SEC || g_resize) {
            g_resize = 0;
            elapsed  = 0;

            tui_sample_t *samples = NULL;
            int n = db_read(db_path, hours, &samples);
            if (n < 0) n = 0;

            term_init();
            draw_all(samples, n, hours);
            free(samples);

            term_move(999, 1); /* push cursor below charts */
            printf("\x1b[2m q=quit  r=refresh  ±h=hours\x1b[0m");
            fflush(stdout);
        }

        /* non-blocking key read */
        char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) == 1) {
            if (ch == 'q' || ch == 'Q') break;
            if (ch == 'r' || ch == 'R') elapsed = REFRESH_SEC;
            if (ch == '+' || ch == '>') { hours = hours < 168 ? hours * 2 : 168; elapsed = REFRESH_SEC; }
            if (ch == '-' || ch == '<') { hours = hours > 1   ? hours / 2 : 1;   elapsed = REFRESH_SEC; }
        }

        usleep(200000); /* 200 ms poll */
        elapsed++;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    term_reset();
    return 0;
}
