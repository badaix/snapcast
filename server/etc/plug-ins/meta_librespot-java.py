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
# - requests

from time import sleep
import websocket
import logging
import threading
import json
import sys
import getopt
import logging
import requests

__version__ = "@version@"
__git_version__ = "@gitversion@"


identity = "Snapcast"

params = {
    'progname': sys.argv[0],
    # Connection
    'librespot-host': None,
    'librespot-port': None,
    'snapcast-host': None,
    'snapcast-port': None,
    'stream': None,
}

defaults = {
    # Connection
    'librespot-host': 'localhost',
    'librespot-port': 24879,
    'snapcast-host': 'localhost',
    'snapcast-port': 1780,
    'stream': 'default',
}


def send(json_msg):
    sys.stdout.write(json.dumps(json_msg))
    sys.stdout.write('\n')
    sys.stdout.flush()


class LibrespotControl(object):
    """ Librespot websocket remote control """

    def __init__(self, params):
        self._params = params

        self._metadata = {}
        self._properties = {}
        self._properties['playbackStatus'] = 'playing'
        self._properties['loopStatus'] = 'none'
        self._properties['shuffle'] = False
        self._properties['volume'] = 100
        self._properties['mute'] = False
        self._properties['rate'] = 1.0
        self._properties['position'] = 0

        self._properties['canGoNext'] = True
        self._properties['canGoPrevious'] = True
        self._properties['canPlay'] = True
        self._properties['canPause'] = True
        self._properties['canSeek'] = True
        self._properties['canControl'] = True

        self._req_id = 0
        self._librespot_request_map = {}
        self._seek_offset = 0.0

        self.websocket = websocket.WebSocketApp(
            url=f"ws://{self._params['librespot-host']}:{self._params['librespot-port']}/events",
            on_message=self.on_ws_message,
            on_error=self.on_ws_error,
            on_open=self.on_ws_open,
            on_close=self.on_ws_close
        )

        self.websocket_thread = threading.Thread(
            target=self.websocket_loop, args=())
        self.websocket_thread.name = "LibrespotControl"
        self.websocket_thread.start()

    def websocket_loop(self):
        logger.info("Started LibrespotControl loop")
        while True:
            try:
                self.websocket.run_forever()
                sleep(2)
            except Exception as e:
                logger.info(f"Exception: {str(e)}")
                self.websocket.close()
        # logger.info("Ending LibrespotControl loop")

    def getMetaData(self, data):
        data = json.loads(data)
        logger.debug(f'getMetaData: {data}')
        metadata = {}
        if data is None:
            return metadata
        if 'trackTime' in data:
            self._properties['position'] = max(
                0.0, float(data['trackTime']) / 1000.0)
        if 'current' in data:
            metadata['url'] = data['current']
        if 'track' in data:
            track = data['track']
            if 'name' in track:
                metadata['title'] = track['name']
            if 'duration' in track:
                metadata['duration'] = float(
                    track['duration']) / 1000
            if 'artist' in track:
                metadata['artist'] = []
                artists = track['artist']
                for artist in artists:
                    metadata['artist'].append(
                        artist['name'])

            if 'album' in track:
                album = track['album']
                if 'name' in album:
                    metadata['album'] = album['name']
                if 'date' in album:
                    date = album['date']
                    metadata['contentCreated'] = f"{date['year']:04d}-{date['month']:02d}-{date['day']:02d}"
                if 'coverGroup' in album and 'image' in album['coverGroup']:
                    images = album['coverGroup']['image']
                    for image in images:
                        if 'size' in image and image['size'] == 'DEFAULT':
                            metadata['artUrl'] = f"http://i.scdn.co/image/{str(image['fileId']).lower()}"

            if 'discNumber' in track:
                metadata['discNumber'] = track['discNumber']
            if 'number' in track:
                metadata['trackNumber'] = track['number']
            metadata['trackId'] = track['gid']

        return metadata

    def updateProperties(self):
        res = self.send_request('player/current')
        if res.status_code / 100 == 2:
            self._metadata = self.getMetaData(res.text)
            self._properties['metadata'] = self._metadata

    def on_ws_message(self, ws, message):
        # TODO: error handling
        logger.debug(f'Snapcast RPC websocket message received: {message}')
        jmsg = json.loads(message)

        # Batch request
        if isinstance(jmsg, list):
            return

        # Request
        elif 'id' in jmsg:
            return

        # Notification
        else:
            if 'event' in jmsg:
                event = jmsg['event']
                logger.info(f"Event: {event}, msg: {jmsg}")
                # elif event == 'contextChanged':
                # elif event == 'trackChanged':
                sendupdate = True
                if event == 'playbackEnded':
                    self._properties['playbackStatus'] = 'stopped'
                elif event == 'playbackPaused':
                    self._properties['playbackStatus'] = 'paused'
                elif event == 'playbackResumed':
                    self._properties['playbackStatus'] = 'playing'
                elif event == 'volumeChanged':
                    self._properties['volume'] = int(jmsg['value'] * 100.0)
                elif event == 'trackSeeked':
                    self._properties['position'] = float(
                        jmsg['trackTime']) / 1000.0
                elif event == 'metadataAvailable':
                    self.updateProperties()
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
                    send({"jsonrpc": "2.0", "method": "Plugin.Stream.Player.Properties",
                         "params": self._properties})

    def on_ws_error(self, ws, error):
        logger.error("Snapcast RPC websocket error")
        logger.error(error)

    def on_ws_open(self, ws):
        logger.info("Snapcast RPC websocket opened")
        send({"jsonrpc": "2.0", "method": "Plugin.Stream.Ready"})

    def on_ws_close(self, ws, close_status_code, close_msg):
        logger.info("Snapcast RPC websocket closed")

    def send_request(self, method, params=None):
        request = f"http://{self._params['librespot-host']}:{self._params['librespot-port']}/{method}"
        logger.debug(f"Sending request: {request}")
        r = requests.post(request, data=params)
        logger.info(
            f"Status: {r.status_code}, reason: {r.reason}, text: {r.text}")
        return r

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
                    command = request['params']['command']
                    params = request['params'].get('params', {})
                    logger.debug(
                        f'Control command: {command}, params: {params}')
                    if command == 'next':
                        self.send_request("player/next")
                    elif command == 'previous':
                        self.send_request("player/prev")
                    elif command == 'play':
                        self.send_request("player/resume")
                        self._properties['playbackStatus'] = 'playing'
                    elif command == 'pause':
                        self.send_request("player/pause")
                        self._properties['playbackStatus'] = 'paused'
                    elif command == 'playPause':
                        # self.send_request("player/play-pause")
                        if self._properties['playbackStatus'] == 'playing':
                            self.send_request("player/pause")
                            self._properties['playbackStatus'] = 'paused'
                        else:
                            self.send_request("player/resume")
                            self._properties['playbackStatus'] = 'playing'
                    elif command == 'stop':
                        self.send_request("player/pause")
                        self._properties['playbackStatus'] = 'paused'
                    elif command == 'setPosition':
                        position = float(params['position'])
                        self._properties['position'] = position
                        self.send_request(
                            "player/seek", {"pos": int(position * 1000)})
                    elif command == 'seek':
                        self._seek_offset = float(params['offset'])
                        logger.debug("todo: not implemented")
                        # self.send_request(
                        #     "core.playback.get_time_position", None, self.onGetTimePositionResponse)
                    # self.updateProperties()
                elif cmd == 'SetProperty':
                    property = request['params']
                    logger.debug(f'SetProperty: {property}')
                    if 'shuffle' in property:
                        self._properties['shuffle'] = not self._properties['shuffle']
                        self.send_request("player/shuffle")
                    if 'loopStatus' in property:
                        value = property['loopStatus']
                        self._properties['loopStatus'] = value
                        if value == "playlist":
                            self.send_request(
                                "player/repeat", {"val": "context"})
                        elif value == "track":
                            self.send_request(
                                "player/repeat", {"val": "track"})
                        elif value == "none":
                            self.send_request("player/repeat", {"val": "none"})
                    if 'volume' in property:
                        self.send_request(
                            "player/set-volume", {"volume": int(float(property['volume']) / 100.0 * 65536.0)})
                    if 'mute' in property:
                        logger.debug("todo: not implemented")
                        # self._properties['mute'] = False
                        # self.send_request("core.mixer.set_mute", {
                        #                   "mute": property['mute']})
                    # self.updateProperties()
                elif cmd == 'GetProperties':
                    res = self.send_request('player/current')
                    # if res.status_code / 100 == 2:
                    self._metadata = self.getMetaData(res.text)
                    self._properties['metadata'] = self._metadata
                    send({"jsonrpc": "2.0", "id": id,
                          "result": self._properties})
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

     --librespot-host=ADDR     Set the librespot server address
     --librespot-port=PORT     Set the librespot server port
     --snapcast-host=ADDR      Set the snapcast server address
     --snapcast-port=PORT      Set the snapcast server port
     --stream=ID               Set the stream id

     -h, --help             Show this help message
     -d, --debug            Run in debug mode
     -v, --version          meta_librespot version

