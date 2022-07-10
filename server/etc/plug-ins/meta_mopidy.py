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

from cmath import log
from time import sleep
import websocket
import logging
import threading
import json
import os
import sys
import getopt
import time
import logging
import gettext

__version__ = "@version@"
__git_version__ = "@gitversion@"


_ = gettext.gettext

identity = "Snapcast"

params = {
    'progname': sys.argv[0],
    # Connection
    'host': None,
    'port': None,
    'password': None,
}

defaults = {
    # Connection
    'host': 'localhost',
    'port': 6680,
    'password': None,
}

notification = None


def send(json_msg):
    print(json.dumps(json_msg))
    sys.stdout.flush()


class MopidyControl(object):
    """ Mopidy websocket remote control """

    def __init__(self, params):
        self._params = params

        self._metadata = {}
        self._properties = {}
        self._req_id = 0
        self._stream_id = ''
        self._request_map = {}

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

    def __get_stream_id_from_server_status(self, status, client_id):
        try:
            for group in status['server']['groups']:
                for client in group['clients']:
                    if client['id'] == client_id:
                        return group['stream_id']
            for group in status['server']['groups']:
                for client in group['clients']:
                    if client['name'] == client_id:
                        return group['stream_id']
        except:
            logger.error('Failed to parse server status')
        logger.error(f'Failed to get stream id for client {client_id}')
        return None

    def __update_metadata(self, meta):
        try:
            if meta is None:
                meta = {}
            logger.info(f'Meta: "{meta}"')

            self._metadata = {}
            self._metadata['xesam:artist'] = ['Unknown Artist']
            self._metadata['xesam:title'] = 'Unknown Title'

            for key, value in meta.items():
                if key in tag_mapping:
                    try:
                        self._metadata[tag_mapping[key][0]
                                       ] = tag_mapping[key][1](value)
                    except KeyError:
                        logger.warning(f'tag "{key}" not supported')
                    except (ValueError, TypeError):
                        logger.warning(
                            f"Can't cast value {value} to {tag_mapping[key][1]}")

            if not 'mpris:artUrl' in self._metadata:
                self._metadata['mpris:artUrl'] = f'http://{self._params["host"]}:{self._params["port"]}/snapcast-512.png'

            logger.debug(f'mpris meta: {self._metadata}')

            self.notify_about_track(self._metadata)
        except Exception as e:
            logger.error(f'Error in update_metadata: {str(e)}')

    def __update_properties(self, props):
        try:
            if props is None:
                props = {}
            logger.debug(f'Properties: "{props}"')
            # store the last receive time stamp for better position estimation
            if 'position' in props:
                props['_received'] = time.time()
            # ignore "internal" properties, starting with "_"
            changed_properties = {}
            for key, value in props.items():
                if key.startswith('_'):
                    continue
                if not key in self._properties:
                    changed_properties[key] = [None, value]
                elif value != self._properties[key]:
                    changed_properties[key] = [self._properties[key], value]
            for key, value in self._properties.items():
                if key.startswith('_'):
                    continue
                if not key in props:
                    changed_properties[key] = [value, None]
            self._properties = props
            logger.info(f'Changed properties: "{changed_properties}"')
            # for key, value in changed_properties.items():
            #     if key in property_mapping:
            #         self._dbus_service.update_property(
            #             'org.mpris.MediaPlayer2.Player', property_mapping[key])
            if 'metadata' in changed_properties:
                self.__update_metadata(props.get('metadata', None))
        except Exception as e:
            logger.error(f'Error in update_properties: {str(e)}')

    def on_ws_message(self, ws, message):
        # TODO: error handling
        logger.info(f'Snapcast RPC websocket message received: {message}')
        jmsg = json.loads(message)
        if isinstance(jmsg, list):
            logger.info('List')
            self._properties = {}
            self._metadata = {}
            for msg in jmsg:
                id = msg['id']
                if id in self._request_map:
                    request = self._request_map[id]
                    del self._request_map[id]
                    logger.info(f'Received response to {request}')
                    result = msg['result']
                    repeat = False
                    single = False
                    logger.info(f'Result for {request}: {result}')
                    if request == 'core.playback.get_state':
                        self._properties['playbackStatus'] = str(result)
                    if request == 'core.tracklist.get_repeat':
                        repeat = result
                    if request == 'core.tracklist.get_single':
                        single = result
                    if request == 'core.tracklist.get_random':
                        self._properties['shuffle'] = result
                    if request == 'core.mixer.get_volume':
                        self._properties['volume'] = result
                    if request == 'core.playback.get_current_track':
                        if 'uri' in result:
                            self._metadata['url'] = result['uri']
                        if 'name' in result:
                            self._metadata['name'] = result['name']
                        if 'artists' in result:
                            for artist in result['artists']:
                                if 'musicbrainz_id' in artist:
                                    self._metadata['musicbrainz_artistid'] = artist['musicbrainz_id']
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
                                self._metadata['musicbrainz_albumid'] = album['musicbrainz_id']
                            if 'name' in album:
                                self._metadata['album'] = album['name']
                        # if 'composers' in result:
                        #     self._metadata[''] = result['']
                        # if 'performers' in result:
                        #     self._metadata[''] = result['']
                        if 'genre' in result:
                            self._metadata['genre'] = result['genre']
                        if 'track_no' in result:
                            self._metadata['trackNumber'] = result['track_no']
                        if 'disc_no' in result:
                            self._metadata['discNumber'] = result['disc_no']
                        if 'date' in result:
                            self._metadata['contentCreated'] = result['date']
                        if 'length' in result:
                            self._metadata['duration'] = float(result['length']) / 1000
                        # if 'bitrate' in result:
                        #     self._metadata[''] = result['bitrate']
                        if 'comment' in result:
                            self._metadata['comment'] = result['comment']
                        if 'musicbrainz_id' in result:
                            self._metadata['musicbrainzTrackId'] = result['musicbrainz_id']
                        # if 'last_modified' in result:
                        #     self._metadata[''] = result['last_modified']
                    if request == 'core.playback.get_time_position':
                        self._properties['position'] = float(result) / 1000
                        logger.info(f'Result for {request}: {result}')
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
            self._properties['metadata'] = self._metadata
            logger.info(f'Result: {self._properties}')

            if 'url' in self._metadata:
                self.send_request('core.library.get_images', {'uris': [self._metadata['url']]})
            # return send({"jsonrpc": "2.0", "id": id, "result": self._properties})

        if 'id' in jmsg:
            id = jmsg['id']
            if id in self._request_map:
                request = self._request_map[id]
                del self._request_map[id]
                logger.info(f'Received response to {request}')
                if request == 'core.library.get_images':
                    result = dict(jmsg['result'])
                    for _, value in result.items():
                        if value and 'uri' in value[0]:
                            url = str(f"http://{self._params['host']}:{self._params['port']}{value[0]['uri']}") 
                            logger.info(f"Image: {url}")

                if request == 'Server.GetStatus':
                    self._stream_id = self.__get_stream_id_from_server_status(
                        jmsg['result'], self._params['client'])
                    logger.info(f'Stream id: {self._stream_id}')
                    for stream in jmsg['result']['server']['streams']:
                        if stream['id'] == self._stream_id:
                            if 'properties' in stream:
                                self.__update_properties(stream['properties'])
                            break
        elif jmsg['method'] == "Server.OnUpdate":
            self._stream_id = self.__get_stream_id_from_server_status(
                jmsg['params'], self._params['client'])
            logger.info(f'Stream id: {self._stream_id}')
        elif jmsg['method'] == "Group.OnStreamChanged":
            self.send_request("Server.GetStatus")
        elif jmsg["method"] == "Stream.OnProperties":
            stream_id = jmsg["params"]["id"]
            logger.info(
                f'Stream properties changed for "{stream_id}"')
            if self._stream_id != stream_id:
                return
            props = jmsg["params"]["properties"]
            self.__update_properties(props)

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
        logger.info(f'send_request: {j}')
        result = self._req_id
        self._request_map[result] = str(method)
        self._req_id += 1
        self.websocket.send(json.dumps(j))
        return result

    def send_requests(self, methods):
        j = []
        for method in methods:
            j.append({"id": self._req_id, "jsonrpc": "2.0",
                     "method": str(method)})
            self._request_map[self._req_id] = str(method)
            self._req_id += 1
        logger.info(f'send_request: {j}')
        self.websocket.send(json.dumps(j))

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
                        if self._properties['playbackStatus'] == 'play':
                            self.send_request("core.playback.pause")
                        else:
                            self.send_request("core.playback.resume")
                    elif command == 'stop':
                        self.send_request("core.playback.stop")
                    elif command == 'setPosition':
                        position = float(params['position'])
                        logger.info(f'setPosition {position}')
                        self.send_request("core.playback.seek", {"time_position": float(position) / 1000})
                    elif command == 'seek':
                        offset = float(params['offset'])
                        strOffset = str(offset)
                        if offset >= 0:
                            strOffset = "+" + strOffset
                        # TODO: self.seekcur(strOffset)
                elif cmd == 'SetProperty':
                    property = request['params']
                    logger.info(f'SetProperty: {property}')
                    if 'shuffle' in property:
                        self.send_request("core.tracklist.set_random", {"value": property['shuffle']})
                    if 'loopStatus' in property:
                        value = property['loopStatus']
                        if value == "playlist":
                            self.send_request("core.tracklist.set_single", {"value": False})
                            self.send_request("core.tracklist.set_repeat", {"value": True})
                        elif value == "track":
                            self.send_request("core.tracklist.set_single", {"value": True})
                            self.send_request("core.tracklist.set_repeat", {"value": True})
                        elif value == "none":
                            self.send_request("core.tracklist.set_single", {"value": False})
                            self.send_request("core.tracklist.set_repeat", {"value": False})
                    if 'volume' in property:
                        self.send_request("core.mixer.set_volume", {"volume": int(property['volume'])})
                elif cmd == 'GetProperties':
                    self.send_requests(["core.playback.get_state", "core.tracklist.get_repeat", "core.tracklist.get_single",
                                       "core.tracklist.get_random", "core.mixer.get_volume", "core.playback.get_current_track", "core.playback.get_time_position"])
                    # return send({"jsonrpc": "2.0", "id": id, "result": snapstatus})
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

    def _get_properties(self, mpd_status):
        snapstatus = {}
        for key, value in mpd_status.items():
            try:
                mapped_key = status_mapping[key][0]
                mapped_val = status_mapping[key][1](value)
                snapstatus[mapped_key] = mapped_val
                logger.debug(
                    f'key: {key}, value: {value}, mapped key: {mapped_key}, mapped value: {mapped_val}')
            except KeyError:
                logger.debug(f'property "{key}" not supported')
            except (ValueError, TypeError):
                logger.warning(
                    f"Can't cast value {value} to {status_mapping[key][1]}")

        snapstatus['canGoNext'] = True
        snapstatus['canGoPrevious'] = True
        snapstatus['canPlay'] = True
        snapstatus['canPause'] = True
        snapstatus['canSeek'] = 'duration' in snapstatus
        snapstatus['canControl'] = True
        return snapstatus


def usage(params):
    print("""\
Usage: %(progname)s [OPTION]...

     -h, --host=ADDR        Set the mpd server address
     -p, --port=PORT        Set the TCP port
     -d, --debug            Run in debug mode
     -v, --version          snapcast_mpris version

Report bugs to https://github.com/badaix/snapcast/issues""" % params)


if __name__ == '__main__':

    gettext.bindtextdomain('snapcast_mpris', '@datadir@/locale')
    gettext.textdomain('snapcast_mpris')

    log_format_stderr = '%(asctime)s %(module)s %(levelname)s: %(message)s'

    log_level = logging.INFO

    # Parse command line
    try:
        (opts, args) = getopt.getopt(sys.argv[1:], 'dh:p:v',
                                     ['help',
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
        elif opt in ['-v', '--version']:
            v = __version__
            if __git_version__:
                v = __git_version__
            print("snapcast_mopidy version %s" % v)
            sys.exit()

    if len(args) > 2:
        usage(params)
        sys.exit()

    logger = logging.getLogger('snapcast_mpris')
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
