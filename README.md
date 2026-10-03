# Car-bench

Firmware for a 0–100 km/h performance timer ("dragy") built on an ESP32-C3 SuperMini with a GPS module and an ADXL345/346 accelerometer. The UI is being migrated from a local OLED display and speaker to a Wi-Fi served web page (see `plan.md`); the device currently exposes a Wi-Fi access point with a live web UI, a JSON/SSE API and run history persisted in flash.

## Hardware

| Module | Interface | Pins |
|---|---|---|
| GPS (PCAS, e.g. ATGM336H) | UART1 | RX=GPIO4, TX=GPIO3, 9600 -> 115200, 10 Hz, GPS+BDS |
| Accelerometer ADXL345/346 | I2C | SDA=GPIO1, SCL=GPIO0, addr 0x53 |

## How it works

The firmware runs a state machine with 3 stages:

1. **Init** — GPS is bootstrapped; once a fix is found, accelerometer calibration starts.
2. **Main** — current car speed is tracked continuously. After the car has moved and then stands still for 3 seconds, a single recalibration runs. Calibration runs in parallel with motion detection and discards itself if the car moves.
3. **Bench** — a launch is detected only after 400 ms of sustained acceleration (4 consecutive samples above 0.2 g), and the run is confirmed only after GPS confirms the car really moves (speed rose 4 km/h from the launch moment). The timer start is backdated to the exact sample where acceleration began, using a timestamped accelerometer history buffer. When the car reaches 100 km/h the exact crossing is linearly interpolated between GPS samples and the final time is logged over serial. A run silently aborts back to the ready state when speed plateaus for 3 s without progress, drops 3 km/h below its peak (braking/lift-off), or the car comes to a stop — so ordinary town driving never produces a result.

Timing rules: accelerometer samples 10 times per second, GPS is fed continuously. The loop has no `delay()` — everything is scheduled with `millis()`, so calibration never blocks GPS reading. Calibration aborts itself if the car moves, so launch detection is never blocked for long.

## Web UI

On power-up the device starts a Wi-Fi access point:

| Setting | Value |
|---|---|
| SSID | `Dragy` |
| Password | `dragybench` |
| URL | http://192.168.4.1 |

The page can be tested on a computer without the device via `test/` (see `test/README.md`).

The page is a single dark, monochrome, dependency-free HTML/CSS/JS app served entirely from flash. The live screen mirrors the old OLED (satellite count, blinking calibration dot, big current speed, elapsed timer during a run, result overlay after a finish) and updates over SSE. The History view lists the last 10 runs; each opens a detail view with the final time and hand-drawn speed and acceleration charts. A Recalibrate button triggers accelerometer recalibration.

## HTTP API

| Method | Path | Description |
|---|---|---|
| GET | `/` | web UI (single page) |
| GET | `/api/state` | current stage, speed, satellites, calibration, elapsed and last result |
| GET | `/api/runs` | list of the last runs (`id`, `resultSeconds`, `date`), newest first |
| GET | `/api/runs/{id}` | full run including chart samples |
| GET | `/events` | SSE stream: `state` every 200 ms, `result` on finish, `ping` heartbeat |
| POST | `/api/recalibrate` | request an accelerometer recalibration |

Each successful run is stored in LittleFS as `/runs/<id>.json` with 10 Hz samples of elapsed time, speed and acceleration.

## Project layout

```
platformio.ini            build configuration and library dependencies
plan.md                   migration plan to the Wi-Fi web UI
src/config.h              all constants (pins, addresses, thresholds, intervals)
src/main.cpp              setup, loop and the stage state machine
src/gps.h/.cpp            GPS init, NMEA feeding, speed/satellite accessors
src/accelerometer.h/.cpp  accelerometer init, calibration, motion detection
src/wifi.h/.cpp           Wi-Fi SoftAP setup
src/page.h                web UI (HTML/CSS/JS) stored in PROGMEM
src/server.h/.cpp         async HTTP server, API routes and SSE
src/runrecorder.h/.cpp    run sample recording and JSON generation
src/storage.h/.cpp        LittleFS persistence of the last 10 runs
test/                     synthetic web UI test (JS-mocked API, headless Chrome check)
draft/                    original draft sketches (GPS+OLED, accelerometer)
```

## Build and flash

Requires [PlatformIO](https://platformio.org/):

```sh
pio run -e esp32-c3-devkitm-1 -t upload   # build and flash
pio device monitor                        # serial console at 115200
```

Mount the device level in the car (any orientation works — calibration detects the vertical axis automatically).
