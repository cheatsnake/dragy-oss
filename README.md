# Car-bench

Firmware for a 0–100 km/h performance timer ("dragy") built on an ESP32-C3 SuperMini with an SSD1306 OLED, a GPS module, an ADXL345/346 accelerometer and a passive speaker.

## Hardware

| Module | Interface | Pins |
|---|---|---|
| OLED SSD1306 128x64 | I2C | SDA=GPIO1, SCL=GPIO0, addr 0x3C |
| GPS (PCAS, e.g. ATGM336H) | UART1 | RX=GPIO4, TX=GPIO3, 9600 -> 115200, 10 Hz, GPS+BDS |
| Accelerometer ADXL345/346 | I2C (shared bus) | addr 0x53 |
| Speaker | GPIO | GPIO10 |

## How it works

The firmware runs a state machine with 3 stages:

1. **Init** — OLED and GPS are bootstrapped, the screen shows `Searching signal...`. Once a GPS fix is found the speaker beeps and accelerometer calibration starts (screen shows `Calibration...`).
2. **Main** — current car speed as a large centered number, satellite count in the top-left corner. After the car has moved and then stands still for 3 seconds, a single recalibration runs, shown as a blinking dot in the top-right corner. Calibration runs in parallel with motion detection and discards itself if the car moves.
3. **Bench** — a launch is detected only after 400 ms of sustained acceleration (4 consecutive samples above 0.2 g), and the timer view appears only after GPS confirms the car really moves (speed rose 4 km/h from the launch moment). The timer start is backdated to the exact sample where acceleration began, using a timestamped accelerometer history buffer. The count-up time is shown small in the bottom-right corner; when the car reaches 100 km/h the speaker beeps and the final time is shown large on screen (swapped with speed) for 20 seconds — the exact 100 km/h crossing is linearly interpolated between GPS samples. A run silently aborts back to the speedometer when speed plateaus for 3 s without progress, drops 3 km/h below its peak (braking/lift-off), or the car comes to a stop — so ordinary town driving never shows the timer.

Timing rules: display refreshes 5 times per second, accelerometer samples 10 times per second, GPS is fed continuously. The loop has no `delay()` — everything is scheduled with `millis()`, so calibration never blocks GPS reading or screen refresh. Calibration aborts itself if the car moves, so launch detection is never blocked for long.

## Project layout

```
platformio.ini         build configuration and library dependencies
src/config.h           all constants (pins, addresses, thresholds, intervals)
src/main.cpp           setup, loop and the stage state machine
src/gps.h/.cpp         GPS init, NMEA feeding, speed/satellite accessors
src/accelerometer.h/.cpp  accelerometer init, calibration, motion detection
src/speaker.h/.cpp     beep signals
src/display.h/.cpp     all screen rendering
draft/                 original draft sketches (GPS+OLED, accelerometer)
```

## Build and flash

Requires [PlatformIO](https://platformio.org/):

```sh
pio run -e esp32-c3-devkitm-1 -t upload   # build and flash
pio device monitor                        # serial console at 115200
```

Mount the device level in the car (any orientation works — calibration detects the vertical axis automatically).
