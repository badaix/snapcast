#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# This file is part of snapcast
# Copyright (C) 2014-2024  Johannes Pohl
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
# - websocket-client

from time import sleep
import websocket
import logging
import threading
import json
import sys
import getopt
import logging

__version__ = "@version@"
__git_version__ = "@gitversion@"


identity = "Snapcast"

params = {
    "progname": sys.argv[0],
    # Connection
    "mopidy-host": None,
    "mopidy-port": None,
    "snapcast-host": None,
    "snapcast-port": None,
    "stream": None,
}

defaults = {
    # Connection
    "mopidy-host": "localhost",
    "mopidy-port": 6680,
    "snapcast-host": "localhost",
    "snapcast-port": 1780,
    "stream": "default",
}


def send(json_msg):
    sys.stdout.write(json.dumps(json_msg))
    sys.stdout.write("\n")
    sys.stdout.flush()


class MopidyControl(object):
    """Mopidy websocket remote control"""

    def __init__(self, params):
        self._params = params

        self._metadata = {}
        self._properties = {}
        self._req_id = 0
        self._mopidy_request_map = {}
        self._seek_offset = 0.0

        wsversion = websocket.__version__.split(".")
        if int(wsversion[0]) == 0 and int(wsversion[1]) < 58:
            logger.error(
                f"websocket-client version 0.58.0 or higher required, installed: {websocket.__version__}, exiting."
            )
            exit()

        self.websocket = websocket.WebSocketApp(
            url=f"ws://{self._params['mopidy-host']}:{self._params['mopidy-port']}/mopidy/ws",
            on_message=self.on_ws_message,
            on_error=self.on_ws_error,
            on_open=self.on_ws_open,
            on_close=self.on_ws_close
        )

        self.websocket_thread = threading.Thread(
            target=self.websocket_loop, args=())
        self.websocket_thread.name = "MopidyControl"
        self.websocket_thread.start()

    def websocket_loop(self):
        logger.info("Started MopidyControl loop")
        while True:
            try:
                self.websocket.run_forever()
                sleep(2)
            except Exception as e:
                logger.info(f"Exception: {str(e)}")
                self.websocket.close()
        # logger.info("Ending MopidyControl loop")

    def extractImageUrl(self, track_uri, jmsg):
        url = None
        if jmsg and track_uri in jmsg and jmsg[track_uri]:
            url = jmsg[track_uri][0]["uri"]
            if url.find("://") == -1:
                url = str(
                    f"http://{self._params['mopidy-host']}:{self._params['mopidy-port']}{url}"
                )
            logger.debug(f"Image: {url}")
        return url

    def onGetImageResponse(self, result):
        if "metadata" in self._properties:
            # => {'id': 25, 'jsonrpc': '2.0', 'method': 'core.library.get_images', 'params': {'uris': ['local:track:A/ABBA/ABBA%20-%20Voyage%20%282021%29/10%20-%20Ode%20to%20Freedom.ogg']}}
            # <= {"jsonrpc": "2.0", "id": 25, "result": {"local:track:A/ABBA/ABBA%20-%20Voyage%20%282021%29/10%20-%20Ode%20to%20Freedom.ogg": [{"__model__": "Image", "uri": "/local/f0b20b441175563334f6ad75e76426b7-500x500.jpeg", "width": 500, "height": 500}]}}
            meta = self._properties["metadata"]
            if "url" in meta:
                url = self.extractImageUrl(meta["url"], result)
                if not url is None:
                    self._properties["metadata"]["artUrl"] = url
                    logger.info(f"New properties: {self._properties}")
                    return send(
                        {
                            "jsonrpc": "2.0",
                            "method": "Plugin.Stream.Player.Properties",
                            "params": self._properties,
                        }
                    )

    def getMetaData(self, track):
        metadata = {}
        if track is None:
            return metadata
        if "uri" in track:
            metadata["url"] = track["uri"]
        if "name" in track:
            metadata["title"] = track["name"]
        if "artists" in track:
            for artist in track["artists"]:
                if "musicbrainz_id" in artist:
                    metadata["musicbrainzArtistId"] = artist["musicbrainz_id"]
                if "name" in artist:
                    if not "artist" in metadata:
                        metadata["artist"] = []
                    metadata["artist"].append(artist["name"])
                if "sortname" in artist:
                    if not "sortname" in metadata:
                        metadata["artistsort"] = []
                    metadata["artistsort"].append(artist["sortname"])
        if "album" in track:
            album = track["album"]
            if "musicbrainz_id" in album:
                self._metadata["musicbrainzAlbumId"] = album["musicbrainz_id"]
            if "name" in album:
                self._metadata["album"] = album["name"]
        if "genre" in track:
            metadata["genre"] = [track["genre"]]
        if "track_no" in track:
            metadata["trackNumber"] = track["track_no"]
            metadata["trackId"] = str(track["track_no"])
        if "disc_no" in track:
            metadata["discNumber"] = track["disc_no"]
        if "date" in track:
            metadata["contentCreated"] = track["date"]
        if "length" in track:
            metadata["duration"] = float(track["length"]) / 1000
        if "comment" in track:
            metadata["comment"] = [track["comment"]]
        if "musicbrainz_id" in track:
            metadata["musicbrainzTrackId"] = track["musicbrainz_id"]
        # Not supported:
        # if 'composers' in result:
        # if 'performers' in result:
        # if 'bitrate' in result:
        # if 'last_modified' in result:
        return metadata

    def getProperties(self, req_res):
        properties = {}
        repeat = False
        single = False
        for rr in req_res:
            request = rr[0]
            result = rr[1]
            logger.debug(f"getProperties request: {request}, result: {result}")
            if request == "core.playback.get_stream_title":
                if not result is None and not self._metadata is None:
                    self._metadata["title"] = result
            elif request == "core.playback.get_state":
                properties["playbackStatus"] = str(result)
            elif request == "core.tracklist.get_repeat":
                repeat = result
            elif request == "core.tracklist.get_single":
                single = result
            elif request == "core.tracklist.get_random":
                properties["shuffle"] = result
            elif request == "core.mixer.get_volume":
                properties["volume"] = result
            elif request == "core.mixer.get_mute":
                properties["mute"] = result
            elif request == "core.playback.get_time_position":
                properties["position"] = float(result) / 1000
            elif request == "core.playback.get_current_track":
                self._metadata = self.getMetaData(result)
            elif request == "core.library.get_images":
                metadata = self._metadata
                if not metadata is None and "url" in metadata:
                    url = self.extractImageUrl(metadata["url"], result)
                    if not url is None:
                        self._metadata["artUrl"] = url

        if repeat and single:
            properties["loopStatus"] = "track"
        elif repeat:
            properties["loopStatus"] = "playlist"
        else:
            properties["loopStatus"] = "none"

        properties["canGoNext"] = True
        properties["canGoPrevious"] = True
        properties["canPlay"] = True
        properties["canPause"] = True
        properties["canSeek"] = "duration" in self._metadata
        properties["canControl"] = True
        if self._metadata:
            properties["metadata"] = self._metadata
        return properties

    def onGetTrackResponse(self, req_id, track):
        self._metadata = self.getMetaData(track)
        batch_req = [
            ("core.playback.get_stream_title", None),
            ("core.playback.get_state", None),
            ("core.tracklist.get_repeat", None),
            ("core.tracklist.get_single", None),
            ("core.tracklist.get_random", None),
            ("core.mixer.get_volume", None),
            ("core.playback.get_time_position", None),
        ]
        if "url" in self._metadata:
            batch_req.append(
                ("core.library.get_images", {"uris": [self._metadata["url"]]})
            )
        self.send_batch_request(
            batch_req,
            lambda req_res: self.onSnapcastPropertiesResponse(req_id, req_res),
        )

    def onSnapcastPropertiesResponse(self, req_id, req_res):
        logger.debug(f"onSnapcastPropertiesRequest id: {req_id}")
        self._properties = self.getProperties(req_res)
        logger.info(f"New properties: {self._properties}")
        send({"jsonrpc": "2.0", "id": req_id, "result": self._properties})

    def onPropertiesResponse(self, req_res):
        self._properties = self.getProperties(req_res)
        logger.info(f"New properties: {self._properties}")
        send(
            {
                "jsonrpc": "2.0",
                "method": "Plugin.Stream.Player.Properties",
                "params": self._properties,
            }
        )

    def onGetTimePositionResponse(self, result):
        if self._seek_offset != 0:
            pos = int(int(result) + self._seek_offset * 1000)
            logger.debug(f"Seeking to: {pos}")
            self.send_request("core.playback.seek", {"time_position": pos})
            self._seek_offset = 0.0

    def on_ws_message(self, ws, message):
        # TODO: error handling
        logger.debug(f"Snapcast RPC websocket message received: {message}")
        jmsg = json.loads(message)

        # Batch request
        if isinstance(jmsg, list):
            self._properties = {}
            req_res = []
            callback = None
            for msg in jmsg:
                id = msg["id"]
                if id in self._mopidy_request_map:
                    request = self._mopidy_request_map[id]
                    del self._mopidy_request_map[id]
                    req_res.append((request[0], msg["result"]))
                    if not request[1] is None:
                        callback = request[1]
            if not callback is None:
                callback(req_res)

        # Request
        elif "id" in jmsg:
            id = jmsg["id"]
            if id in self._mopidy_request_map:
                request = self._mopidy_request_map[id]
                del self._mopidy_request_map[id]
                logger.debug(f'Received response to request "{request[0]}"')
                callback = request[1]
                if not callback is None:
                    callback(jmsg["result"])

        # Notification
        else:
            if "event" in jmsg:
                event = jmsg["event"]
                logger.info(f"Event: {event}")
                if event == "track_playback_started":
                    self._metadata = self.getMetaData(
                        jmsg["tl_track"]["track"])
                    logger.debug(f"Meta: {self._metadata}")
                    batch_req = [
                        ("core.playback.get_stream_title", None),
                        ("core.playback.get_state", None),
                        ("core.tracklist.get_repeat", None),
                        ("core.tracklist.get_single", None),
                        ("core.tracklist.get_random", None),
                        ("core.mixer.get_volume", None),
                        ("core.mixer.get_mute", None),
                        ("core.playback.get_time_position", None),
                    ]
                    if "url" in self._metadata:
                        batch_req.append(
                            (
                                "core.library.get_images",
                                {"uris": [self._metadata["url"]]},
                            )
                        )
                    self.send_batch_request(
                        batch_req, self.onPropertiesResponse)
                elif event in ["tracklist_changed", "track_playback_ended"]:
                    logger.debug("Nothing to do")
                elif (
                    event == "playback_state_changed"
                    and jmsg["old_state"] == jmsg["new_state"]
                ):
                    logger.debug("Nothing to do")
                elif (
                    event == "volume_changed"
                    and "volume" in self._properties
                    and jmsg["volume"] == self._properties["volume"]
                ):
                    logger.debug("Nothing to do")
                else:
                    self.send_batch_request(
                        [
                            ("core.playback.get_stream_title", None),
                            ("core.playback.get_state", None),
                            ("core.tracklist.get_repeat", None),
                            ("core.tracklist.get_single", None),
                            ("core.tracklist.get_random", None),
                            ("core.mixer.get_volume", None),
                            ("core.mixer.get_mute", None),
                            ("core.playback.get_time_position", None),
                        ],
                        self.onPropertiesResponse,
                    )

    def on_ws_error(self, ws, error):
        logger.error("Snapcast RPC websocket error")
        logger.error(error)

    def on_ws_open(self, ws):
        logger.info("Snapcast RPC websocket opened")
        send({"jsonrpc": "2.0", "method": "Plugin.Stream.Ready"})

    def on_ws_close(self, ws, close_status_code, close_msg):
        logger.info("Snapcast RPC websocket closed")

    def send_request(self, method, params=None, callback=None):
        j = {"id": self._req_id, "jsonrpc": "2.0", "method": str(method)}
        if not params is None:
            j["params"] = params
        logger.debug(f"send_request: {j}")
        result = self._req_id
        self._mopidy_request_map[result] = (str(method), callback)
        self._req_id += 1
        self.websocket.send(json.dumps(j))
        return result

    def send_batch_request(self, methods_params, callback=None):
        batch = []
        result = self._req_id
        for method_param in methods_params:
            method = str(method_param[0])
            params = method_param[1]
            j = {"id": self._req_id, "jsonrpc": "2.0", "method": method}
            if not params is None:
                j["params"] = params
            batch.append(j)
            self._mopidy_request_map[self._req_id] = (method, callback)
            self._req_id += 1
        logger.debug(f"send_batch_request: {batch}")
        self.websocket.send(json.dumps(batch))
        return result

    def stop(self):
        self.websocket.keep_running = False
        logger.info("Waiting for websocket thread to exit")
        # self.websocket_thread.join()

    def control(self, cmd):
        try:
            id = None
            request = json.loads(cmd)
            id = request["id"]
            [interface, cmd] = request["method"].rsplit(".", 1)
            if interface == "Plugin.Stream.Player":
                if cmd == "Control":
                    success = True
                    command = request["params"]["command"]
                    params = request["params"].get("params", {})
                    logger.debug(
                        f"Control command: {command}, params: {params}")
                    if command == "next":
                        self.send_request("core.playback.next")
                    elif command == "previous":
                        self.send_request("core.playback.previous")
                    elif command == "play":
                        self.send_request("core.playback.play")
                    elif command == "pause":
                        self.send_request("core.playback.pause")
                    elif command == "playPause":
                        if self._properties["playbackStatus"] == "playing":
                            self.send_request("core.playback.pause")
                        else:
                            self.send_request("core.playback.play")
                    elif command == "stop":
                        self.send_request("core.playback.stop")
                    elif command == "setPosition":
                        position = float(params["position"])
                        self.send_request(
                            "core.playback.seek",
                            {"time_position": int(position * 1000)},
                        )
                    elif command == "seek":
                        self._seek_offset = float(params["offset"])
                        self.send_request(
                            "core.playback.get_time_position",
                            None,
                            self.onGetTimePositionResponse,
                        )
                elif cmd == "SetProperty":
                    property = request["params"]
                    logger.debug(f"SetProperty: {property}")
                    if "shuffle" in property:
                        self.send_request(
                            "core.tracklist.set_random", {
                                "value": property["shuffle"]}
                        )
                    if "loopStatus" in property:
                        value = property["loopStatus"]
                        if value == "playlist":
                            self.send_request(
                                "core.tracklist.set_single", {"value": False}
                            )
                            self.send_request(
                                "core.tracklist.set_repeat", {"value": True}
                            )
                        elif value == "track":
                            self.send_request(
                                "core.tracklist.set_single", {"value": True}
                            )
                            self.send_request(
                                "core.tracklist.set_repeat", {"value": True}
                            )
                        elif value == "none":
                            self.send_request(
                                "core.tracklist.set_single", {"value": False}
                            )
                            self.send_request(
                                "core.tracklist.set_repeat", {"value": False}
                            )
                    if "volume" in property:
                        self.send_request(
                            "core.mixer.set_volume", {
                                "volume": int(property["volume"])}
                        )
                    if "mute" in property:
                        self.send_request(
                            "core.mixer.set_mute", {"mute": property["mute"]}
                        )
                elif cmd == "GetProperties":
                    self.send_request(
                        "core.playback.get_current_track",
                        None,
                        lambda track: self.onGetTrackResponse(id, track),
                    )
                    return
                elif cmd == "GetMetadata":
                    send(
                        {
                            "jsonrpc": "2.0",
                            "method": "Plugin.Stream.Log",
                            "params": {"severity": "Info", "message": "Logmessage"},
                        }
                    )
                    return send(
                        {
                            "jsonrpc": "2.0",
                            "error": {
                                "code": -32601,
                                "message": "TODO: GetMetadata not yet implemented",
                            },
                            "id": id,
                        }
                    )
                else:
                    return send(
                        {
                            "jsonrpc": "2.0",
                            "error": {"code": -32601, "message": "Method not found"},
                            "id": id,
                        }
                    )
            else:
                return send(
                    {
                        "jsonrpc": "2.0",
                        "error": {"code": -32601, "message": "Method not found"},
                        "id": id,
                    }
                )
            send({"jsonrpc": "2.0", "result": "ok", "id": id})
        except Exception as e:
            send(
                {
                    "jsonrpc": "2.0",
                    "error": {"code": -32700, "message": "Parse error", "data": str(e)},
                    "id": id,
                }
            )


