# weather-report

A lightweight weather station daemon for the Raspberry Pi. Reads temperature, humidity, and pressure from two I2C sensors, displays live readings on an OLED screen, logs JSON to stdout, stores every sample in a SQLite database, and pushes to Firestore.

## Hardware

| Device  | I2C addr | Measures                         |
|---------|----------|----------------------------------|
| BMP280  | `0x77`   | Temperature, barometric pressure |
| AHT20   | `0x38`   | Temperature, relative humidity   |
| SSD1315 | `0x3C`   | 128×64 OLED display (output)     |

A micro switch on GPIO 17 toggles the display on/off.

All I2C devices share `/dev/i2c-1`.

## Dependencies

```sh
sudo apt install libsqlite3-dev libcurl4-openssl-dev
```

## Build

```sh
make
```

## Usage

```sh
./sensors [period_seconds [db_path]]
```

| Argument         | Default      | Description                      |
|------------------|--------------|----------------------------------|
| `period_seconds` | `5`          | How often to take a sample       |
| `db_path`        | `sensors.db` | Path to the SQLite database file |

```sh
./sensors              # sample every 5 s, write to sensors.db
./sensors 60           # sample every minute
./sensors 30 /var/db/weather.db
```

Stop with `Ctrl-C` — the display is cleared on exit.

## Firestore

Set these environment variables to enable Firestore ingestion (optional — omitting them disables it silently):

```sh
export FIRESTORE_PROJECT_ID=my-project
export FIRESTORE_API_KEY=AIza...
export FIRESTORE_COLLECTION=samples   # optional, default: "samples"
```

## Output

Each sample prints a JSON object to stdout (when built with `-DDEBUG`):

```json
{
  "timestamp": "2026-04-29T14:00:00Z",
  "bmp280": {
    "temperature_c": 22.45,
    "pressure_hpa": 1013.25
  },
  "aht20": {
    "temperature_c": 22.10,
    "humidity_pct": 48.30
  }
}
```

If a sensor fails, its reading appears as `"error": "read failed"` and the corresponding database columns are `NULL`.

## Database

Samples are stored in a `samples` table:

```sql
CREATE TABLE samples (
  timestamp      TEXT NOT NULL,
  bmp_temp_c     REAL,
  bmp_press_hpa  REAL,
  aht_temp_c     REAL,
  aht_hum_pct    REAL
);
```

Query examples:

```sh
sqlite3 sensors.db "SELECT * FROM samples ORDER BY timestamp DESC LIMIT 10;"
sqlite3 sensors.db "SELECT AVG(aht_hum_pct) FROM samples WHERE timestamp >= datetime('now','-1 hour');"
```

## Display layout

```
>>Weather Report<<
2026-04-29 14:00:00

Temp:      22.3 °C
Press:   1013.3 hPa
Humidity:  48.3 %


2026-04-29 14:00:00
```

The temperature shown is the average of both sensors when both are available, falling back to whichever one succeeded. The display can be toggled on/off with the button on GPIO 17; the last sample is shown immediately when turned back on.
