#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

int db_read(const char *path, int hours, tui_sample_t **out)
{
    sqlite3 *sql;
    if (sqlite3_open_v2(path, &sql, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        fprintf(stderr, "tui db open %s: %s\n", path, sqlite3_errmsg(sql));
        sqlite3_close(sql);
        return -1;
    }

    const char *query = hours > 0
        ? "SELECT timestamp, bmp_temp_c, bmp_press_hpa, aht_temp_c, aht_hum_pct"
          "  FROM samples"
          "  WHERE timestamp >= datetime('now', printf('-%d hours', ?))"
          "  ORDER BY timestamp ASC"
        : "SELECT timestamp, bmp_temp_c, bmp_press_hpa, aht_temp_c, aht_hum_pct"
          "  FROM samples ORDER BY timestamp ASC";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(sql, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "tui db prepare: %s\n", sqlite3_errmsg(sql));
        sqlite3_close(sql);
        return -1;
    }
    if (hours > 0)
        sqlite3_bind_int(stmt, 1, hours);

    size_t cap = 256, n = 0;
    tui_sample_t *rows = malloc(cap * sizeof(*rows));
    if (!rows) { sqlite3_finalize(stmt); sqlite3_close(sql); return -1; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (n == cap) {
            cap *= 2;
            rows = realloc(rows, cap * sizeof(*rows));
            if (!rows) { sqlite3_finalize(stmt); sqlite3_close(sql); return -1; }
        }
        tui_sample_t *s = &rows[n++];
        strncpy(s->timestamp, (const char *)sqlite3_column_text(stmt, 0),
                sizeof(s->timestamp) - 1);
        s->bmp_ok       = sqlite3_column_type(stmt, 1) == SQLITE_NULL ? -1 : 0;
        s->bmp_temp_c   = s->bmp_ok == 0 ? sqlite3_column_double(stmt, 1) : 0.0;
        s->bmp_press_hpa = s->bmp_ok == 0 ? sqlite3_column_double(stmt, 2) : 0.0;
        s->aht_ok       = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? -1 : 0;
        s->aht_temp_c   = s->aht_ok == 0 ? sqlite3_column_double(stmt, 3) : 0.0;
        s->aht_hum_pct  = s->aht_ok == 0 ? sqlite3_column_double(stmt, 4) : 0.0;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(sql);
    *out = rows;
    return (int)n;
}
