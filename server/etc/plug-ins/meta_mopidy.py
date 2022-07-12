#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# This file is part of snapcast
# Copyright (C) 2014-2022  Johannes Pohl
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
import os
import sys
import getopt
import logging

__version__ = "@version@"
__git_version__ = "@gitversion@"


identity = "Snapcast"

params = {
    'progname': sys.argv[0],
    # Connection
    'host': None,
    'port': None,
    'password': None,
    'snapcast-host': None,
    'snapcast-port': None,
    'stream': None,
}

defaults = {
    # Connection
    'host': 'localhost',
    'port': 6680,
    'password': None,
    'snapcast-host': 'localhost',
    'snapcast-port': 1780,
    'stream': 'default',
}


def send(json_msg):
    sys.stdout.write(json.dumps(json_msg))
    sys.stdout.write('\n')
    sys.stdout.flush()


class MopidyControl(object):
    """ Mopidy websocket remote control """

    def __init__(self, params):
        self._params = params

        self._metadata = {}
        self._properties = {}
        self._req_id = 0
        self._mopidy_request_map = {}
        self._snapcast_request_map = {}
        self._seek_offset = 0.0

        self.websocket = websocket.WebSocketApp("ws://" + self._params['host'] + ":" + str(self._params['port']) + "/mopidy/ws",
                                                on_message=self.on_ws_message,
                                                on_error=self.on_ws_error,
                                                on_open=self.on_ws_open,
                                                on_close=self.on_ws_close)

        self.websocket_thread = threading.Thread(
            target=self.websocket_loop, args=())
        self.websocket_thread.name = "MopidyControl"
        self.websocket_thread.start()

    def websocket_loop(self):
        logger.info("Started MopidyControl loop")
        while True:
            self.websocket.run_forever()
            sleep(1)
        logger.info("Ending MopidyControl loop")

    def on_ws_message(self, ws, message):
        # TODO: error handling
        logger.debug(f'Snapcast RPC websocket message received: {message}')
        jmsg = json.loads(message)

        # Batch request
        if isinstance(jmsg, list):
            snapcast_req_id = -1
            self._properties = {}
            update_image = False
            for msg in jmsg:
                id = msg['id']
                if id in self._mopidy_request_map:
                    if id in self._snapcast_request_map:
                        snapcast_req_id = self._snapcast_request_map[id]
                        del self._snapcast_request_map[id]
                    request = self._mopidy_request_map[id]
                    del self._mopidy_request_map[id]
                    result = msg['result']
                    repeat = False
                    single = False
                    logger.debug(
                        f'Received response to batch request "{request}": {result}')
                    if request == 'core.playback.get_state':
                        self._properties['playbackStatus'] = str(result)
                    elif request == 'core.tracklist.get_repeat':
                        repeat = result
                    elif request == 'core.tracklist.get_single':
                        single = result
                    elif request == 'core.tracklist.get_random':
                        self._properties['shuffle'] = result
                    elif request == 'core.mixer.get_volume':
                        self._properties['volume'] = result
                    elif request == 'core.playback.get_time_position':
                        self._properties['position'] = float(result) / 1000
                    elif request == 'core.playback.get_current_track':
                        self._metadata = {}
                        if 'uri' in result:
                            self._metadata['url'] = result['uri']
                            update_image = True
                        if 'name' in result:
                            self._metadata['title'] = result['name']
                        if 'artists' in result:
                            for artist in result['artists']:
                                if 'musicbrainz_id' in artist:
                                    self._metadata['musicbrainzArtistId'] = artist['musicbrainz_id']
                                if 'name' in artist:
                                    if not 'artist' in self._metadata:
                                        self._metadata['artist'] = []
                                    self._metadata['artist'].append(
                                        artist['name'])
                                if 'sortname' in artist:
                                    if not 'sortname' in self._metadata:
                                        self._metadata['artistsort'] = []
                                    self._metadata['artistsort'].append(
                                        artist['sortname'])
                        if 'album' in result:
                            album = result['album']
                            if 'musicbrainz_id' in album:
                                self._metadata['musicbrainzAlbumId'] = album['musicbrainz_id']
                            if 'name' in album:
                                self._metadata['album'] = album['name']
                        # if 'composers' in result:
                        #     self._metadata[''] = result['']
                        # if 'performers' in result:
                        #     self._metadata[''] = result['']
                        if 'genre' in result:
                            self._metadata['genre'] = [result['genre']]
                        if 'track_no' in result:
                            self._metadata['trackNumber'] = result['track_no']
                        if 'disc_no' in result:
                            self._metadata['discNumber'] = result['disc_no']
                        if 'date' in result:
                            self._metadata['contentCreated'] = result['date']
                        if 'length' in result:
                            self._metadata['duration'] = float(
                                result['length']) / 1000
                        # if 'bitrate' in result:
                        #     self._metadata[''] = result['bitrate']
                        if 'comment' in result:
                            self._metadata['comment'] = [result['comment']]
                        if 'musicbrainz_id' in result:
                            self._metadata['musicbrainzTrackId'] = result['musicbrainz_id']
                        if 'track_no' in result:
                            self._metadata['trackId'] = str(result['track_no'])
                        # if 'last_modified' in result:
                        #     self._metadata[''] = result['last_modified']

                if repeat and single:
                    self._properties['loopStatus'] = 'track'
                elif repeat:
                    self._properties['loopStatus'] = 'playlist'
                else:
                    self._properties['loopStatus'] = 'none'

            self._properties['canGoNext'] = True
            self._properties['canGoPrevious'] = True
            self._properties['canPlay'] = True
            self._properties['canPause'] = True
            self._properties['canSeek'] = 'duration' in self._metadata
            self._properties['canControl'] = True
            if self._metadata:
                self._properties['metadata'] = self._metadata
            logger.info(f'New properties: {self._properties}')

            if snapcast_req_id >= 0:
                # Update was requested by Snapcast
                send({"jsonrpc": "2.0", "id": snapcast_req_id,
                     "result": self._properties})
            else:
                # Update was triggered by a Mopidy event notification
                send({"jsonrpc": "2.0", "method": "Plugin.Stream.Player.Properties",
                     "params": self._properties})

            if update_image:
                self.send_request('core.library.get_images', {
                    'uris': [self._metadata['url']]})

        # Request
        elif 'id' in jmsg:
            id = jmsg['id']
            if id in self._mopidy_request_map:
                request = self._mopidy_request_map[id]
                del self._mopidy_request_map[id]
                logger.debug(f'Received response to request "{request}"')

                if request == 'core.library.get_images' and 'metadata' in self._properties:
                    # => {'id': 25, 'jsonrpc': '2.0', 'method': 'core.library.get_images', 'params': {'uris': ['local:track:A/ABBA/ABBA%20-%20Voyage%20%282021%29/10%20-%20Ode%20to%20Freedom.ogg']}}
                    # <= {"jsonrpc": "2.0", "id": 25, "result": {"local:track:A/ABBA/ABBA%20-%20Voyage%20%282021%29/10%20-%20Ode%20to%20Freedom.ogg": [{"__model__": "Image", "uri": "/local/f0b20b441175563334f6ad75e76426b7-500x500.jpeg", "width": 500, "height": 500}]}}
                    result = jmsg['result']
                    meta = self._properties['metadata']
                    if 'url' in meta and meta['url'] in result and result[meta['url']]:
                        uri = result[meta['url']][0]['uri']
                        url = str(
                            f"http://{self._params['host']}:{self._params['port']}{uri}")
                        logger.debug(f"Image: {url}")
                        if 'metadata' in self._properties:
                            self._properties['metadata']['artUrl'] = url
                            logger.info(f'New properties: {self._properties}')
                            return send({"jsonrpc": "2.0", "method": "Plugin.Stream.Player.Properties",
                                         "params": self._properties})

                elif request == 'core.playback.get_time_position' and self._seek_offset != 0:
                    pos = int(int(jmsg['result']) + self._seek_offset * 1000)
                    logger.debug(f"Seeking to: {pos}")
                    self.send_request("core.playback.seek", {
                        "time_position": pos})
                    self._seek_offset = 0.0

        # Notification
        else:
            if 'event' in jmsg:
                event = jmsg['event']
                logger.info(f"Event: {event}")
                if event == 'track_playback_started':
                    self.send_requests(["core.playback.get_state", "core.tracklist.get_repeat", "core.tracklist.get_single",
                                        "core.tracklist.get_random", "core.mixer.get_volume", "core.playback.get_current_track", "core.playback.get_time_position"])
                elif event in ['tracklist_changed', 'track_playback_ended']:
                    logger.debug("Nothing to do")
                elif event == 'playback_state_changed' and jmsg["old_state"] == jmsg["new_state"]:
                    logger.debug("Nothing to do")
                elif event == 'volume_changed' and 'volume' in self._properties and jmsg['volume'] == self._properties['volume']:
                    logger.debug("Nothing to do")
                else:
                    self.send_requests(["core.playback.get_state", "core.tracklist.get_repeat", "core.tracklist.get_single",
                                        "core.tracklist.get_random", "core.mixer.get_volume", "core.playback.get_time_position"])

    def on_ws_error(self, ws, error):
        logger.error("Snapcast RPC websocket error")
        logger.error(error)

    def on_ws_open(self, ws):
        logger.info("Snapcast RPC websocket opened")
        send({"jsonrpc": "2.0", "method": "Plugin.Stream.Ready"})

    def on_ws_close(self, ws):
        logger.info("Snapcast RPC websocket closed")

    def send_request(self, method, params=None):
        j = {"id": self._req_id, "jsonrpc": "2.0", "method": str(method)}
        if not params is None:
            j["params"] = params
        logger.debug(f'send_request: {j}')
        result = self._req_id
        self._mopidy_request_map[result] = str(method)
        self._req_id += 1
        self.websocket.send(json.dumps(j))
        return result

    def send_requests(self, methods):
        j = []
        result = self._req_id
        for method in methods:
            j.append({"id": self._req_id, "jsonrpc": "2.0",
                     "method": str(method)})
            self._mopidy_request_map[self._req_id] = str(method)
            self._req_id += 1
        logger.debug(f'send_batch request: {j}')
        self.websocket.send(json.dumps(j))
        return result

    def stop(self):
        self.websocket.keep_running = False
        logger.info("Waiting for websocket thread to exit")
        # self.websocket_thread.join()

    def control(self, cmd):
        try:
            id = None
            request = json.loads(cmd)
            id = request['id']
            [interface, cmd] = request['method'].rsplit('.', 1)
            if interface == 'Plugin.Stream.Player':
                if cmd == 'Control':
                    success = True
                    command = request['params']['command']
                    params = request['params'].get('params', {})
                    logger.debug(
                        f'Control command: {command}, params: {params}')
                    if command == 'next':
                        self.send_request("core.playback.next")
                    elif command == 'previous':
                        self.send_request("core.playback.previous")
                    elif command == 'play':
                        self.send_request("core.playback.play")
                    elif command == 'pause':
                        self.send_request("core.playback.pause")
                    elif command == 'playPause':
                        if self._properties['playbackStatus'] == 'playing':
                            self.send_request("core.playback.pause")
                        else:
                            self.send_request("core.playback.play")
                    elif command == 'stop':
                        self.send_request("core.playback.stop")
                    elif command == 'setPosition':
                        position = float(params['position'])
                        self.send_request("core.playback.seek", {
                                          "time_position": int(position * 1000)})
                    elif command == 'seek':
                        self._seek_offset = float(params['offset'])
                        self.send_request("core.playback.get_time_position")
                elif cmd == 'SetProperty':
                    property = request['params']
                    logger.debug(f'SetProperty: {property}')
                    if 'shuffle' in property:
                        self.send_request("core.tracklist.set_random", {
                                          "value": property['shuffle']})
                    if 'loopStatus' in property:
                        value = property['loopStatus']
                        if value == "playlist":
                            self.send_request(
                                "core.tracklist.set_single", {"value": False})
                            self.send_request(
                                "core.tracklist.set_repeat", {"value": True})
                        elif value == "track":
                            self.send_request(
                                "core.tracklist.set_single", {"value": True})
                            self.send_request(
                                "core.tracklist.set_repeat", {"value": True})
                        elif value == "none":
                            self.send_request(
                                "core.tracklist.set_single", {"value": False})
                            self.send_request(
                                "core.tracklist.set_repeat", {"value": False})
                    if 'volume' in property:
                        self.send_request("core.mixer.set_volume", {
                                          "volume": int(property['volume'])})
                elif cmd == 'GetProperties':
                    req_id = self.send_requests(["core.playback.get_state", "core.tracklist.get_repeat", "core.tracklist.get_single",
                                                 "core.tracklist.get_random", "core.mixer.get_volume", "core.playback.get_current_track", "core.playback.get_time_position"])
                    self._snapcast_request_map[req_id] = id
                    return
                elif cmd == 'GetMetadata':
                    send({"jsonrpc": "2.0", "method": "Plugin.Stream.Log", "params": {
                         "severity": "Info", "message": "Logmessage"}})
                    return send({"jsonrpc": "2.0", "error": {"code": -32601,
                                                             "message": "TODO: GetMetadata not yet implemented"}, "id": id})
                else:
                    return send({"jsonrpc": "2.0", "error": {"code": -32601,
                                                             "message": "Method not found"}, "id": id})
            else:
                return send({"jsonrpc": "2.0", "error": {"code": -32601,
                                                         "message": "Method not found"}, "id": id})
            send({"jsonrpc": "2.0", "result": "ok", "id": id})
        except Exception as e:
            send({"jsonrpc": "2.0", "error": {
                 "code": -32700, "message": "Parse error", "data": str(e)}, "id": id})


