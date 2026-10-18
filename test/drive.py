#!/usr/bin/env python3
"""Headless smoke test for the web UI against the synthetic mock (server.py + benchlogic.js + mock.js)."""

import base64
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
SCREENSHOT_DIR = os.path.join(HERE, "screenshots")


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def http(method, url, payload=None, timeout=5):
    data = json.dumps(payload).encode() if payload is not None else None
    request = urllib.request.Request(
        url, data=data, method=method, headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        body = response.read().decode()
    return json.loads(body) if body else None


def wait_for(description, check, timeout=25.0, interval=0.25):
    deadline = time.time() + timeout
    last_error = None
    while time.time() < deadline:
        try:
            result = check()
            if result:
                return result
        except Exception as error:  # noqa: BLE001 - poll-until-ready helper
            last_error = error
        time.sleep(interval)
    raise RuntimeError(
        "timeout waiting for " + description + (": " + str(last_error) if last_error else "")
    )


class Driver:
    def __init__(self, chromedriver, chromium, port, window_size):
        self.base = "http://127.0.0.1:%d" % port
        session = http(
            "POST",
            self.base + "/session",
            {
                "capabilities": {
                    "alwaysMatch": {
                        "browserName": "chrome",
                        "goog:chromeOptions": {
                            "binary": chromium,
                            "args": [
                                "--headless=new",
                                "--no-sandbox",
                                "--disable-gpu",
                                "--window-size=" + window_size,
                                "--force-device-scale-factor=1",
                            ],
                        },
                    }
                }
            },
        )
        self.session = session["value"]["sessionId"]

    def call(self, method, path, payload=None):
        return http(method, self.base + "/session/" + self.session + path, payload)

    def get(self, url):
        self.call("POST", "/url", {"url": url})

    def find(self, selector):
        value = self.call(
            "POST", "/element", {"using": "css selector", "value": selector}
        )["value"]
        return value["element-6066-11e4-a52e-4f735466cecf"]

    def click(self, selector):
        self.call("POST", "/element/" + self.find(selector) + "/click", {})

    def count(self, selector):
        return len(
            self.call("POST", "/elements", {"using": "css selector", "value": selector})[
                "value"
            ]
        )

    def js(self, script):
        return self.call(
            "POST", "/execute/sync", {"script": "return (" + script + ")", "args": []}
        )["value"]

    def screenshot(self, name):
        os.makedirs(SCREENSHOT_DIR, exist_ok=True)
        data = self.call("GET", "/screenshot")["value"]
        path = os.path.join(SCREENSHOT_DIR, name + ".png")
        with open(path, "wb") as image:
            image.write(base64.b64decode(data))
        print("  screenshot test/screenshots/%s.png" % name)

    def close(self):
        try:
            self.call("DELETE", "")
        except Exception:  # noqa: BLE001 - session may already be gone
            pass


def main():
    chromium = (
        shutil.which("chromium")
        or shutil.which("chromium-browser")
        or shutil.which("google-chrome")
    )
    chromedriver = shutil.which("chromedriver")
    if not chromium or not chromedriver:
        sys.exit("chromium and chromedriver are required for test/drive.py")

    server_port = free_port()
    driver_port = free_port()

    print("Starting mock server on port %d" % server_port)
    server = subprocess.Popen(
        [sys.executable, os.path.join(HERE, "server.py"), str(server_port)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
    )
    driver_process = subprocess.Popen(
        [chromedriver, "--port=%d" % driver_port],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
    )
    driver = None

    def server_ready():
        with urllib.request.urlopen(
            "http://127.0.0.1:%d/" % server_port, timeout=2
        ) as response:
            return response.status == 200

    try:
        wait_for("mock server", server_ready)
        wait_for(
            "chromedriver",
            lambda: http("GET", "http://127.0.0.1:%d/status" % driver_port)["value"][
                "ready"
            ],
        )

        driver = Driver(chromedriver, chromium, driver_port, "420,1000")
        print("Loading page and waiting for boot (satellites + first calibration)...")
        driver.get("http://127.0.0.1:%d/" % server_port)

        wait_for(
            "READY stage",
            lambda: driver.js(
                "document.getElementById('bigvalue').hidden === false &&"
                " document.getElementById('message').hidden === true"
            ),
        )
        satellites = driver.js("document.getElementById('sats').textContent")
        assert "sats" in satellites, "satellite count missing: %r" % satellites
        active_mode = driver.js("document.querySelector('.modebtn.active').dataset.mode")
        assert active_mode == "0-100", "default mode is not 0-100: %r" % active_mode
        driver.screenshot("boot")
        print("  booted, %s, mode %s" % (satellites.strip(), active_mode))

        print("Driving a synthetic 0-100 run...")
        wait_for(
            "mock ready to launch",
            lambda: "ready" in driver.js(
                "document.getElementById('mock-status').textContent"
            ),
        )
        driver.js("document.getElementById('mock-launch').click()")
        wait_for(
            "run timer active",
            lambda: driver.js("document.getElementById('timer').textContent !== ''"),
            timeout=5,
        )
        backdated = driver.js("parseFloat(document.getElementById('timer').textContent)")
        assert backdated >= 0.25, "timer not backdated to the launch moment: %r" % backdated
        wait_for(
            "result overlay",
            lambda: driver.js("document.getElementById('bigunit').textContent === 's'"),
            timeout=20,
        )
        result_text = driver.js("document.getElementById('bigtext').textContent")
        result_seconds = float(result_text)
        assert 5.0 < result_seconds < 8.0, "implausible result: %s" % result_text
        time.sleep(1.5)
        status_text = driver.js("document.getElementById('mock-status').textContent")
        assert "4 run(s)" in status_text, "unexpected extra runs saved: %r" % status_text
        driver.screenshot("result")
        print("  0-100 result overlay: %ss (timer backdated %.1fs)" % (result_text, backdated))

        print("Checking history and detail views...")
        driver.js("document.getElementById('historybtn').click()")
        wait_for("history list", lambda: driver.count(".runrow") == 4)
        assert driver.js(
            "document.getElementById('runlist').textContent.includes('No GPS date')"
        ), "seeded run without GPS date missing from list"
        assert driver.js(
            "document.querySelector('#runlist .runrow .runmode').textContent === '0-100'"
        ), "0-100 tag missing from history"
        expected_local = driver.js(
            "(function () {"
            " var d = new Date('2026-09-28T18:47:09Z');"
            " function p(v) { return (v < 10 ? '0' : '') + v; }"
            " return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate()) + ' ' +"
            " p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds());"
            "})()"
        )
        listed_date = driver.js(
            "document.querySelectorAll('#runlist .runrow')[2].querySelector('.rundate').textContent"
        )
        assert listed_date == expected_local, (
            "GPS date not rendered in the client timezone: %r != %r"
            % (listed_date, expected_local)
        )
        driver.screenshot("history")
        print("  history shows 4 runs (including the seeded no-date run), GPS date in local time")

        driver.click(".runrow")
        wait_for(
            "run detail",
            lambda: driver.js(
                "document.getElementById('detail').hidden === false &&"
                " document.querySelectorAll('#speedchart, #accelchart').length === 2"
            ),
        )
        time.sleep(0.5)
        detail_result = driver.js("document.getElementById('dresult').textContent")
        assert abs(float(detail_result) - result_seconds) < 0.01, (
            "detail result %s does not match %s" % (detail_result, result_text)
        )
        assert driver.js("document.getElementById('dmode').textContent") == "0-100", (
            "detail mode tag missing"
        )
        assert driver.js("document.getElementById('dalt').hidden === false"), (
            "0-60 split missing on the 0-100 detail page"
        )
        alt_text = driver.js("document.getElementById('dalt').textContent")
        alt_match = re.fullmatch(r"(\d+\.\d{2})s \(0-60\)", alt_text)
        assert alt_match, "unexpected 0-60 split text: %r" % alt_text
        alt_seconds = float(alt_match.group(1))
        assert 3.5 < alt_seconds < result_seconds, (
            "implausible 0-60 split %ss for a %ss run" % (alt_text, detail_result)
        )
        driver.screenshot("detail")
        print("  detail charts rendered for %ss (0-60: %s)" % (detail_result, alt_text))

        driver.js("document.getElementById('backlive').click()")
        wait_for(
            "live view",
            lambda: driver.js("document.getElementById('live').hidden === false"),
        )

        print("Switching to 0-60 and driving a run...")
        driver.js("document.querySelector('.modebtn[data-mode=\"0-60\"]').click()")
        wait_for(
            "0-60 mode active",
            lambda: driver.js(
                "document.querySelector('.modebtn.active').dataset.mode === '0-60'"
            ),
        )
        wait_for(
            "mock ready to launch",
            lambda: "ready" in driver.js(
                "document.getElementById('mock-status').textContent"
            ),
        )
        driver.js("document.getElementById('mock-launch').click()")
        wait_for(
            "0-60 result overlay",
            lambda: driver.js("document.getElementById('bigunit').textContent === 's'"),
            timeout=20,
        )
        result60_text = driver.js("document.getElementById('bigtext').textContent")
        result60_seconds = float(result60_text)
        assert 2.5 < result60_seconds < 3.8, "implausible 0-60 result: %s" % result60_text
        time.sleep(1.5)
        status_text = driver.js("document.getElementById('mock-status').textContent")
        assert "5 run(s)" in status_text, "unexpected extra runs saved: %r" % status_text
        driver.screenshot("result60")
        print("  0-60 result overlay: %ss" % result60_text)

        print("Checking the 0-60 run is stored with its mode...")
        driver.js("document.getElementById('historybtn').click()")
        wait_for("history list", lambda: driver.count(".runrow") == 5)
        assert driver.js(
            "document.querySelector('#runlist .runrow .runmode').textContent === '0-60'"
        ), "0-60 tag missing from history"
        driver.screenshot("history60")
        print("  history shows 5 runs, newest tagged 0-60")

        driver.click(".runrow")
        wait_for(
            "0-60 run detail",
            lambda: driver.js(
                "document.getElementById('detail').hidden === false &&"
                " document.getElementById('dmode').textContent === '0-60'"
            ),
        )
        assert driver.js("document.getElementById('dalt').hidden === true"), (
            "0-60 split should not be shown on a 0-60 run"
        )
        print("  0-60 run detail hides the 0-60 split")

        driver.js("document.getElementById('backlive').click()")
        wait_for(
            "live view",
            lambda: driver.js("document.getElementById('live').hidden === false"),
        )

        print("Switching to Speedo mid-run: aborts the run and ignores launches...")
        wait_for(
            "mock ready to launch",
            lambda: "ready" in driver.js(
                "document.getElementById('mock-status').textContent"
            ),
        )
        driver.js("document.getElementById('mock-launch').click()")
        wait_for(
            "run timer active",
            lambda: driver.js("document.getElementById('timer').textContent !== ''"),
            timeout=5,
        )
        driver.js("document.querySelector('.modebtn[data-mode=\"speedo\"]').click()")
        wait_for(
            "speedo mode active",
            lambda: driver.js(
                "document.querySelector('.modebtn.active').dataset.mode === 'speedo'"
            ),
        )
        wait_for(
            "active run aborted",
            lambda: driver.js(
                "document.getElementById('timer').textContent === '' &&"
                " document.getElementById('bigunit').textContent === 'km/h'"
            ),
        )
        driver.js("document.getElementById('mock-launch').click()")
        time.sleep(2.0)
        assert (
            driver.js("document.getElementById('bigunit').textContent") == "km/h"
        ), "speedo mode started a measurement"
        assert driver.js("document.getElementById('timer').textContent") == "", (
            "timer active in speedo mode"
        )
        assert driver.js(
            "document.querySelector('.modebtn.active').dataset.mode"
        ) == "speedo", "speedo mode not active"
        driver.screenshot("speedo")
        print("  run aborted on switch, speedo launch ignored, still showing speed")

        print("Sleeping the device after simulated idle minutes...")
        driver.js("document.getElementById('mock-sleepnow').click()")
        wait_for(
            "sleep banner",
            lambda: driver.js(
                "document.getElementById('message').hidden === false &&"
                " document.getElementById('message').textContent.indexOf('asleep') !== -1"
            ),
            timeout=5,
        )
        driver.screenshot("sleep")
        print("  device asleep, banner shown")

        print("Movement wakes the device (the launch is consumed by the reboot)...")
        driver.js("document.getElementById('mock-launch').click()")
        wait_for(
            "reboot searching",
            lambda: driver.js(
                "document.getElementById('message').hidden === false &&"
                " document.getElementById('message').textContent.indexOf('Searching') !== -1"
            ),
            timeout=5,
        )
        time.sleep(1.0)
        assert driver.js("document.getElementById('timer').textContent === ''"), (
            "run started while waking from sleep"
        )
        print("  device rebooted to GPS search, no run started on wake")

        wait_for(
            "READY stage after wake",
            lambda: driver.js(
                "document.getElementById('bigvalue').hidden === false &&"
                " document.getElementById('message').hidden === true"
            ),
        )
        driver.js("document.querySelector('.modebtn[data-mode=\"0-100\"]').click()")
        wait_for(
            "0-100 mode active after wake",
            lambda: driver.js(
                "document.querySelector('.modebtn.active').dataset.mode === '0-100'"
            ),
        )
        wait_for(
            "mock ready to launch",
            lambda: "ready" in driver.js(
                "document.getElementById('mock-status').textContent"
            ),
        )
        driver.js("document.getElementById('mock-launch').click()")
        wait_for(
            "result overlay after wake",
            lambda: driver.js("document.getElementById('bigunit').textContent === 's'"),
            timeout=20,
        )
        result_wake_text = driver.js("document.getElementById('bigtext').textContent")
        result_wake_seconds = float(result_wake_text)
        assert 5.0 < result_wake_seconds < 8.0, "implausible result: %s" % result_wake_text
        time.sleep(1.5)
        status_text = driver.js("document.getElementById('mock-status').textContent")
        assert "6 run(s)" in status_text, "unexpected extra runs saved: %r" % status_text
        driver.screenshot("wake_result")
        print("  run after wake: %ss, 6 runs stored" % result_wake_text)

        print("PASS: web UI flows work against the synthetic mock")
        return 0
    finally:
        if driver:
            driver.close()
        driver_process.terminate()
        server.terminate()
        driver_process.wait(timeout=10)
        server.wait(timeout=10)


if __name__ == "__main__":
    sys.exit(main())
