#!/usr/bin/env bash
# Batch-export samples from sensors.db to VictoriaMetrics (InfluxDB line protocol).
#
# Usage:
#   ./export_to_vm.sh [DB_PATH [VM_URL [SINCE]]]
#
# DB_PATH  – path to SQLite database      (default: sensors.db)
# VM_URL   – VictoriaMetrics base URL     (default: http://localhost:8428)
# SINCE    – only export rows after this  (default: beginning of time)
#            ISO-8601 timestamp, e.g. "2026-05-01T00:00:00Z"
#
# Environment variables (override positional args):
#   VM_DB, VM_URL, VM_SINCE, VM_BATCH_SIZE (default 500)
#
# The script tracks the last exported timestamp in a state file next to the DB
# so repeated runs only send new rows (like a simple incremental export).

set -euo pipefail

DB="${VM_DB:-${1:-sensors.db}}"
VM_BASE="${VM_URL:-${2:-http://localhost:8428}}"
SINCE="${VM_SINCE:-${3:-}}"
BATCH="${VM_BATCH_SIZE:-500}"
WRITE_URL="${VM_BASE}/write"
STATE_FILE="${DB}.vm_cursor"

if [[ ! -f "$DB" ]]; then
    echo "error: database not found: $DB" >&2
    exit 1
fi

# Prefer state file cursor, then explicit SINCE arg, then epoch start
if [[ -f "$STATE_FILE" ]]; then
    CURSOR="$(cat "$STATE_FILE")"
elif [[ -n "$SINCE" ]]; then
    CURSOR="$SINCE"
else
    CURSOR="1970-01-01T00:00:00Z"
fi

echo "Exporting rows after: $CURSOR"
echo "Destination: $WRITE_URL"

# Convert ISO-8601 timestamp to Unix nanoseconds for the InfluxDB line protocol
iso_to_ns() {
    # date(1) accepts the Z suffix on Linux
    local ts
    ts=$(date -d "$1" +%s 2>/dev/null) || { echo "0"; return; }
    echo "${ts}000000000"
}

total=0
last_ts="$CURSOR"

while true; do
    # Fetch a batch of rows strictly after the current cursor
    rows=$(sqlite3 -separator $'\t' "$DB" \
        "SELECT timestamp, bmp_temp_c, bmp_press_hpa, aht_temp_c, aht_hum_pct
         FROM samples
         WHERE timestamp > '${last_ts}'
         ORDER BY timestamp ASC
         LIMIT ${BATCH};")

    [[ -z "$rows" ]] && break

    lines=""
    batch_last_ts=""

    while IFS=$'\t' read -r ts bmp_temp bmp_press aht_temp aht_hum; do
        ns=$(iso_to_ns "$ts")
        batch_last_ts="$ts"

        # Emit one line per non-NULL field; each metric gets its own series
        [[ "$bmp_temp"  != "NULL" && -n "$bmp_temp"  ]] && \
            lines+="bmp_temp_c,sensor=bmp280 value=${bmp_temp} ${ns}"$'\n'
        [[ "$bmp_press" != "NULL" && -n "$bmp_press" ]] && \
            lines+="bmp_press_hpa,sensor=bmp280 value=${bmp_press} ${ns}"$'\n'
        [[ "$aht_temp"  != "NULL" && -n "$aht_temp"  ]] && \
            lines+="aht_temp_c,sensor=aht20 value=${aht_temp} ${ns}"$'\n'
        [[ "$aht_hum"   != "NULL" && -n "$aht_hum"   ]] && \
            lines+="aht_hum_pct,sensor=aht20 value=${aht_hum} ${ns}"$'\n'
    done <<< "$rows"

    row_count=$(echo "$rows" | wc -l)

    if [[ -n "$lines" ]]; then
        http_code=$(printf '%s' "$lines" | curl -s -o /dev/stderr -w "%{http_code}" \
            --request POST "$WRITE_URL" \
            --header "Content-Type: text/plain" \
            --data-binary @-)
        if [[ "$http_code" -lt 200 || "$http_code" -ge 300 ]]; then
            echo "error: VM returned HTTP $http_code — aborting" >&2
            exit 1
        fi
    fi

    total=$((total + row_count))
    last_ts="$batch_last_ts"
    echo "  sent $row_count rows (total: $total, cursor: $last_ts)"

    # Exact multiple of BATCH? There might be more rows — loop again.
    [[ "$row_count" -lt "$BATCH" ]] && break
done

# Persist cursor so next run is incremental
echo "$last_ts" > "$STATE_FILE"
echo "Done. Exported $total rows. Cursor saved to $STATE_FILE"
