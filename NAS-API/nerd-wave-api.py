#!/usr/bin/env python3
"""Nerd-Wave NAS API with lightweight request and streaming diagnostics."""

import itertools
import json
import os
import logging
import mimetypes
import re
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit, parse_qs, unquote

MUSIC_ROOT = Path(os.environ.get("NERDWAVE_MUSIC_ROOT", "/mnt/nas/music")).resolve()
HOST = "0.0.0.0"
PORT = 8080

AUDIO_EXT = {".mp3", ".flac", ".wav", ".ogg", ".opus", ".m4a", ".aac"}
STREAM_CHUNK_BYTES = 16384
STREAM_LOG_INTERVAL_S = 5.0
SLOW_WRITE_MS = 500.0

logging.basicConfig(level=logging.INFO, format="%(asctime)s.%(msecs)03d %(levelname)s %(message)s", datefmt="%Y-%m-%d %H:%M:%S")
LOG = logging.getLogger("nerd-wave-api")
_request_ids = itertools.count(1)
_stats_lock = threading.Lock()
_active_requests = 0
_active_streams = 0


def _adjust_counts(requests=0, streams=0):
    global _active_requests, _active_streams
    with _stats_lock:
        _active_requests += requests
        _active_streams += streams
        return _active_requests, _active_streams


