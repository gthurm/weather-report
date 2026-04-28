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

#define I2C_BUS  "/dev/i2c-1"

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void update_display(ssd1315_t *oled, int bmp_ok, double bmp_t, double bmp_p,
                            int aht_ok, double aht_t, double aht_h)
{
    char line[32];
    ssd1315_clear(oled);
    ssd1315_text(oled, 0, 0, "-- Weather Station --");

    if (bmp_ok == 0) {
        snprintf(line, sizeof(line), "Temp(BMP): %5.1f C", bmp_t);
        ssd1315_text(oled, 2, 0, line);
        snprintf(line, sizeof(line), "Press: %7.1f hPa", bmp_p / 100.0);
        ssd1315_text(oled, 3, 0, line);
    } else {
        ssd1315_text(oled, 2, 0, "BMP280 error");
    }

    if (aht_ok == 0) {
        snprintf(line, sizeof(line), "Temp(AHT): %5.1f C", aht_t);
        ssd1315_text(oled, 5, 0, line);
        snprintf(line, sizeof(line), "Humidity:  %5.1f %%", aht_h);
        ssd1315_text(oled, 6, 0, line);
    } else {
        ssd1315_text(oled, 5, 0, "AHT20 error");
    }

    ssd1315_flush(oled);
}

static void print_json(int bmp_ok, double bmp_t, double bmp_p,
                       int aht_ok, double aht_t, double aht_h)
{
    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    printf("{\n");
    printf("  \"timestamp\": \"%s\",\n", ts);

    printf("  \"bmp280\": {\n");
    if (bmp_ok == 0)
        printf("    \"temperature_c\": %.2f,\n    \"pressure_hpa\": %.2f\n", bmp_t, bmp_p / 100.0);
    else
        printf("    \"error\": \"read failed\"\n");
    printf("  },\n");

    printf("  \"aht20\": {\n");
    if (aht_ok == 0)
        printf("    \"temperature_c\": %.2f,\n    \"humidity_pct\": %.2f\n", aht_t, aht_h);
    else
        printf("    \"error\": \"read failed\"\n");
    printf("  }\n");

    printf("}\n");
    fflush(stdout);
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

    bmp280_t  *bmp  = bmp280_init(fd);
    ssd1315_t *oled = ssd1315_init(fd);
    db_t      *db   = db_open(db_path);

    while (!g_stop) {
        double bmp_t = 0, bmp_p = 0, aht_t = 0, aht_h = 0;
        int bmp_ok = bmp ? bmp280_sample(bmp, &bmp_t, &bmp_p) : -1;
        int aht_ok = aht20_sample(fd, &aht_t, &aht_h);

        if (oled)
            update_display(oled, bmp_ok, bmp_t, bmp_p, aht_ok, aht_t, aht_h);

        print_json(bmp_ok, bmp_t, bmp_p, aht_ok, aht_t, aht_h);

        if (db)
            db_insert(db,
                      bmp_ok == 0 ? &bmp_t : NULL,
                      bmp_ok == 0 ? &(double){bmp_p / 100.0} : NULL,
                      aht_ok == 0 ? &aht_t : NULL,
                      aht_ok == 0 ? &aht_h : NULL);

        sleep(period);
    }

    if (oled) {
        ssd1315_clear(oled);
        ssd1315_flush(oled);
        ssd1315_close(oled);
    }
    if (bmp) bmp280_close(bmp);
    if (db)  db_close(db);

    close(fd);
    return 0;
}