Report bugs to https://github.com/badaix/snapcast/issues""" % params)


if __name__ == '__main__':

    log_format_stderr = '%(asctime)s %(module)s %(levelname)s: %(message)s'

    log_level = logging.INFO

    # Parse command line
    try:
        (opts, args) = getopt.getopt(sys.argv[1:], 'hdv',
                                     ['help', 'librespot-host=', 'librespot-port=', 'snapcast-host=', 'snapcast-port=', 'stream=', 'debug', 'version'])

    except getopt.GetoptError as ex:
        (msg, opt) = ex.args
        print("%s: %s" % (sys.argv[0], msg), file=sys.stderr)
        print(file=sys.stderr)
        usage(params)
        sys.exit(2)

    for (opt, arg) in opts:
        if opt in ['-h', '--help']:
            usage(params)
            sys.exit()
        elif opt in ['--librespot-host']:
            params['librespot-host'] = arg
        elif opt in ['--librespot-port']:
            params['librespot-port'] = int(arg)
        elif opt in ['--snapcast-host']:
            params['snapcast-host'] = arg
        elif opt in ['--snapcast-port']:
            params['snapcast-port'] = int(arg)
        elif opt in ['--stream']:
            params['stream'] = arg
        elif opt in ['-d', '--debug']:
            log_level = logging.DEBUG
        elif opt in ['-v', '--version']:
            v = __version__
            if __git_version__:
                v = __git_version__
            print("meta_librespot version %s" % v)
            sys.exit()

    if len(args) > 2:
        usage(params)
        sys.exit()

    logger = logging.getLogger('meta_librespot')
    logger.propagate = False
    logger.setLevel(log_level)

    # Log to stderr
    log_handler = logging.StreamHandler()
    log_handler.setFormatter(logging.Formatter(log_format_stderr))

    logger.addHandler(log_handler)

    for p in ['librespot-host', 'librespot-port', 'snapcast-host', 'snapcast-port', 'stream']:
        if not params[p]:
            params[p] = defaults[p]

    logger.debug(f'Parameters: {params}')

    librespot_ctrl = LibrespotControl(params)

    for line in sys.stdin:
        librespot_ctrl.control(line)
