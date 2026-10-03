# AGENTS.md

ESP32-C3 SuperMini firmware for a car performance timer ("dragy" / car bench): 0–100 km/h, 0–60 km/h and speedometer modes. GPS (UART1) + ADXL345/346 accelerometer (I2C). The device runs a Wi-Fi SoftAP and serves a single dependency-free web UI from flash, with a JSON/SSE API and run history persisted in LittleFS.

## Commands

```sh
pio run -e esp32-c3-devkitm-1              # build firmware
pio run -e esp32-c3-devkitm-1 -t upload    # build and flash
pio device monitor                          # serial console at 115200

python3 test/drive.py                       # headless web UI smoke test (needs chromium + chromedriver)
python3 test/server.py [port]               # serve the mock UI for manual browsing (default 8000)
```

- PlatformIO may not be installed in this environment, so firmware builds often cannot be verified here. Say so rather than guessing build results.
- `test/drive.py` talks the WebDriver protocol directly via `urllib`; no selenium required. It needs `chromium` and `chromedriver` on `PATH`.
- No linter or formatter is configured.

## Architecture

- `src/config.h` — all constants (pins, addresses, thresholds, intervals). New tunables go here, not inline.
- `src/main.cpp` — setup, loop and the stage state machine: `SEARCHING_GPS` → `CALIBRATING_FIRST` → `READY` → `BENCH_ARMING` → `BENCH_RUNNING`.
- `src/gps.*` — GPS init, NMEA feeding, speed/satellite accessors.
- `src/accelerometer.*` — accelerometer init, calibration, motion detection.
- `src/wifi.*` — Wi-Fi SoftAP setup.
- `src/page.h` — the entire web UI (HTML/CSS/JS) stored in PROGMEM. Edit UI here; there are no separate asset files.
- `src/server.*` — async HTTP server, API routes and SSE.
- `src/runrecorder.*` — run sample recording and JSON generation.
- `src/storage.*` — LittleFS persistence of the last 10 runs (`/runs/<id>.json`) and the selected mode (`/mode`).
- `test/` — synthetic web UI test: `server.py` extracts the page string from `src/page.h`, `mock.js` replaces `fetch`/`EventSource` with a device model.
- `test/mock.js` mirrors firmware behavior (modes, launch/confirm/abort rules). When firmware behavior changes, update the mock and `test/drive.py` assertions to match.

Key rules:

- The loop has no `delay()` — everything is scheduled with `millis()` so calibration and GPS reading never block. Keep it that way.
- `test/server.py` reads the page straight out of `src/page.h`, so web UI changes must go through that file.
- Each saved run records the mode it was driven in; switching modes aborts an active run; Speedo mode never arms a measurement.

## Code style

- Constants: `ALL_CAPS` in `config.h`; enum values `SCREAMING_SNAKE` (see `BenchMode`).
- Functions: `PascalCase` (project style, e.g. `getSpeedKmh`, `startBenchRun`); locals `camelCase`; file-local helpers `static`.
- Floats use the `f` suffix (`0.2f`); serial strings wrapped in `F()`.
- Do not add comments unless asked.
- Web page: vanilla dependency-free HTML/CSS/JS, single dark monochrome theme.
- Commit messages: short lowercase subject; `feat:`/`fix:`/`docs:` prefixes are common but not mandatory.

## Verification

- After changing `page.h`, `server.*`, or mock behavior, run `python3 test/drive.py` and check that it passes.
- Screenshots land in `test/screenshots/` (git-ignored).
- There is no unit test suite for C++ code; the web UI smoke test is the only automated check.