def usage(params):
    print("""\
Usage: %(progname)s [OPTION]...

     -h, --host=ADDR        Set the mpd server address
     -p, --port=PORT        Set the TCP port
     --snapcast-host=ADDR   Set the snapcast server address
     --snapcast-port=PORT   Set the snapcast server port
     --stream=ID            Set the stream id

     -d, --debug            Run in debug mode
     -v, --version          meta_mopidy version

Report bugs to https://github.com/badaix/snapcast/issues""" % params)


if __name__ == '__main__':

    log_format_stderr = '%(asctime)s %(module)s %(levelname)s: %(message)s'

    log_level = logging.INFO

    # Parse command line
    try:
        (opts, args) = getopt.getopt(sys.argv[1:], 'h:p:dv',
                                     ['help', 'stream=', 'snapcast-host=', 'snapcast-port=',
                                     'debug', 'host=',
                                      'port=', 'version'])
    except getopt.GetoptError as ex:
        (msg, opt) = ex.args
        print("%s: %s" % (sys.argv[0], msg), file=sys.stderr)
        print(file=sys.stderr)
        usage(params)
        sys.exit(2)

    for (opt, arg) in opts:
        if opt in ['--help']:
            usage(params)
            sys.exit()
        elif opt in ['-d', '--debug']:
            log_level = logging.DEBUG
        elif opt in ['-h', '--host']:
            params['host'] = arg
        elif opt in ['-p', '--port']:
            params['port'] = int(arg)
        elif opt in ['--snapcast-host']:
            params['snapcast-host'] = arg
        elif opt in ['--snapcast-port']:
            params['snapcast-port'] = int(arg)
        elif opt in ['--stream']:
            params['stream'] = arg
        elif opt in ['-v', '--version']:
            v = __version__
            if __git_version__:
                v = __git_version__
            print("snapcast_mopidy version %s" % v)
            sys.exit()

    if len(args) > 2:
        usage(params)
        sys.exit()

    logger = logging.getLogger('meta_mopidy')
    logger.propagate = False
    logger.setLevel(log_level)

    # Log to stderr
    log_handler = logging.StreamHandler()
    log_handler.setFormatter(logging.Formatter(log_format_stderr))

    logger.addHandler(log_handler)

    # Pick up the server address (argv -> environment -> config)
    for arg in args[:2]:
        if arg.isdigit():
            params['port'] = arg
        else:
            params['host'] = arg

    for p in ['host', 'port', 'password']:
        if not params[p]:
            params[p] = defaults[p]

    params['host'] = os.path.expanduser(params['host'])

    logger.debug(f'Parameters: {params}')

    mopidy_ctrl = MopidyControl(params)

    for line in sys.stdin:
        mopidy_ctrl.control(line)
