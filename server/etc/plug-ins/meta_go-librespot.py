#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# This file is part of snapcast
# Copyright (C) 2014-2025  Johannes Pohl
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import argparse
import json
import logging
import sys
import threading
import time

import requests

__version__ = "@version@"
__git_version__ = "@gitversion@"

identity = "Snapcast"

params = {
    "librespot-host": "127.0.0.1",
    "librespot-port": 24879,
    "stream": "SpotCast",
    "snapcast-host": "localhost",
    "snapcast-port": 1780,
}

log_level = logging.INFO
logger = logging.getLogger("meta_go-librespot")
logger.propagate = False
log_handler = logging.StreamHandler()
log_handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s: %(message)s"))
logger.addHandler(log_handler)
logger.setLevel(log_level)

BASE = None  # filled below


# JSON-RPC sender to Snapserver
def send(msg):
    sys.stdout.write(json.dumps(msg) + "\n")
    sys.stdout.flush()


def api_url(path: str) -> str:
    return f"{BASE}/{path.lstrip('/')}"


def post_json(path: str, payload=None, timeout=2.0):
    """POST with json payload (default {}), log result, return response or None."""
    if payload is None:
        payload = {}
    url = api_url(path)
    try:
        r = requests.post(url, json=payload, timeout=timeout)
        logger.debug(f"POST {url} -> {r.status_code} {r.reason} {r.text[:256]!r}")
        return r
    except requests.RequestException as e:
        logger.debug(f"POST {url} failed: {e}")
        return None


def get_simple(path: str, timeout=2.0):
    url = api_url(path)
    try:
        r = requests.get(url, timeout=timeout)
        logger.debug(f"GET  {url} -> {r.status_code} {r.reason} {r.text[:256]!r}")
        return r
    except requests.RequestException as e:
        logger.debug(f"GET  {url} failed: {e}")
        return None


class LibrespotControl(threading.Thread):
    def __init__(self, host, port, stream):
        super().__init__(daemon=True)
        self.host = host
        self.port = port
        self.stream = stream
        self._stop_event = threading.Event()
        self._lock = threading.Lock()

        self._properties = {
            "playbackStatus": "stopped",
            "shuffle": False,
            "loopStatus": "none",
            "volume": 100,
            "mute": False,
            "position": 0.0,
            "canGoNext": True,
            "canGoPrevious": True,
            "canPlay": True,
            "canPause": True,
            "canSeek": True,
            "canControl": True,
            "metadata": {
                "title": "",
                "artist": [],
                "album": "",
                "trackId": "",
                "artUrl": "",
                "trackNumber": 0,
                "discNumber": 0,
                "duration": 0.0,
                "contentCreated": "",
            },
        }
        self._metadata = self._properties["metadata"].copy()

    def run(self):
        while not self._stop_event.is_set():
            interval = 1.0
            try:
                r = get_simple("status", timeout=1.5)
                if not r or r.status_code // 100 != 2:
                    interval = 3.0
                    time.sleep(interval)
                    continue

                try:
                    data = r.json()
                except ValueError as e:
                    logger.debug(f"Invalid JSON: {e}, content: {r.text[:200]!r}")
                    data = None

                if not data or "track" not in data or data["track"] is None:
                    interval = 3.0
                    with self._lock:
                        if self._properties["playbackStatus"] != "stopped":
                            self._properties["playbackStatus"] = "stopped"
                            send(
                                {
                                    "jsonrpc": "2.0",
                                    "method": "Plugin.Stream.Player.Properties",
                                    "params": self._properties,
                                }
                            )
                else:
                    interval = 0.5
                    track = data["track"]
                    with self._lock:
                        self._metadata.update(
                            {
                                "title": track.get("name", ""),
                                "artist": track.get("artist_names", []),
                                "album": track.get("album_name", ""),
                                "trackId": track.get("uri", ""),
                                "artUrl": track.get("album_cover_url", ""),
                                "trackNumber": track.get("track_number", 0),
                                "discNumber": track.get("disc_number", 0),
                                "duration": track.get("duration", 0) / 1000.0,
                                "contentCreated": track.get("release_date", ""),
                            }
                        )
                        self._properties["metadata"] = self._metadata
                        self._properties["playbackStatus"] = (
                            "paused" if data.get("paused", True) else "playing"
                        )
                        self._properties["position"] = track.get("position", 0) / 1000.0

                    send(
                        {
                            "jsonrpc": "2.0",
                            "method": "Plugin.Stream.Player.Properties",
                            "params": self._properties,
                        }
                    )

            except requests.RequestException as e:
                logger.debug(f"Status request failed: {e}")
                interval = 3.0

            time.sleep(interval)

    def stop(self):
        self._stop_event.set()


