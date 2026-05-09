#!/usr/bin/env bash
# Export samples from sensors.db to VictoriaMetrics (InfluxDB line protocol).
# Tracks progress in sensors.db.vm_cursor for incremental runs.
#
# Usage: ./export_to_vm.sh [DB_PATH [VM_URL]]
# Defaults: sensors.db, http://localhost:8428

set -euo pipefail

DB="${1:-sensors.db}"
VM_URL="${2:-http://localhost:8428}"
CURSOR_FILE="${DB}.vm_cursor"
CURSOR="1970-01-01T00:00:00Z"

[[ -f "$CURSOR_FILE" ]] && CURSOR="$(cat "$CURSOR_FILE")"

echo "Exporting rows after: $CURSOR"

lines=$(sqlite3 -separator $'\t' "$DB" \
    "SELECT timestamp, bmp_temp_c, bmp_press_hpa, aht_temp_c, aht_hum_pct
     FROM samples WHERE timestamp > '${CURSOR}' ORDER BY timestamp ASC;" \
| while IFS=$'\t' read -r ts bmp_temp bmp_press aht_temp aht_hum; do
    ns=$(date -d "$ts" +%s%N)
    [[ "$bmp_temp"  != "NULL" && -n "$bmp_temp"  ]] && echo "temperature,sensor=bmp280 value=${bmp_temp} ${ns}"
    [[ "$bmp_press" != "NULL" && -n "$bmp_press" ]] && echo "pressure,sensor=bmp280 value=${bmp_press} ${ns}"
    [[ "$aht_temp"  != "NULL" && -n "$aht_temp"  ]] && echo "temperature,sensor=aht20 value=${aht_temp} ${ns}"
    [[ "$aht_hum"   != "NULL" && -n "$aht_hum"   ]] && echo "humidity,sensor=aht20 value=${aht_hum} ${ns}"
done)

if [[ -z "$lines" ]]; then
    echo "No new rows."
    exit 0
fi

row_count=$(echo "$lines" | wc -l)

http_code=$(printf '%s\n' "$lines" | curl -s -o /dev/stderr -w "%{http_code}" \
    --request POST "${VM_URL}/write" \
    --header "Content-Type: text/plain" \
    --data-binary @-)

if [[ "$http_code" -lt 200 || "$http_code" -ge 300 ]]; then
    echo "error: VM returned HTTP $http_code" >&2
    exit 1
fi

new_cursor=$(sqlite3 "$DB" \
    "SELECT timestamp FROM samples WHERE timestamp > '${CURSOR}' ORDER BY timestamp DESC LIMIT 1;")
echo "$new_cursor" > "$CURSOR_FILE"
echo "Exported $row_count lines. Cursor: $new_cursor"
