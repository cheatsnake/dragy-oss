# Synthetic web UI test setup

This folder tests the web UI without the device. `server.py` extracts the exact page string from
`src/page.h` (so the tested page is always in sync with the firmware) and injects `mock.js`, which
replaces `fetch` and `EventSource` with a synthetic device model. Every piece of data the page
normally gets over HTTP — `/api/state`, `/api/runs`, `/api/runs/{id}`, `POST /api/recalibrate` and
the `/events` SSE stream — comes from JavaScript. No ESP32, Wi-Fi or LittleFS involved.

## Run it

```sh
python3 test/server.py          # optional port argument, default 8000
```

Open http://localhost:8000 in any browser. The `TEST MOCK` strip at the bottom drives the
simulation:

| Button | Effect |
|---|---|
| Launch | simulates a 0–100 km/h run (~6.2 s), shows the result overlay and saves the run |
| Abort run | aborts the active run without a result |
| Recalibrate | 1.8 s of calibration (blinking dot) |
| GPS: on/off | toggles the fix; satellites ramp up/down |
| Clear runs | empties the run history |

The history starts with 3 seeded runs, one of them without a GPS date. The page's own Recalibrate
button is wired to the same mock endpoint.

## Headless check

```sh
python3 test/drive.py
```

Requires Chromium and chromedriver. It boots the mock, drives the page through
READY → run → result overlay → history → run detail, asserts the DOM values and saves screenshots
to `test/screenshots/` (git-ignored).

`mock.js` is test-only and is never part of the firmware image.
