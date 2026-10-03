#!/usr/bin/env python3
"""Headless smoke test for the web UI against the synthetic mock (server.py + mock.js)."""

import base64
import json
import os
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
        driver.screenshot("boot")
        print("  booted, %s" % satellites.strip())

        print("Driving a synthetic run...")
        driver.js("document.getElementById('mock-launch').click()")
        wait_for(
            "result overlay",
            lambda: driver.js("document.getElementById('bigunit').textContent === 's'"),
            timeout=20,
        )
        result_text = driver.js("document.getElementById('bigtext').textContent")
        result_seconds = float(result_text)
        assert 5.0 < result_seconds < 8.0, "implausible result: %s" % result_text
        driver.screenshot("result")
        print("  result overlay: %ss" % result_text)

        print("Checking history and detail views...")
        driver.js("document.getElementById('historybtn').click()")
        wait_for("history list", lambda: driver.count(".runrow") == 4)
        assert driver.js(
            "document.getElementById('runlist').textContent.includes('No GPS date')"
        ), "seeded run without GPS date missing from list"
        driver.screenshot("history")
        print("  history shows 4 runs (including the seeded no-date run)")

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
        driver.screenshot("detail")
        print("  detail charts rendered for %ss" % detail_result)

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
