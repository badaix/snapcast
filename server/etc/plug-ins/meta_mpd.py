#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Author: Johannes Pohl <snapcast@badaix.de>
# Based on mpDris2 by
#          Jean-Philippe Braun <eon@patapon.info>,
#          Mantas Mikulėnas <grawity@gmail.com>
# Based on mpDris by:
#          Erik Karlsson <pilo@ayeon.org>
# Some bits taken from quodlibet mpris plugin by:
#           <christoph.reiter@gmx.at>


# Dependencies:
# - python-mpd2
# - musicbrainzngs
# - PyGObject
# - dbus-python

import os
import sys
import socket
import getopt
import mpd
from dbus.mainloop.glib import DBusGMainLoop
import logging
import gettext
import time
import json
import musicbrainzngs
import fcntl

__version__ = "@version@"
__git_version__ = "@gitversion@"

musicbrainzngs.set_useragent(
    "snapcast-mtea-mpd",
    "0.1",
    "https://github.com/badaix/snapcast",
)

using_gi_glib = False

try:
    from gi.repository import GLib
    using_gi_glib = True
except ImportError:
    import glib as GLib


# _ = gettext.gettext


params = {
    'progname': sys.argv[0],
    # Connection
    'mpd-host': None,
    'mpd-port': None,
    'mpd-password': None,
    'snapcast-host': None,
    'snapcast-port': None,
    'stream': None,
}

defaults = {
    # Connection
    'mpd-host': 'localhost',
    'mpd-port': 6600,
    'mpd-password': None,
    'snapcast-host': 'localhost',
    'snapcast-port': 1780,
    'stream': 'default',
}

# Player.Status
status_mapping = {
    # https://specifications.freedesktop.org/mpris-spec/latest/Player_Interface.html#properties
    # R/O - play => playing, pause => paused, stop => stopped
    'state': ['playbackStatus', lambda val: {'play': 'playing', 'pause': 'paused', 'stop': 'stopped'}[val]],
    # R/W - 0 => none, 1 => track, n/a => playlist
    'repeat': ['loopStatus', lambda val: {'0': 'none', '1': 'track', '2': 'playlist'}[val]],
    # 'Rate	d (Playback_Rate)	R/W
    # R/W - 0 => false, 1 => true
    'random': ['shuffle', lambda val: {'0': False, '1': True}[val]],
    # 'Metadata	a{sv} (Metadata_Map)	Read only
    'volume': ['volume', int],         # R/W - 0-100 => 0-100
    'elapsed': ['position', float],    # R/O - seconds? ms?
    # 'MinimumRate	d (Playback_Rate)	Read only
    # 'MaximumRate	d (Playback_Rate)	Read only
    # 'CanGoNext	b	Read only
    # 'CanGoPrevious	b	Read only
    # 'CanPlay	b	Read only
    # 'CanPause	b	Read only
    # 'CanSeek	b	Read only
    # 'CanControl	b	Read only

    # https://mpd.readthedocs.io/en/stable/protocol.html#status
    # partition: the name of the current partition (see Partition commands)
    # single 2: 0, 1, or oneshot 6
    # consume 2: 0 or 1
    # playlist: 31-bit unsigned integer, the playlist version number
    # playlistlength: integer, the length of the playlist
    # song: playlist song number of the current song stopped on or playing
    # songid: playlist songid of the current song stopped on or playing
    # nextsong 2: playlist song number of the next song to be played
    # nextsongid 2: playlist songid of the next song to be played
    # time: total time elapsed (of current playing/paused song) in seconds (deprecated, use elapsed instead)
    # duration 5: Duration of the current song in seconds.
    'duration': ['duration', float],
    # bitrate: instantaneous bitrate in kbps
    # xfade: crossfade in seconds
    # mixrampdb: mixramp threshold in dB
    # mixrampdelay: mixrampdelay in seconds
    # audio: The format emitted by the decoder plugin during playback, format: samplerate:bits:channels. See Global Audio Format for a detailed explanation.
    # updating_db: job id
    # error: if there is an error, returns message here

    # Snapcast
    # R/W true/false
    'mute': ['mute', lambda val: {'0': False, '1': True}[val]]
}

