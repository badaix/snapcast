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

# Dependencies:
# - websocket-client>=0.58
# - requests

import argparse
import json
import logging
import sys
import threading
import time

import requests
import websocket

__version__ = "@version@"
__git_version__ = "@gitversion@"

identity = "Snapcast"

defaults = {
    "librespot_host": "127.0.0.1",
    "librespot_port": 24879,
    "stream": "SpotCast",
    "snapcast_host": "localhost",
    "snapcast_port": 1780,
}

log_level = logging.INFO
logger = logging.getLogger("meta_go-librespot")
logger.propagate = False
log_handler = logging.StreamHandler()
log_handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s: %(message)s"))
logger.addHandler(log_handler)
logger.setLevel(log_level)


def send(msg):
    sys.stdout.write(json.dumps(msg) + "\n")
    sys.stdout.flush()


class LibrespotControl:
    def __init__(self, params):
        self._params = params

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

        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self.BASE_URL = (
            f"http://{self._params.librespot_host}:{self._params.librespot_port}"
        )

        wsversion = websocket.__version__.split(".")

        if int(wsversion[0]) == 0 and int(wsversion[1]) < 58:
            logger.error(
                "websocket-client version 0.58.0 or higher required, installed: %s, exiting.",
                websocket.__version__,
            )
            exit()

        ws_url = (
            f"ws://{self._params.librespot_host}:{self._params.librespot_port}/events"
        )
        logger.info("Connecting to librespot WebSocket at %s", ws_url)

        self.websocket = websocket.WebSocketApp(
            url=ws_url,
            on_message=self.on_ws_message,
            on_error=self.on_ws_error,
            on_open=self.on_ws_open,
            on_close=self.on_ws_close,
        )

        self.websocket_thread = threading.Thread(target=self.websocket_loop, args=())
        self.websocket_thread.name = "LibrespotControl"
        self.websocket_thread.start()

    def websocket_loop(self):
        logger.info("Started LibrespotControl loop")
        while not self._stop_event.is_set():
            try:
                self.websocket.run_forever()
            except Exception as e:
                logger.info(f"Exception: {str(e)}")
                self.websocket.close()

            if not self._stop_event.is_set():
                logger.info("Disconnected. Retrying in 2 seconds...")
                time.sleep(2)

    def on_ws_message(self, ws, message):
        logger.debug("Snapcast RPC websocket message received: %s", message)

        try:
            jmsg = json.loads(message)
            event = jmsg["type"]
            data = jmsg["data"]
        except json.JSONDecodeError as e:
            logger.error("Invalid JSON from WebSocket: %s, content: %s", e, message)
            return

        logger.info(f"Event: {type}, msg: {data}")
        sendupdate = True

        if event == "not_playing":
            self._properties["playbackStatus"] = "stopped"
        elif event == "paused":
            self._properties["playbackStatus"] = "paused"
        elif event == "playing":
            self._properties["playbackStatus"] = "playing"
        elif event == "volume":
            self._properties["volume"] = int(data["value"] / data["max"] * 100.0)
        elif event == "seek":
            self._properties["position"] = float(data["position"]) / 1000.0
        elif event == "metadata":
            with self._lock:
                self._metadata.update(
                    {
                        "trackId": data.get("uri", ""),
                        "title": data.get("name", ""),
                        "artist": data.get("artist_names", []),
                        "album": data.get("album_name", ""),
                        "artUrl": data.get("album_cover_url", ""),
                        "duration": data.get("duration", 0) / 1000.0,
                        "contentCreated": data.get("release_date", ""),
                        "trackNumber": data.get("track_number", 0),
                        "discNumber": data.get("disc_number", 0),
                    }
                )
                self._properties["metadata"] = self._metadata
                self._properties["position"] = data.get("position", 0) / 1000.0

        # elif event == 'active':
        # elif event == 'inactive':
        # elif event == 'playbackHaltStateChanged':
        # elif event == 'sessionCleared':
        # elif event == 'sessionChanged':
        # elif event == 'inactiveSession':
        # elif event == 'connectionDropped':
        # elif event == 'connectionEstablished':
        # elif event == 'panic':
        else:
            sendupdate = False

        if sendupdate:
            send(
                {
                    "jsonrpc": "2.0",
                    "method": "Plugin.Stream.Player.Properties",
                    "params": self._properties,
                }
            )

    def on_ws_error(self, ws, error):
        logger.error("Snapcast RPC websocket error (%s)", str(error))

    def on_ws_open(self, ws):
        logger.info("Snapcast RPC websocket opened")
        # send({"jsonrpc": "2.0", "method": "Plugin.Stream.Ready"})

    def on_ws_close(self, ws, close_status_code, close_msg):
        logger.info("Snapcast RPC websocket closed")

    def stop(self):
        self._stop_event.set()
        self.websocket.keep_running = False
        logger.info("Waiting for websocket thread to exit")
        # self.websocket_thread.join()

    def post_json(self, path: str, payload=None, timeout=2.0):
        if payload is None:
            payload = {}

        url = f"{self.BASE_URL}/{path}"

        try:
            r = requests.post(url, json=payload, timeout=timeout)
            logger.debug(f"POST {url} -> {r.status_code} {r.reason} {r.text[:256]!r}")
            return r
        except requests.RequestException as e:
            logger.debug(f"POST {url} failed: {e}")
            return None

    def control(self, cmd):
        if not cmd:
            return
        try:
            req = json.loads(cmd)
            method = req.get("method", "")
            id_ = req.get("id")

            if method.endswith(".Control"):
                action = req["params"].get("command", "")
                logger.debug(f"Control command: {action}")

                if action == "play":
                    self.post_json("player/resume")
                elif action == "pause":
                    self.post_json("player/pause")
                elif action == "playPause":
                    status = ctrl._properties["playbackStatus"]
                    self.post_json(
                        "player/pause" if status == "playing" else "player/resume"
                    )

                elif action == "previous":
                    # prev always worked via POST without body
                    r = self.post_json("player/prev")
                    if not r or r.status_code // 100 != 2:
                        self.get_simple("player/prev")

                elif action == "next":
                    # send an explicit empty JSON body; fallback to GET if needed
                    r = self.post_json("player/next", payload={})
                    if not r or r.status_code // 100 != 2:
                        self.get_simple("player/next")

                elif action == "setPosition":
                    pos = float(req["params"].get("params", {}).get("position", 0))
                    self.post_json("player/seek", {"position": int(pos * 1000)})

                elif action == "seek":
                    pos = float(req["params"].get("params", {}).get("offset", 0))
                    self.post_json(
                        "player/seek", {"position": int(pos * 1000), "relative": True}
                    )

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
        default=defaults["librespot_host"],
        help="The hostname or IP address of the librespot instance.",
    )
    parser.add_argument(
        "--librespot-port",
        type=int,
        default=defaults["librespot_port"],
        help="The port of the librespot web API.",
    )
    parser.add_argument(
        "--stream",
        type=str,
        default=defaults["stream"],
        help="The name of the Snapcast stream to control.",
    )
    parser.add_argument(
        "--snapcast-host",
        type=str,
        default=defaults["snapcast_host"],
        help="The hostname or IP address of the Snapserver.",
    )
    parser.add_argument(
        "--snapcast-port",
        type=int,
        default=defaults["snapcast_port"],
        help="The JSON-RPC port of the Snapserver.",
    )
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging."
    )
    parser.add_argument(
        "-v", "--version", action="version", version=f"%(prog)s {__version__}"
    )

    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()

    ctrl = LibrespotControl(args)

    try:
        for line in sys.stdin:
            ctrl.control(line)
    except KeyboardInterrupt:
        ctrl.stop()
