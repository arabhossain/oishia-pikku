#!/usr/bin/env python3
"""Serve the Oishia controller on localhost for Web Bluetooth. No dependencies."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import argparse

root = Path(__file__).resolve().parents[1] / "learning"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", type=int, default=8080)
args = parser.parse_args()

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        files = {"/": ("dashboard.html", "text/html; charset=utf-8"),
                 "/dashboard-ble.js": ("dashboard-ble.js", "text/javascript; charset=utf-8"),
                 "/dashboard-face.js": ("dashboard-face.js", "text/javascript; charset=utf-8")}
        item = files.get(self.path.split("?", 1)[0])
        if not item:
            self.send_error(404)
            return
        payload = (root / item[0]).read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", item[1])
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

print(f"Open http://localhost:{args.port} on this computer and choose Bluetooth.", flush=True)
HTTPServer(("127.0.0.1", args.port), Handler).serve_forever()