# Player.Metadata
# MPD to Snapcast tag mapping: <mpd tag>: [<snapcast tag>, <type>, <is list?>]
tag_mapping = {
    'file': ['url', str, False],
    'id': ['trackId', str, False],
    'album': ['album', str, False],
    'albumsort': ['albumSort', str, False],
    'albumartist': ['albumArtist', str, True],
    'albumartistsort': ['albumArtistSort', str, True],
    'artist': ['artist', str, True],
    'artistsort': ['artistSort', str, True],
    'astext': ['asText', str, True],
    'audiobpm': ['audioBPM', int, False],
    'autorating': ['autoRating', float, False],
    'comment': ['comment', str, True],
    'composer': ['composer', str, True],
    'date': ['contentCreated', str, False],
    'disc': ['discNumber', int, False],
    'firstused': ['firstUsed', str, False],
    'genre': ['genre', str, True],
    'lastused': ['lastUsed', str, False],
    'lyricist': ['lyricist', str, True],
    'title': ['title', str, False],
    'track': ['trackNumber', int, False],
    'arturl': ['artUrl', str, False],
    'usecount': ['useCount', int, False],
    'userrating': ['userRating', float, False],

    'duration': ['duration', float, False],
    'name': ['name', str, False],
    'originaldate': ['originalDate', str, False],
    'performer': ['performer', str, True],
    'conductor': ['conductor', str, True],
    'work': ['work', str, False],
    'grouping': ['grouping', str, False],
    'label': ['label', str, False],
    'musicbrainz_artistid': ['musicbrainzArtistId', str, False],
    'musicbrainz_albumid': ['musicbrainzAlbumId', str, False],
    'musicbrainz_albumartistid': ['musicbrainzAlbumArtistId', str, False],
    'musicbrainz_trackid': ['musicbrainzTrackId', str, False],
    'musicbrainz_releasetrackid': ['musicbrainzReleaseTrackId', str, False],
    'musicbrainz_workid': ['musicbrainzWorkId', str, False],
}


# Default url handlers if MPD doesn't support 'urlhandlers' command
urlhandlers = ['http://']


def send(json_msg):
    print(json.dumps(json_msg))
    sys.stdout.flush()


