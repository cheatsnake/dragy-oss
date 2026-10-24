# Dragy OSS

Firmware for a car performance timer ("dragy") with 0–100 km/h and 0–60 km/h measurement modes plus a plain speedometer mode, built on an ESP32-C3 SuperMini with a GPS module and an ADXL345/346 accelerometer. The UI is being migrated from a local OLED display and speaker to a Wi-Fi served web page; the device currently exposes a Wi-Fi access point with a live web UI, a JSON/SSE API and run history persisted in flash.

## Hardware

| Module | Interface | Pins |
|---|---|---|
| GPS (PCAS, e.g. ATGM336H) | UART1 | RX=GPIO4, TX=GPIO3, 9600 -> 115200, 10 Hz, GPS+BDS |
| Accelerometer ADXL345/346 | I2C | SDA=GPIO1, SCL=GPIO0, INT1=GPIO5, addr 0x53 |

## How it works

The firmware runs a state machine with 3 stages:

1. **Init** — GPS is bootstrapped; once a fix is found, accelerometer calibration starts.
2. **Main** — current car speed is tracked continuously. After the car has moved and then stands still for 3 seconds, a single recalibration runs. Calibration runs in parallel with motion detection and discards itself if the car moves.
3. **Bench** — a launch is detected only after 400 ms of sustained acceleration (4 consecutive samples above 0.2 g), and the run is confirmed only after GPS confirms the car really moves (speed rose 4 km/h above the speed at the moment the launch was detected). A launch is ignored while the car is already rolling: if the speed at the launch moment is above 3 km/h — or the speed history shows the car was already moving — nothing is armed. The timer start is backdated to the exact sample where acceleration began, using a timestamped accelerometer history buffer. When the car reaches the selected mode's target (100 km/h in 0–100 mode, 60 km/h in 0–60 mode) the exact crossing is linearly interpolated between GPS samples and the final time is logged over serial. A run silently aborts back to the ready state when speed plateaus for 3 s without progress, drops 3 km/h below its peak (braking/lift-off), or the car comes to a stop — so ordinary town driving never produces a result.

The mode switcher on the live screen selects between **0–100**, **0–60** and **Speedo**; Speedo never arms a measurement at all. The selection is stored in flash and restored on power-up. Switching modes aborts any run in progress, and each saved run records the mode it was driven in.

Timing rules: accelerometer samples 10 times per second, GPS is fed continuously. The loop has no `delay()` — everything is scheduled with `millis()`, so calibration never blocks GPS reading. Calibration aborts itself if the car moves, so launch detection is never blocked for long.

## Sleep mode

After 5 minutes without accelerometer movement (and with GPS speed near zero, never mid-run or mid-calibration) the device shuts down to save power: the Wi-Fi AP and web server stop, the GPS module is asked to enter standby (best effort — only some modules honor `$PCAS12`), and the MCU enters deep sleep, leaving only the accelerometer's activity interrupt armed (~50 µA total). Movement above ~0.2 g asserts ADXL345 INT1 on GPIO5, which reboots the device; `setup()` logs "Woke by motion" and sends the GPS a standby-cancel plus hot-restart. The web UI shows a "Device asleep" notice before the AP disappears and reconnects on wake. A launch that starts at the exact moment of waking is missed, since boot and GPS re-fix take a few seconds.

## Web UI

On power-up the device starts a Wi-Fi access point:

| Setting | Value |
|---|---|
| SSID | `Dragy` |
| Password | `11112222` |
| URL | http://192.168.4.1 |

The page can be tested on a computer without the device via `test/` (see `test/README.md`).

The page is a single dark, monochrome, dependency-free HTML/CSS/JS app served entirely from flash. The live screen mirrors the old OLED (satellite count, blinking calibration dot, big current speed, elapsed timer during a run, result overlay after a finish) and updates over SSE. A three-way mode switcher (0–100 / 0–60 / Speedo) sits in the top bar. The History view lists the last 10 runs tagged with their mode; each opens a detail view with the final time, mode and hand-drawn speed and acceleration charts. A Recalibrate button triggers accelerometer recalibration.

## HTTP API

| Method | Path | Description |
|---|---|---|
| GET | `/` | web UI (single page) |
| GET | `/api/state` | current stage, mode, speed, satellites, calibration, elapsed and last result |
| GET | `/api/runs` | list of the last runs (`id`, `resultSeconds`, `date`, `mode`), newest first |
| GET | `/api/runs/{id}` | full run including mode and chart samples |
| GET | `/events` | SSE stream: `state` every 200 ms, `result` on finish, `sleep` when entering sleep mode, `ping` heartbeat |
| POST | `/api/recalibrate` | request an accelerometer recalibration |
| POST | `/api/mode/{mode}` | set the mode: `0-100`, `0-60` or `speedo` (aborts an active run) |

Each successful run is stored in LittleFS as `/runs/<id>.json` with the mode and 10 Hz samples of elapsed time, speed and acceleration. The selected mode is stored in `/mode` and restored on boot.

## Project layout

```
platformio.ini            build configuration and library dependencies
plan.md                   migration plan to the Wi-Fi web UI
src/config.h              all constants (pins, addresses, thresholds, intervals)
src/main.cpp              setup, loop and the stage state machine
src/gps.h/.cpp            GPS init, NMEA feeding, speed/satellite accessors
src/accelerometer.h/.cpp  accelerometer init, calibration, motion detection
src/sleep.h/.cpp          deep sleep entry, wake cause and module power-down
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

## License

Released under the [MIT License](LICENSE).