def parse_args():
    """Configures and parses command-line arguments."""
    parser = argparse.ArgumentParser(
        description="""Connects go-librespot to Snapcast for metadata display and control.
        \nReport bugs to https://github.com/badaix/snapcast/issues
        """,
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "--librespot-host",
        type=str,
        default=params["librespot-host"],
        help="The hostname or IP address of the librespot instance.",
    )
    parser.add_argument(
        "--librespot-port",
        type=int,
        default=params["librespot-port"],
        help="The port of the librespot web API.",
    )
    parser.add_argument(
        "--stream",
        type=str,
        default=params["stream"],
        help="The name of the Snapcast stream to control.",
    )
    parser.add_argument(
        "--snapcast-host",
        type=str,
        default=params["snapcast-host"],
        help="The hostname or IP address of the Snapserver.",
    )
    parser.add_argument(
        "--snapcast-port",
        type=int,
        default=params["snapcast-port"],
        help="The JSON-RPC port of the Snapserver.",
    )
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging."
    )
    parser.add_argument(
        "-v", "--version", action="version", version=f"%(prog)s {__version__}"
    )

    args = parser.parse_args()

    # Update global configuration from parsed arguments
    params["librespot-host"] = args.librespot_host
    params["librespot-port"] = args.librespot_port
    params["stream"] = args.stream
    params["snapcast-host"] = args.snapcast_host
    params["snapcast-port"] = args.snapcast_port

    if args.debug:
        global log_level
        log_level = logging.DEBUG
        logger.setLevel(log_level)


if __name__ == "__main__":
    parse_args()

    BASE = f"http://{params['librespot-host']}:{params['librespot-port']}"

    ctrl = LibrespotControl(
        params["librespot-host"], params["librespot-port"], params["stream"]
    )
    ctrl.start()

    try:
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            try:
                req = json.loads(line)
                method = req.get("method", "")
                id_ = req.get("id")
                if method.endswith(".Control"):
                    action = req["params"].get("command", "")
                    logger.debug(f"Control command: {action}")

                    if action == "play":
                        post_json("player/resume")
                    elif action == "pause":
                        post_json("player/pause")
                    elif action == "playPause":
                        status = ctrl._properties["playbackStatus"]
                        post_json(
                            "player/pause" if status == "playing" else "player/resume"
                        )

                    elif action == "previous":
                        # prev always worked via POST without body
                        r = post_json("player/prev")
                        if not r or r.status_code // 100 != 2:
                            get_simple("player/prev")

                    elif action == "next":
                        # send an explicit empty JSON body; fallback to GET if needed
                        r = post_json("player/next", payload={})
                        if not r or r.status_code // 100 != 2:
                            get_simple("player/next")

                    elif action == "setPosition":
                        pos = float(req["params"].get("params", {}).get("position", 0))
                        post_json("player/seek", {"position": int(pos * 1000)})

                    # ack
                    if id_ is not None:
                        send({"jsonrpc": "2.0", "result": "ok", "id": id_})

                elif method.endswith(".GetProperties"):
                    send({"jsonrpc": "2.0", "id": id_, "result": ctrl._properties})

                elif method.endswith(".SetProperty"):
                    # We keep Spotify volume fixed; ignore for now.
                    if id_ is not None:
                        send({"jsonrpc": "2.0", "id": id_, "result": "ok"})

            except Exception as e:
                logger.debug(f"Error processing command: {e}")

    except KeyboardInterrupt:
        ctrl.stop()
