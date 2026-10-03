# Synthetic web UI test setup

This folder tests the web UI without the device. `server.py` extracts the exact page string from
`src/page.h` (so the tested page is always in sync with the firmware) and injects `benchlogic.js`
and `mock.js`, which replace `fetch` and `EventSource` with a synthetic device model. Every piece of
data the page normally gets over HTTP — `/api/state`, `/api/runs`, `/api/runs/{id}`,
`POST /api/recalibrate`, `POST /api/mode/{mode}` and the `/events` SSE stream — comes from
JavaScript. No ESP32, Wi-Fi or LittleFS involved.

`benchlogic.js` is a 1:1 port of the firmware run-detection pipeline (accelerometer history,
launch detection, GPS confirm/abort rules, crossing interpolation); `mock.js` feeds it a synthetic
car with a lagged GPS, so the mock behaves like the firmware: the Launch button puts the pedal down,
the launch is detected after 400 ms of sustained acceleration, the timer is backdated to that
moment, and the run finishes at the selected target. Switching modes aborts an active run, and in
Speedo mode the Launch button does nothing, exactly like the firmware never arms a measurement. A
launch is only accepted from standstill (watch the `ready` hint in the mock strip).

## Run it

```sh
python3 test/server.py          # optional port argument, default 8000
```

Open http://localhost:8000 in any browser. The `TEST MOCK` strip at the bottom drives the
simulation:

| Button | Effect |
|---|---|
| Launch | puts the pedal down at 0.4–0.6 g; the firmware model detects the launch, backdates the timer, confirms via the lagged GPS and finishes at the selected target; no-op in Speedo mode or while the car is rolling |
| Abort run | lifts off and brakes to a stop; the run aborts through the normal speed-drop rules |
| Recalibrate | 1.8 s of calibration (blinking dot) |
| GPS: on/off | toggles the fix; satellites ramp up/down |
| Clear runs | empties the run history |

The mode is switched through the page's own top-bar switcher, which POSTs to the mock. The history
starts with 3 seeded 0–100 runs, one of them without a GPS date. The page's own Recalibrate button is
wired to the same mock endpoint. The status strip shows the mode, run count, current (GPS-lagged)
speed and a `ready` hint when a launch would be accepted.

## Headless check

```sh
python3 test/drive.py
```

Requires Chromium and chromedriver. It boots the mock, drives the page through
READY → 0–100 run → result overlay → history → run detail → 0–60 run → history → Speedo, asserts the
DOM values (backdated timer, mode tags, GPS dates rendered in the browser's local timezone, a mid-run
mode switch aborting the run, Speedo never starting a measurement, and no extra runs saved after a
finish) and saves screenshots to `test/screenshots/` (git-ignored).

## Detection-logic test

```sh
node test/detect.test.js
```

Runs `benchlogic.js` against synthetic accelerometer/GPS scenarios at firmware timing (10 Hz
sensors, 2 ms loop): standstill launches from 0.45 g down to barely above the 0.2 g threshold,
rolling launches that must be ignored, keeping the pedal down past the target, hard braking, mode
switches and the launch-detection walk-back. No browser needed.

`benchlogic.js` and `mock.js` are test-only and never part of the firmware image.
