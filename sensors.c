#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "bmp280.h"
#include "aht20.h"
#include "ssd1315.h"
#include "db.h"
#include "firestore.h"
#include "gpio.h"

#define I2C_BUS    "/dev/i2c-1"
#define GPIO_CHIP  "/dev/gpiochip0"
#define GPIO_BTN   17

typedef struct {
    int    bmp_ok;
    double bmp_temp_c;
    double bmp_press_pa;

    int    aht_ok;
    double aht_temp_c;
    double aht_hum_pct;
} sample_t;

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void update_display(ssd1315_t *oled, const sample_t *s)
{
    char line[32];
    ssd1315_clear(oled);
    ssd1315_text(oled, 0, 0, ">>Weather Report<<");

    if (s->bmp_ok == 0 || s->aht_ok == 0) {
        double avg_t;
        if (s->bmp_ok == 0 && s->aht_ok == 0)
            avg_t = (s->bmp_temp_c + s->aht_temp_c) / 2.0;
        else
            avg_t = s->bmp_ok == 0 ? s->bmp_temp_c : s->aht_temp_c;
        snprintf(line, sizeof(line), "Temp:    %5.1f °C", avg_t);
        ssd1315_text(oled, 3, 0, line);
    } else {
        ssd1315_text(oled, 3, 0, "Temp: error");
    }

    if (s->bmp_ok == 0) {
        snprintf(line, sizeof(line), "Press: %7.1f hPa", s->bmp_press_pa / 100.0);
        ssd1315_text(oled, 4, 0, line);
    } else {
        ssd1315_text(oled, 4, 0, "Press: error");
    }

    if (s->aht_ok == 0) {
        snprintf(line, sizeof(line), "Humidity:  %5.1f %%", s->aht_hum_pct);
        ssd1315_text(oled, 5, 0, line);
    } else {
        ssd1315_text(oled, 5, 0, "Humidity: error");
    }

    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    ssd1315_text(oled, 7, 0, ts);

    ssd1315_flush(oled);
}

#ifdef DEBUG
static void print_json(const sample_t *s)
{
    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    printf("{\n");
    printf("  \"timestamp\": \"%s\",\n", ts);

    printf("  \"bmp280\": {\n");
    if (s->bmp_ok == 0)
        printf("    \"temperature_c\": %.2f,\n    \"pressure_hpa\": %.2f\n",
               s->bmp_temp_c, s->bmp_press_pa / 100.0);
    else
        printf("    \"error\": \"read failed\"\n");
    printf("  },\n");

    printf("  \"aht20\": {\n");
    if (s->aht_ok == 0)
        printf("    \"temperature_c\": %.2f,\n    \"humidity_pct\": %.2f\n",
               s->aht_temp_c, s->aht_hum_pct);
    else
        printf("    \"error\": \"read failed\"\n");
    printf("  }\n");

    printf("}\n");
    fflush(stdout);
}
#endif

static void store_sample(db_t *db, firestore_t *fs, const sample_t *s)
{
    double bmp_press_hpa = s->bmp_press_pa / 100.0;
    const double *p_bmp_t = s->bmp_ok == 0 ? &s->bmp_temp_c   : NULL;
    const double *p_bmp_p = s->bmp_ok == 0 ? &bmp_press_hpa   : NULL;
    const double *p_aht_t = s->aht_ok == 0 ? &s->aht_temp_c   : NULL;
    const double *p_aht_h = s->aht_ok == 0 ? &s->aht_hum_pct  : NULL;

    if (db) db_insert(db, p_bmp_t, p_bmp_p, p_aht_t, p_aht_h);
    if (fs) firestore_insert(fs, p_bmp_t, p_bmp_p, p_aht_t, p_aht_h);
}

int main(int argc, char *argv[])
{
    unsigned int period = 5;
    const char *db_path = "sensors.db";

    if (argc >= 2) {
        char *end;
        long v = strtol(argv[1], &end, 10);
        if (*end != '\0' || v <= 0) {
            fprintf(stderr, "usage: %s [period_seconds [db_path]]\n", argv[0]);
            return 1;
        }
        period = (unsigned int)v;
    }
    if (argc >= 3)
        db_path = argv[2];

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    int fd = open(I2C_BUS, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", I2C_BUS, strerror(errno));
        return 1;
    }

    bmp280_t    *bmp  = bmp280_init(fd);
    ssd1315_t   *oled = ssd1315_init(fd);
    db_t        *db   = db_open(db_path);
    firestore_t *fs   = firestore_open();
    int          btn  = gpio_open(GPIO_CHIP, GPIO_BTN);

    int display_on = 1;
    int btn_prev   = 0;
    unsigned int ticks = 0;
    const unsigned int ticks_per_sample = period * 10; /* 100 ms ticks */

    sample_t last = { .bmp_ok = -1, .aht_ok = -1 };

    while (!g_stop) {
        int btn_cur = btn >= 0 ? gpio_read(btn) : 0;
        if (btn_cur == 1 && btn_prev == 0) {
            display_on = !display_on;
            if (oled) {
                if (display_on)
                    update_display(oled, &last);
                else {
                    ssd1315_clear(oled);
                    ssd1315_flush(oled);
                }
            }
        }
        btn_prev = btn_cur;

        if (ticks % ticks_per_sample == 0) {
            last.bmp_ok = bmp ? bmp280_sample(bmp, &last.bmp_temp_c, &last.bmp_press_pa) : -1;
            last.aht_ok = aht20_sample(fd, &last.aht_temp_c, &last.aht_hum_pct);

            if (oled && display_on)
                update_display(oled, &last);
#ifdef DEBUG
            print_json(&last);
#endif
            store_sample(db, fs, &last);
        }

        usleep(100000); /* 100 ms */
        ticks++;
    }

    if (oled) {
        ssd1315_clear(oled);
        ssd1315_flush(oled);
        ssd1315_close(oled);
    }
    if (bmp) bmp280_close(bmp);
    if (db)  db_close(db);
    if (fs)  firestore_close(fs);
    gpio_close(btn);

    close(fd);
    return 0;
}