class MPDWrapper(object):
    """ Wrapper of mpd.MPDClient to handle socket
        errors and similar
    """

    def __init__(self, params):
        self.client = mpd.MPDClient()

        self._params = params

        self._can_single = False
        self._can_idle = False

        self._errors = 0
        self._poll_id = None
        self._watch_id = None
        self._idling = False

        self._status = {}
        self._currentsong = {}
        self._position = 0
        self._time = 0
        self._album_art_map = {}

    def run(self):
        """
        Try to connect to MPD; retry every 5 seconds on failure.
        """
        if self.my_connect():
            GLib.timeout_add_seconds(5, self.my_connect)
            return False
        else:
            return True

    @property
    def connected(self):
        return self.client._sock is not None

    def my_connect(self):
        """ Init MPD connection """
        try:
            self._idling = False
            self._can_idle = False
            self._can_single = False
            self._buffer = ''

            self.client.connect(
                self._params['mpd-host'], self._params['mpd-port'])
            if self._params['mpd-password']:
                try:
                    self.client.password(self._params['mpd-password'])
                except mpd.CommandError as e:
                    logger.error(e)
                    sys.exit(1)

            commands = self.commands()
            # added in 0.11
            if 'urlhandlers' in commands:
                global urlhandlers
                urlhandlers = self.urlhandlers()
            # added in 0.14
            if 'idle' in commands:
                self._can_idle = True
            # added in 0.15
            if 'single' in commands:
                self._can_single = True

            if self._errors > 0:
                logger.info('Reconnected to MPD server.')
            else:
                logger.debug('Connected to MPD server.')

            # Make the socket non blocking to detect deconnections
            self.client._sock.settimeout(5.0)

            # Init internal state to throw events at start
            self.init_state()

            # Add periodic status check for sending MPRIS events
            if not self._poll_id:
                interval = 15 if self._can_idle else 1
                self._poll_id = GLib.timeout_add_seconds(interval,
                                                         self.timer_callback)
            if self._can_idle and not self._watch_id:
                if using_gi_glib:
                    self._watch_id = GLib.io_add_watch(self,
                                                       GLib.PRIORITY_DEFAULT,
                                                       GLib.IO_IN | GLib.IO_HUP,
                                                       self.socket_callback)
                else:
                    self._watch_id = GLib.io_add_watch(self,
                                                       GLib.IO_IN | GLib.IO_HUP,
                                                       self.socket_callback)

            flags = fcntl.fcntl(sys.stdin.fileno(), fcntl.F_GETFL)
            flags |= os.O_NONBLOCK
            fcntl.fcntl(sys.stdin.fileno(), fcntl.F_SETFL, flags)
            GLib.io_add_watch(sys.stdin, GLib.IO_IN |
                              GLib.IO_HUP, self.io_callback)

            # Reset error counter
            self._errors = 0

            self.timer_callback()
            self.idle_enter()
            send({"jsonrpc": "2.0", "method": "Plugin.Stream.Ready"})

            # Return False to stop trying to connect
            return False
        except socket.error as e:
            self._errors += 1
            if self._errors < 6:
                logger.error('Could not connect to MPD: %s' % e)
            if self._errors == 6:
                logger.info('Continue to connect but going silent')
            return True

    def reconnect(self):
        logger.warning("Disconnected")

        # Clean mpd client state
        try:
            self.disconnect()
        except:
            self.disconnect()

        # Try to reconnect
        self.run()

    def disconnect(self):
        self.client.disconnect()

    def init_state(self):
        # Get current state
        self._status = self.client.status()
        # Invalid some fields to throw events at start
        self._status['state'] = 'invalid'
        self._status['songid'] = '-1'
        self._position = 0

    def idle_enter(self):
        if not self._can_idle:
            return False
        if not self._idling:
            # NOTE: do not use MPDClient.idle(), which waits for an event
            self._write_command("idle", [])
            self._idling = True
            logger.debug("Entered idle")
            return True
        else:
            logger.warning("Nested idle_enter()!")
            return False

    def idle_leave(self):
        if not self._can_idle:
            return False
        if self._idling:
            # NOTE: don't use noidle() or _execute() to avoid infinite recursion
            self._write_command("noidle", [])
            self._fetch_object()
            self._idling = False
            logger.debug("Left idle")
            return True
        else:
            return False

    # Events

    def timer_callback(self):
        try:
            was_idle = self.idle_leave()
        except (socket.error, mpd.MPDError, socket.timeout):
            logger.error('')
            self.reconnect()
            return False
        self._update_properties(force=False)
        if was_idle:
            self.idle_enter()
        return True

    def control(self, cmd):
        try:
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
                        self.next()
                    elif command == 'previous':
                        self.previous()
                    elif command == 'play':
                        self.play()
                    elif command == 'pause':
                        self.pause(1)
                    elif command == 'playPause':
                        if self.status()['state'] == 'play':
                            self.pause(1)
                        else:
                            self.play()
                    elif command == 'stop':
                        self.stop()
                    elif command == 'setPosition':
                        position = float(params['position'])
                        logger.info(f'setPosition {position}')
                        self.seekcur(position)
                    elif command == 'seek':
                        offset = float(params['offset'])
                        strOffset = str(offset)
                        if offset >= 0:
                            strOffset = "+" + strOffset
                        self.seekcur(strOffset)
                elif cmd == 'SetProperty':
                    property = request['params']
                    logger.info(f'SetProperty: {property}')
                    if 'shuffle' in property:
                        self.random(int(property['shuffle']))
                    if 'loopStatus' in property:
                        value = property['loopStatus']
                        if value == "playlist":
                            self.repeat(1)
                            if self._can_single:
                                self.single(0)
                        elif value == "track":
                            if self._can_single:
                                self.repeat(1)
                                self.single(1)
                        elif value == "none":
                            self.repeat(0)
                            if self._can_single:
                                self.single(0)
                    if 'volume' in property:
                        self.setvol(int(property['volume']))
                elif cmd == 'GetProperties':
                    snapstatus = self._get_properties(self.status())
                    logger.info(f'Snapstatus: {snapstatus}')
                    return send({"jsonrpc": "2.0", "id": id, "result": snapstatus})
                    # return send({"jsonrpc": "2.0", "error": {"code": -32601,
                    #                                          "message": "TODO: GetProperties not yet implemented"}, "id": id})
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

    def io_callback(self, fd, event):
        try:
            logger.error(
                f'IO event "{event}" on fd "{fd}" (type: "{type(fd)}"')
            if event & GLib.IO_HUP:
                logger.debug("IO_HUP")
                return True
            elif event & GLib.IO_IN:
                chunk = fd.read()
                for char in chunk:
                    if char == '\n':
                        logger.info(f'Received: {self._buffer}')
                        self.control(self._buffer)
                        self._buffer = ''
                    else:
                        self._buffer += char
                return True
        except Exception as e:
            logger.error(f'Exception in io_callback: "{str(e)}"')
            return True

    def socket_callback(self, fd, event):
        try:
            logger.debug(f'Socket event "{event}" on fd "{fd}"')
            if event & GLib.IO_HUP:
                self.reconnect()
                return True
            elif event & GLib.IO_IN:
                if self._idling:
                    self._idling = False
                    data = fd._fetch_objects("changed")
                    logger.debug("Idle events: %r" % data)
                    updated = False
                    for item in data:
                        subsystem = item["changed"]
                        # subsystem list: <http://www.musicpd.org/doc/protocol/ch03.html>
                        if subsystem in ("player", "mixer", "options", "playlist"):
                            if not updated:
                                logger.info(f'Subsystem: {subsystem}')
                                self._update_properties(force=True)
                                updated = True
                    self.idle_enter()
            return True
        except Exception as e:
            logger.error(f'Exception in socket_callback: "{str(e)}"')
            self.reconnect()
            return True

    def __track_key(self, snapmeta):
        return hash(snapmeta.get('artist', [''])[0] + snapmeta.get('album', snapmeta.get('title', '')))

    def get_albumart(self, snapmeta, cached):
        album_key = 'musicbrainzAlbumId'
        track_key = self.__track_key(snapmeta)
        album_art = self._album_art_map.get(track_key)
        if album_art is not None:
            if album_art == '':
                return None
            else:
                return album_art

        if cached:
            return None

        self._album_art_map[track_key] = ''
        try:
            if not album_key in snapmeta:
                mbartist = None
                mbrelease = None
                if 'artist' in snapmeta:
                    mbartist = snapmeta['artist'][0]
                if 'album' in snapmeta:
                    mbrelease = snapmeta['album']
                else:
                    if 'title' in snapmeta:
                        mbrelease = snapmeta['title']

                if mbartist is not None and mbrelease is not None:
                    logger.info(
                        f'Querying album art for artist "{mbartist}", release: "{mbrelease}"')
                    result = musicbrainzngs.search_releases(artist=mbartist, release=mbrelease,
                                                            limit=1)
                    if result['release-list']:
                        snapmeta[album_key] = result['release-list'][0]['id']

            if album_key in snapmeta:
                data = musicbrainzngs.get_image_list(snapmeta[album_key])
                for image in data["images"]:
                    if "Front" in image["types"] and image["approved"]:
                        album_art = image["thumbnails"]["small"]
                        logger.debug(
                            f'{album_art} is an approved front image')
                        self._album_art_map[track_key] = album_art
                        break

        except musicbrainzngs.musicbrainz.ResponseError as e:
            logger.error(
                f'Error while getting cover for {snapmeta[album_key]}: {e}')
        album_art = self._album_art_map[track_key]
        if album_art == '':
            return None
        return album_art

    def get_metadata(self):
        """
        Translate metadata returned by MPD to the MPRIS v2 syntax.
        http://www.freedesktop.org/wiki/Specifications/mpris-spec/metadata
        """

        mpd_meta = self._currentsong.copy()
        logger.debug(f'mpd meta: {mpd_meta}')
        snapmeta = {}
        for key, values in mpd_meta.items():
            try:
                value = {}
                if type(values) == list:
                    if len(values) == 0:
                        continue
                    if tag_mapping[key][2]:
                        value = list(map(type(tag_mapping[key][1]), values))
                    else:
                        value = tag_mapping[key][1](values[0])
                else:
                    if tag_mapping[key][2]:
                        value = [tag_mapping[key][1](values)]
                    else:
                        value = tag_mapping[key][1](values)
                snapmeta[tag_mapping[key][0]] = value
                logger.debug(
                    f'key: {key}, value: {value}, mapped key: {tag_mapping[key][0]}, mapped value: {snapmeta[tag_mapping[key][0]]}')
            except KeyError:
                logger.debug(f'tag "{key}" not supported')
            except (ValueError, TypeError):
                logger.warning(
                    f"Can't cast value {value} to {tag_mapping[key][1]}")
        logger.debug(f'snapcast meta: {snapmeta}')

        # Hack for web radio:
        # "name" and "title" are set, but not "album" and not "artist"
        # where
        #  - "name" containts the name of the radio station
        #  - "title" has the format "<artist> - <title>"
        # {'url': 'http://wdr-wdr2-aachenundregion.icecast.wdr.de/wdr/wdr2/aachenundregion/mp3/128/stream.mp3', 'title': 'Johannes Oerding - An guten Tagen', 'name': 'WDR 2 Aachen und die Region aktuell, Westdeutscher Rundfunk Koeln', 'trackId': '1'}

        if 'title' in snapmeta and 'name' in snapmeta and 'url' in snapmeta and not 'album' in snapmeta and not 'artist' in snapmeta:
            if snapmeta['url'].find('http') == 0:
                fields = snapmeta['title'].split(' - ', 1)
                if len(fields) == 1:
                    fields = snapmeta['title'].split(' / ', 1)
                if len(fields) == 2:
                    snapmeta['artist'] = [fields[0]]
                    snapmeta['title'] = fields[1]

        art_url = self.get_albumart(snapmeta, True)
        if art_url is not None:
            logger.info(f'album art cache hit: "{art_url}"')
            snapmeta['artUrl'] = art_url
        return snapmeta

    def __diff_map(self, old_map, new_map):
        diff = {}
        for key, value in new_map.items():
            if not key in old_map:
                diff[key] = [None, value]
            elif value != old_map[key]:
                diff[key] = [old_map[key], value]
        for key, value in old_map.items():
            if not key in new_map:
                diff[key] = [value, None]
        return diff

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

    def _update_properties(self, force=False):
        logger.debug(f'update_properties force: {force}')
        old_position = self._position
        old_time = self._time

        new_song = self.client.currentsong()
        if not new_song:
            logger.warning("_update_properties: failed to get current song")
            new_song = {}

        new_status = self.client.status()
        if not new_status:
            logger.warning("_update_properties: failed to get new status")
            new_status = {}

        changed_status = self.__diff_map(self._status, new_status)
        if len(changed_status) > 0:
            self._status = new_status

        changed_song = self.__diff_map(self._currentsong, new_song)
        if len(changed_song) > 0:
            self._currentsong = new_song

        if len(changed_song) == 0 and len(changed_status) == 0:
            logger.debug('nothing to do')
            return

        logger.info(
            f'new status: {new_status}, changed_status: {changed_status}, changed_song: {changed_song}')
        self._time = new_time = int(time.time())

        snapstatus = self._get_properties(new_status)

        if 'elapsed' in new_status:
            new_position = float(new_status['elapsed'])
        elif 'time' in new_status:
            new_position = int(new_status['time'].split(':')[0])
        else:
            new_position = 0

        self._position = new_position

        # "player" subsystem

        new_song = len(changed_song) > 0

        if not new_song:
            if new_status['state'] == 'play':
                expected_position = old_position + (new_time - old_time)
            else:
                expected_position = old_position
            if abs(new_position - expected_position) > 0.6:
                logger.debug("Expected pos %r, actual %r, diff %r" % (
                    expected_position, new_position, new_position - expected_position))
                logger.debug("Old position was %r at %r (%r seconds ago)" % (
                    old_position, old_time, new_time - old_time))
                # self._dbus_service.Seeked(new_position * 1000000)

        else:
            # Update current song metadata
            snapstatus["metadata"] = self.get_metadata()

        send({"jsonrpc": "2.0", "method": "Plugin.Stream.Player.Properties",
             "params": snapstatus})

        if new_song:
            if 'artUrl' not in snapstatus['metadata']:
                album_art = self.get_albumart(snapstatus['metadata'], False)
                if album_art is not None:
                    snapstatus['metadata']['artUrl'] = album_art
                    send(
                        {"jsonrpc": "2.0", "method": "Plugin.Stream.Player.Properties", "params": snapstatus})

    # Compatibility functions

    # Fedora 17 still has python-mpd 0.2, which lacks fileno().
    if hasattr(mpd.MPDClient, "fileno"):
        def fileno(self):
            return self.client.fileno()
    else:
        def fileno(self):
            if not self.connected:
                raise mpd.ConnectionError("Not connected")
            return self.client._sock.fileno()

    # Access to python-mpd internal APIs

    # We use _write_command("idle") to manually enter idle mode, as it has no
    # immediate response to fetch.
    #
    # Similarly, we use _write_command("noidle") + _fetch_object() to manually
    # leave idle mode (for reasons I don't quite remember). The result of
    # _fetch_object() is not used.

    if hasattr(mpd.MPDClient, "_write_command"):
        def _write_command(self, *args):
            return self.client._write_command(*args)
    elif hasattr(mpd.MPDClient, "_writecommand"):
        def _write_command(self, *args):
            return self.client._writecommand(*args)

    if hasattr(mpd.MPDClient, "_parse_objects_direct"):
        def _fetch_object(self):
            objs = self._fetch_objects()
            if not objs:
                return {}
            return objs[0]
    elif hasattr(mpd.MPDClient, "_fetch_object"):
        def _fetch_object(self):
            return self.client._fetch_object()
    elif hasattr(mpd.MPDClient, "_getobject"):
        def _fetch_object(self):
            return self.client._getobject()

    # We use _fetch_objects("changed") to receive unprompted idle events on
    # socket activity.

    if hasattr(mpd.MPDClient, "_parse_objects_direct"):
        def _fetch_objects(self, *args):
            return list(self.client._parse_objects_direct(self.client._read_lines(), *args))
    elif hasattr(mpd.MPDClient, "_fetch_objects"):
        def _fetch_objects(self, *args):
            return self.client._fetch_objects(*args)
    elif hasattr(mpd.MPDClient, "_getobjects"):
        def _fetch_objects(self, *args):
            return self.client._getobjects(*args)

    # Wrapper to catch connection errors when calling mpd client methods.

    def __getattr__(self, attr):
        if attr[0] == "_":
            raise AttributeError(attr)
        return lambda *a, **kw: self.call(attr, *a, **kw)

    def call(self, command, *args):
        fn = getattr(self.client, command)
        try:
            was_idle = self.idle_leave()
            logger.debug("Sending command %r (was idle? %r)" %
                         (command, was_idle))
            r = fn(*args)
            if was_idle:
                self.idle_enter()
            return r
        except (socket.error, mpd.MPDError, socket.timeout) as ex:
            logger.debug("Trying to reconnect, got %r" % ex)
            self.reconnect()
            return False


