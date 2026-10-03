#!/usr/bin/env python3
"""Serve the real web UI from src/page.h with the API mocked in JavaScript."""

import os
import re
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE_SOURCE = os.path.join(os.path.dirname(HERE), "src", "page.h")
MOCK_SCRIPT = os.path.join(HERE, "mock.js")
INJECTION = '<script src="mock.js"></script>'


def load_page():
    with open(PAGE_SOURCE, "r", encoding="utf-8") as source:
        match = re.search(r'R"HTML\((.*)\)HTML"', source.read(), re.S)
    if not match:
        raise RuntimeError("web page raw string not found in " + PAGE_SOURCE)
    return match.group(1).replace("<body>", "<body>\n" + INJECTION, 1)


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def _send(self, status, content_type, body):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path == "/":
            self._send(200, "text/html; charset=utf-8", load_page().encode("utf-8"))
        elif path == "/mock.js":
            with open(MOCK_SCRIPT, "rb") as script:
                self._send(200, "application/javascript; charset=utf-8", script.read())
        else:
            self._send(404, "text/plain", b"not found")

    def log_message(self, *args):
        pass


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print("Serving the mocked Dragy UI at http://localhost:%d" % port)
    print("Page extracted live from src/page.h; API/SSE data mocked by test/mock.js")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
