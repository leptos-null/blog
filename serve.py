#!/usr/bin/env python3

"""
Local dev server that approximates GitHub Pages: serves site/ under /blog
and falls back to site/404.html (with a 404 status) for unmatched paths.
"""

import argparse
import http.server
import os

# GitHub Pages serves this repo at github.io/blog/, so every real URL path
# starts with /blog; requests outside that prefix are out of scope for us.
SCOPE_PREFIX = "/blog"
# The on-disk directory that URL paths (after SCOPE_PREFIX is stripped) get
# resolved against; this is what GitHub Pages deploys as the site root.
FS_ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "site")


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=FS_ROOT, **kwargs)

    def _in_scope(self, path: str):
        return path == SCOPE_PREFIX or path.startswith(SCOPE_PREFIX + "/") or path.startswith(SCOPE_PREFIX + "?")

    def translate_path(self, path):
        if not self._in_scope(path):
            raise RuntimeError(f"translate_path called with out-of-scope path {path!r}; send_head should have rejected this first")
        path = path[len(SCOPE_PREFIX):] or "/"
        return super().translate_path(path)

    def send_head(self):
        if not self._in_scope(self.path):
            self.send_error(404, "Not Found")
            return None

        # Only block "_workbench" as a directory component; if it's the last component,
        # it may be a file named "_workbench" itself, so let that fall through to the normal file lookup.
        # If the last component is actually a directory, the base handler 301-redirects to
        # add a trailing slash, which means "_workbench" is no longer the last path component,
        # so it still ends up blocked (404) here.
        path_components = self.path.split("/")
        if "_workbench" in path_components[:-1]:
            self.send_error(404, "Not Found")
            return None

        return super().send_head()

    def send_error(self, code, message=None, explain=None):
        if code == 404 and self._in_scope(self.path):
            try:
                with open(os.path.join(FS_ROOT, "404.html"), "rb") as f:
                    body = f.read()
            except OSError:
                super().send_error(code, message, explain)
                return
            self.send_response(404)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(body)
            return
        super().send_error(code, message, explain)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("port", default=8000, type=int, nargs="?", help="bind to this port (default: %(default)s)")
    args = parser.parse_args()
    http.server.test(HandlerClass=Handler, port=args.port)
