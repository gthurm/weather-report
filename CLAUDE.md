# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```sh
make          # build the `sensors` binary
make clean    # remove binary and object files
```

No external libraries — only Linux kernel headers (`linux/i2c-dev.h`) and libc.

## Run

```sh
./sensors [period_seconds]   # default: 5
```

Must run on the Raspberry Pi (or any Linux board with `/dev/i2c-1`). Loops indefinitely, sampling every `period_seconds` and updating both the OLED and a JSON line on stdout. Exits cleanly on `SIGINT`/`SIGTERM` (clears the display).

## Hardware

| Device  | I2C addr | Provides                       |
|---------|----------|--------------------------------|
| BMP280  | `0x77`   | temperature (°C), pressure (Pa)|
| AHT20   | `0x38`   | temperature (°C), humidity (%) |
| SSD1315 | `0x3C`   | 128×64 OLED display            |

All three share `/dev/i2c-1`. The main loop opens the bus once and each module calls `ioctl(fd, I2C_SLAVE, addr)` to switch targets before every transaction.

## Architecture

`sensors.c` is the only entry point. It orchestrates three independent modules:

- **`bmp280.c`** — single public function `bmp280_sample(fd, &temp, &press)`. Reads factory calibration registers on every call, triggers a forced-mode measurement, then applies the double-precision compensation formulas from the BMP280 datasheet (§4.2.3).
- **`aht20.c`** — single public function `aht20_sample(fd, &temp, &hum)`. Checks the calibration status bit on every call and re-initialises if needed before triggering a measurement.
- **`ssd1315.c`** — opaque-handle driver for the SSD1306/SSD1315 OLED. Maintains a full framebuffer in memory; `ssd1315_flush` streams all 1024 bytes in one I2C write. Bundles a 5×7 font covering ASCII 0x20–0x7E.

Data flow per iteration: sample both sensors → update OLED → print JSON to stdout. Sensor errors are non-fatal and appear as `"error": "read failed"` in JSON. On `SIGINT`/`SIGTERM` the loop exits, the display is cleared, and the bus fd is closed.
