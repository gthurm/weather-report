#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

#define SCHEMA \
    "CREATE TABLE IF NOT EXISTS samples (" \
    "  timestamp       TEXT NOT NULL," \
    "  bmp_temp_c      REAL," \
    "  bmp_press_hpa   REAL," \
    "  aht_temp_c      REAL," \
    "  aht_hum_pct     REAL" \
    ");"

#define INSERT_SQL \
    "INSERT INTO samples" \
    " (timestamp, bmp_temp_c, bmp_press_hpa, aht_temp_c, aht_hum_pct)" \
    " VALUES (strftime('%Y-%m-%dT%H:%M:%SZ','now'), ?, ?, ?, ?);"

struct db {
    sqlite3      *sql;
    sqlite3_stmt *stmt;
};

db_t *db_open(const char *path)
{
    db_t *db = malloc(sizeof(*db));
    if (!db) return NULL;

    if (sqlite3_open(path, &db->sql) != SQLITE_OK) {
        fprintf(stderr, "db open %s: %s\n", path, sqlite3_errmsg(db->sql));
        sqlite3_close(db->sql);
        free(db);
        return NULL;
    }

    if (sqlite3_exec(db->sql, SCHEMA, NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "db schema: %s\n", sqlite3_errmsg(db->sql));
        sqlite3_close(db->sql);
        free(db);
        return NULL;
    }

    if (sqlite3_prepare_v2(db->sql, INSERT_SQL, -1, &db->stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "db prepare: %s\n", sqlite3_errmsg(db->sql));
        sqlite3_close(db->sql);
        free(db);
        return NULL;
    }

    return db;
}

void db_close(db_t *db)
{
    sqlite3_finalize(db->stmt);
    sqlite3_close(db->sql);
    free(db);
}

static void bind_nullable(sqlite3_stmt *s, int col, const double *v)
{
    if (v)
        sqlite3_bind_double(s, col, *v);
    else
        sqlite3_bind_null(s, col);
}

int db_insert(db_t *db,
              const double *bmp_temp_c, const double *bmp_press_hpa,
              const double *aht_temp_c, const double *aht_hum_pct)
{
    sqlite3_reset(db->stmt);
    bind_nullable(db->stmt, 1, bmp_temp_c);
    bind_nullable(db->stmt, 2, bmp_press_hpa);
    bind_nullable(db->stmt, 3, aht_temp_c);
    bind_nullable(db->stmt, 4, aht_hum_pct);

    if (sqlite3_step(db->stmt) != SQLITE_DONE) {
        fprintf(stderr, "db insert: %s\n", sqlite3_errmsg(db->sql));
        return -1;
    }
    return 0;
}