def usage(params):
    print(
        """\
Usage: %(progname)s [OPTION]...

     --mopidy-host=ADDR     Set the mopidy server address
     --mopidy-port=PORT     Set the mopidy server port
     --snapcast-host=ADDR   Set the snapcast server address
     --snapcast-port=PORT   Set the snapcast server port
     --stream=ID            Set the stream id

     -h, --help             Show this help message
     -d, --debug            Run in debug mode
     -v, --version          meta_mopidy version

Report bugs to https://github.com/badaix/snapcast/issues"""
        % params
    )


if __name__ == "__main__":

    log_format_stderr = "%(asctime)s %(module)s %(levelname)s: %(message)s"

    log_level = logging.INFO

    # Parse command line
    try:
        (opts, args) = getopt.getopt(
            sys.argv[1:],
            "hdv",
            [
                "help",
                "mopidy-host=",
                "mopidy-port=",
                "snapcast-host=",
                "snapcast-port=",
                "stream=",
                "debug",
                "version",
            ],
        )

    except getopt.GetoptError as ex:
        (msg, opt) = ex.args
        print("%s: %s" % (sys.argv[0], msg), file=sys.stderr)
        print(file=sys.stderr)
        usage(params)
        sys.exit(2)

    for opt, arg in opts:
        if opt in ["-h", "--help"]:
            usage(params)
            sys.exit()
        elif opt in ["--mopidy-host"]:
            params["mopidy-host"] = arg
        elif opt in ["--mopidy-port"]:
            params["mopidy-port"] = int(arg)
        elif opt in ["--snapcast-host"]:
            params["snapcast-host"] = arg
        elif opt in ["--snapcast-port"]:
            params["snapcast-port"] = int(arg)
        elif opt in ["--stream"]:
            params["stream"] = arg
        elif opt in ["-d", "--debug"]:
            log_level = logging.DEBUG
        elif opt in ["-v", "--version"]:
            v = __version__
            if __git_version__:
                v = __git_version__
            print("meta_mopidy version %s" % v)
            sys.exit()

    if len(args) > 2:
        usage(params)
        sys.exit()

    logger = logging.getLogger("meta_mopidy")
    logger.propagate = False
    logger.setLevel(log_level)

    # Log to stderr
    log_handler = logging.StreamHandler()
    log_handler.setFormatter(logging.Formatter(log_format_stderr))

    logger.addHandler(log_handler)

    for p in ["mopidy-host", "mopidy-port", "snapcast-host", "snapcast-port", "stream"]:
        if not params[p]:
            params[p] = defaults[p]

    logger.debug(f"Parameters: {params}")

    mopidy_ctrl = MopidyControl(params)

    for line in sys.stdin:
        mopidy_ctrl.control(line)