class MusicAPI(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        # Avoid BaseHTTPRequestHandler's duplicate/default stderr access logs.
        LOG.debug("[HTTP] client=%s %s", self.client_address[0], format % args)

    def send_json(self, data, status=200):
        body = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def resolve_music_path(self, path):
        target = (MUSIC_ROOT / path.lstrip("/")).resolve()
        if not target.is_relative_to(MUSIC_ROOT):
            raise ValueError("Unzulaessiger Pfad")
        return target

    def do_GET(self):
        req_id = next(_request_ids)
        t0 = time.monotonic()
        active, streams = _adjust_counts(requests=1)
        url = urlsplit(self.path)
        route = url.path
        params = parse_qs(url.query)
        client = f"{self.client_address[0]}:{self.client_address[1]}"
        LOG.info("[REQ:%d] START client=%s method=GET path=%s active=%d streams=%d", req_id, client, self.path, active, streams)
        try:
            if route == "/api/status":
                self.send_json({"status": "ok", "service": "Nerd-Wave API", "version": "0.1"})
            elif route == "/api/list":
                self.list_directory(params, req_id)
            elif route.startswith("/music/"):
                relative = unquote(route[len("/music/"):])
                self.stream_file(relative, req_id)
            else:
                self.send_json({"error": "Not found"}, 404)
        except (ValueError, PermissionError) as e:
            LOG.warning("[REQ:%d] BAD_REQUEST %s", req_id, e)
            self._safe_json_error({"error": str(e)}, 400, req_id)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError) as e:
            LOG.info("[REQ:%d] CLIENT_DISCONNECTED %s", req_id, type(e).__name__)
        except OSError as e:
            LOG.exception("[REQ:%d] Dateisystem-/Socketfehler: %s", req_id, e)
            self._safe_json_error({"error": "Dateisystemfehler"}, 500, req_id)
        finally:
            active, streams = _adjust_counts(requests=-1)
            LOG.info("[REQ:%d] END duration_ms=%.1f active=%d streams=%d", req_id, (time.monotonic() - t0) * 1000, active, streams)

    def _safe_json_error(self, data, status, req_id):
        # Do not send an additional HTTP response after headers for a stream were sent.
        if getattr(self, "_stream_headers_sent", False):
            LOG.warning("[REQ:%d] HTTP response already started; no JSON error sent", req_id)
            return
        try:
            self.send_json(data, status)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError, OSError):
            LOG.info("[REQ:%d] Client disconnected before error response", req_id)

    def list_directory(self, params, req_id):
        start = time.monotonic()
        path = params.get("path", ["/"])[0]
        offset = int(params.get("offset", ["0"])[0])
        limit = int(params.get("limit", ["50"])[0])
        if offset < 0 or not 1 <= limit <= 200:
            raise ValueError("Ungueltige Pagination")
        directory = self.resolve_music_path(path)
        if not directory.is_dir():
            self.send_json({"error": "Ordner nicht gefunden"}, 404)
            return
        entries = []
        for item in directory.iterdir():
            if item.name.startswith(".") or not item.resolve().is_relative_to(MUSIC_ROOT):
                continue
            if item.is_dir():
                item_type = "directory"
            elif item.is_file() and item.suffix.lower() in AUDIO_EXT:
                item_type = "file"
            else:
                continue
            entry = {"name": item.name, "type": item_type, "path": "/" + item.relative_to(MUSIC_ROOT).as_posix()}
            if item_type == "file":
                entry["size"] = item.stat().st_size
            entries.append(entry)
        entries.sort(key=lambda e: (e["type"] != "directory", e["name"].casefold()))
        self.send_json({"path": path, "total": len(entries), "offset": offset, "limit": limit, "items": entries[offset:offset + limit]})
        LOG.info("[REQ:%d] LIST path=%r offset=%d limit=%d total=%d duration_ms=%.1f", req_id, path, offset, limit, len(entries), (time.monotonic() - start) * 1000)

    def stream_file(self, path, req_id):
        file = self.resolve_music_path(path)
        if not file.is_file() or file.suffix.lower() not in AUDIO_EXT:
            self.send_json({"error": "Datei nicht gefunden"}, 404)
            return
        size = file.stat().st_size
        start = 0
        end = size - 1
        partial = False
        range_header = self.headers.get("Range")
        if range_header:
            match = re.fullmatch(r"bytes=(\d*)-(\d*)", range_header.strip())
            if not match:
                self.send_error(416, "Invalid Range")
                return
            first, last = match.groups()
            if not first and not last:
                self.send_error(416, "Invalid Range")
                return
            if first:
                start = int(first)
                if last:
                    end = min(int(last), size - 1)
            else:
                start = max(0, size - int(last))
            if start >= size or end < start:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            partial = True

        length = end - start + 1
        mime = mimetypes.guess_type(file.name)[0] or "application/octet-stream"
        self.send_response(206 if partial else 200)
        self.send_header("Content-Type", mime)
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Length", str(length))
        if partial:
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.end_headers()
        self._stream_headers_sent = True

        active, streams = _adjust_counts(streams=1)
        LOG.info("[REQ:%d] STREAM START file=%r bytes=%d range=%d-%d active=%d streams=%d", req_id, path, length, start, end, active, streams)
        t_start = time.monotonic()
        window_start = t_start
        sent = 0
        window_sent = 0
        max_write_ms = 0.0
        last_write_ms = 0.0
        max_read_ms = 0.0
        outcome = "complete"
        try:
            with file.open("rb") as f:
                f.seek(start)
                remaining = length
                while remaining > 0:
                    t_read = time.monotonic()
                    chunk = f.read(min(STREAM_CHUNK_BYTES, remaining))
                    read_ms = (time.monotonic() - t_read) * 1000
                    max_read_ms = max(max_read_ms, read_ms)
                    if not chunk:
                        outcome = "short_read"
                        LOG.warning("[REQ:%d] STREAM short read remaining=%d", req_id, remaining)
                        break
                    t_write = time.monotonic()
                    self.wfile.write(chunk)
                    last_write_ms = (time.monotonic() - t_write) * 1000
                    max_write_ms = max(max_write_ms, last_write_ms)
                    sent += len(chunk)
                    window_sent += len(chunk)
                    remaining -= len(chunk)
                    if last_write_ms >= SLOW_WRITE_MS:
                        LOG.warning("[REQ:%d] STREAM SLOW_WRITE duration_ms=%.1f chunk=%d sent=%d remaining=%d", req_id, last_write_ms, len(chunk), sent, remaining)
                    now = time.monotonic()
                    elapsed = now - window_start
                    if elapsed >= STREAM_LOG_INTERVAL_S:
                        LOG.info("[REQ:%d] STREAM TX=%dB window=%.2fs rate=%.0fB/s total=%d/%d max_write_ms=%.1f max_read_ms=%.1f", req_id, window_sent, elapsed, window_sent / elapsed, sent, length, max_write_ms, max_read_ms)
                        window_start = now
                        window_sent = 0
                        max_write_ms = 0.0
                        max_read_ms = 0.0
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError) as e:
            outcome = type(e).__name__
            LOG.info("[REQ:%d] STREAM client disconnected (%s) after %d/%dB", req_id, outcome, sent, length)
        except OSError:
            outcome = "OSError"
            LOG.exception("[REQ:%d] STREAM I/O error after %d/%dB", req_id, sent, length)
        finally:
            _, streams = _adjust_counts(streams=-1)
            total_time = max(time.monotonic() - t_start, 1e-6)
            LOG.info("[REQ:%d] STREAM END result=%s bytes=%d/%d duration=%.2fs avg_rate=%.0fB/s last_write_ms=%.1f active_streams=%d", req_id, outcome, sent, length, total_time, sent / total_time, last_write_ms, streams)


if __name__ == "__main__":
    if not MUSIC_ROOT.is_dir():
        raise SystemExit("Musikordner nicht gefunden!")
    server = ThreadingHTTPServer((HOST, PORT), MusicAPI)
    LOG.info("Nerd-Wave API auf Port %d", PORT)
    LOG.info("Musikordner: %s", MUSIC_ROOT)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        LOG.info("Server beendet")
    finally:
        server.server_close()