def usage(params):
    print("""\
Usage: %(progname)s [OPTION]...

     --mpd-host=ADDR        Set the mpd server address
     --mpd-port=PORT        Set the TCP port
     --snapcast-host=ADDR   Set the snapcast server address
     --snapcast-port=PORT   Set the snapcast server port
     --stream=ID            Set the stream id

     -h, --help             Show this help message
     -d, --debug            Run in debug mode
     -v, --version          meta_mpd version

Report bugs to https://github.com/badaix/snapcast/issues""" % params)


if __name__ == '__main__':
    DBusGMainLoop(set_as_default=True)

    gettext.bindtextdomain('meta_mpd', '@datadir@/locale')
    gettext.textdomain('meta_mpd')

    log_format_stderr = '%(asctime)s %(module)s %(levelname)s: %(message)s'

    log_level = logging.INFO

    # Parse command line
    try:
        (opts, args) = getopt.getopt(sys.argv[1:], 'hdjv',
                                     ['help', 'mpd-host=', 'mpd-port=', 'snapcast-host=', 'snapcast-port=', 'stream=', 'debug', 'version'])
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
        elif opt in ['--mpd-host']:
            params['mpd-host'] = arg
        elif opt in ['--mpd-port']:
            params['mpd-port'] = int(arg)
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
            print("meta_mpd version %s" % v)
            sys.exit()

    if len(args) > 2:
        usage(params)
        sys.exit()

    logger = logging.getLogger('meta_mpd')
    logger.propagate = False
    logger.setLevel(log_level)

    # Log to stderr
    log_handler = logging.StreamHandler()
    log_handler.setFormatter(logging.Formatter(log_format_stderr))

    logger.addHandler(log_handler)

    for p in ['mpd-host', 'mpd-port', 'snapcast-host', 'snapcast-port', 'mpd-password', 'stream']:
        if not params[p]:
            params[p] = defaults[p]

    if '@' in params['mpd-host']:
        params['mpd-password'], params['mpd-host'] = params['mpd-host'].rsplit(
            '@', 1)

    params['mpd-host'] = os.path.expanduser(params['mpd-host'])

    logger.debug(f'Parameters: {params}')

    # Set up the main loop
    if using_gi_glib:
        logger.debug('Using GObject-Introspection main loop.')
    else:
        logger.debug('Using legacy pygobject2 main loop.')
    loop = GLib.MainLoop()

    # Create wrapper to handle connection failures with MPD more gracefully
    mpd_wrapper = MPDWrapper(params)
    mpd_wrapper.run()

    # Run idle loop
    try:
        loop.run()
    except KeyboardInterrupt:
        logger.debug('Caught SIGINT, exiting.')

    # Clean up
    try:
        mpd_wrapper.client.close()
        mpd_wrapper.client.disconnect()
        logger.debug('Exiting')
    except mpd.ConnectionError:
        logger.error('Failed to disconnect properly')
