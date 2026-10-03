# Synthetic web UI test setup

This folder tests the web UI without the device. `server.py` extracts the exact page string from
`src/page.h` (so the tested page is always in sync with the firmware) and injects `mock.js`, which
replaces `fetch` and `EventSource` with a synthetic device model. Every piece of data the page
normally gets over HTTP — `/api/state`, `/api/runs`, `/api/runs/{id}`, `POST /api/recalibrate`,
`POST /api/mode/{mode}` and the `/events` SSE stream — comes from JavaScript. No ESP32, Wi-Fi or
LittleFS involved.

The mock mirrors the mode logic: the page's mode switcher drives it, runs finish at the selected
target (100 km/h or 60 km/h), switching modes aborts an active run, and in Speedo mode the Launch
button does nothing, exactly like the firmware never arms a measurement.

## Run it

```sh
python3 test/server.py          # optional port argument, default 8000
```

Open http://localhost:8000 in any browser. The `TEST MOCK` strip at the bottom drives the
simulation:

| Button | Effect |
|---|---|
| Launch | simulates a run to the selected mode's target (~6.2 s for 0–100, ~2.9 s for 0–60), shows the result overlay and saves the run; no-op in Speedo mode |
| Abort run | aborts the active run without a result |
| Recalibrate | 1.8 s of calibration (blinking dot) |
| GPS: on/off | toggles the fix; satellites ramp up/down |
| Clear runs | empties the run history |

The mode is switched through the page's own top-bar switcher, which POSTs to the mock. The history
starts with 3 seeded 0–100 runs, one of them without a GPS date. The page's own Recalibrate button is
wired to the same mock endpoint.

## Headless check

```sh
python3 test/drive.py
```

Requires Chromium and chromedriver. It boots the mock, drives the page through
READY → 0–100 run → result overlay → history → run detail → 0–60 run → history → Speedo, asserts the
DOM values (mode tags, a mid-run mode switch aborting the run, and Speedo never starting a
measurement) and saves screenshots to `test/screenshots/` (git-ignored).

`mock.js` is test-only and is never part of the firmware image.
