"""Local web server for ClashFree that tells the browser never to cache files.

Use this instead of "python -m http.server" while developing, so the browser always loads
the newest main.py / *.py / *.csv after you update them:

    cd web
    python serve.py
    -> open http://localhost:8000
"""

import http.server

PORT = 8000


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    """Serves files from this folder, adding 'Cache-Control: no-store' to every reply."""

    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".wasm": "application/wasm"}   # needed to load WebAssembly

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


if __name__ == "__main__":
    print(f"ClashFree running at http://localhost:{PORT}  (press Ctrl+C to stop)")
    http.server.ThreadingHTTPServer(("", PORT), NoCacheHandler).serve_forever()
