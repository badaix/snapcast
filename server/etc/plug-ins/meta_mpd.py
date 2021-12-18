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
        logger.debug("IO event %r on fd %r" % (event, fd))
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

    def socket_callback(self, fd, event):
        try:
            logger.debug("Socket event %r on fd %r" % (event, fd))
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
        except:
            logger.error('Exception in socket_callback')
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

        # if 'metadata' in snapstatus:
        #     snapstatus['metadata']['artData'] = {}
        #     snapstatus['metadata']['artData']['data'] = '/9j/2wCEAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDIBCQkJDAsMGA0NGDIhHCEyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMv/AABEIAZABkAMBIgACEQEDEQH/xAGiAAABBQEBAQEBAQAAAAAAAAAAAQIDBAUGBwgJCgsQAAIBAwMCBAMFBQQEAAABfQECAwAEEQUSITFBBhNRYQcicRQygZGhCCNCscEVUtHwJDNicoIJChYXGBkaJSYnKCkqNDU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6g4SFhoeIiYqSk5SVlpeYmZqio6Slpqeoqaqys7S1tre4ubrCw8TFxsfIycrS09TV1tfY2drh4uPk5ebn6Onq8fLz9PX29/j5+gEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoLEQACAQIEBAMEBwUEBAABAncAAQIDEQQFITEGEkFRB2FxEyIygQgUQpGhscEJIzNS8BVictEKFiQ04SXxFxgZGiYnKCkqNTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqCg4SFhoeIiYqSk5SVlpeYmZqio6Slpqeoqaqys7S1tre4ubrCw8TFxsfIycrS09TV1tfY2dri4+Tl5ufo6ery8/T19vf4+fr/2gAMAwEAAhEDEQA/APScDPWgjmlIz2oxXonmidaCBmij1oQDcUYpwFBFO4huPejHNLikzii4Bj6UlLiimA3vQRzSkZFBFNAhD1pMZpxpKAsJjFFKeBSUAB60HrR1ooBiUlOoIFO4DaDS0GmFhMUYHrR0FGKQCfSjBJ60uKCOKd7iYx2WNTISAqjJJ7DvWVpDNeyT6g6kCX93GD/Cg7fjml1uWSTyNOt/9ZdMQzf3AD/n8jWlBAltbxwR/cjXavuPWpvzMte4rj8dfU9aOtLRiqMxuOeKKU0HpTTATtRilFGKLgJikxTiKSlcLCYoxmlo6UD6DSKUUvFJj0p3JEwPWjFGOaMe9FxhRijGKKLgJRilo4ouIaelL2oop3BISjGO9LijBzQFjQGMUhpeKO1c5rYbijilo/CmAlJTqSgVhKDSmjPtQFhMd6Qind+lGfamFhuM0Y9aUUEUXAbgUhpwHeg5ouA3GaOBS4opgNFGKWijUQmKMZNOxSYxTTGJjmkxS9TRii4rCEUYpaKYDcE4oA3FR6kUveszXbx7XTykIzcT/uovYngn6/40norjSu7EWmyDUNSuL7+CH9zD9OpJ/P8AKtfGAKrafYpp9mtuo5HLH1bv/n2q1SS6hLXQTFIeKcaTGaq4htFOpDQAnFKaMg0UCENJTjSYouAnFBApcUAUBuNop2MUh4oCwmM0UvQUUwsNHNFLRRcLCYzRiloHShMSQ3FFLRTuMTNFLikoEaAFGKdkUlc9zUSm84p+KTGBTEJR2pRRyaAGE47UA57fnTsc0mPegApvSnGkIouAUEfjS4HSk6CmAmaDR16Y/E4o6f8A6qB2EpKdnPSimgsJSZzS8mlxz0ouJobRilPHSkoEITQBmjFLTGJjFJTsUmKBDTxWUix3+vGUYMdh8qejSZ5/LA/KrOr3v9nWDzKN0jfJGP8AaOcfyNJpFj9g09FJLSP+8kJ9W5qWy4qyuXccAelGKdjvQaoiwlFFHSmIbiinGilcY3FJT6TFO4WG4paXGaOB3oAbR3p340nWmAh60mM07GKAM0riG0UoFHei4DaKdik6UxCUlONGKBiUUY4pe1AhvaijHFGKdxWNHnrxRjinbaCMVzm4zBpCKfj8fekxQIbRilxSgUCsMIpOafijFUAw5psjrHG0juFRB8zHoKWWSOGNpZZFSNeSzHH4V5pr2vvqmpqgI+zxhiqg8EAjGfUnOaznNRRpCm5M6+88U2VvJtgBuRj7yMAv9T+lZ8/jPaMx2i9OjNn9ODXE3GoNHuGAcHHWqLamu9X27h3Q9D9T2rCVWXQ6o0I3OxufG96SBGYol9Ao5/MmqR8a6gnP2sk+6g/lkVzF7qwuc4jijQMzIEGMZ7Z7gfSsSe7IYYbp0qIzm9zSdKEXZHpUHxAulYLItvNnuy7SPxX+tbmm+NrC6fZdr9mOQA2dyk+/pXh7Xkh6ULqEqsD5hAHcGtY1GjKVOLPpRJEkjWRGVo2HyupyD6GnV4t4Z8eXWi5jYCWBxllcfxeo6HP8/Q16roOu2uvWnm27DzF++m4Ej39x+HHtW8ZqSOadPlehqYpMcU6jFWZjcUYpxBpKAG85oIpxxVW/u1sLGW6f5vLX5V/vMSAB+opiM24hGpa/FGW3Q2KCSRQeGc4wp/IfrW0fvdc1maDayRWJuZzme8fz3yOmc4H65/EVq7aSHLsNzQaCOaUincQ2kNPFIfpSATBoOTS4pMH1qgEwaOelL70YzQIbg0UtAFHQBpFAzTiKMU7hYSig0dqQCUnNOoI5oENopcdKMc07gIeaOlLikoGFBo70UAJzikB9qcKQdaA6Glig+lLilxWBoMxRjFPIppFIBCKTAzzT+MU09aYDTzVa9u4NPtXuLh9qL+ZPoKTUtRh0qxe6n+6vRe7HsP8A69ea6j4hOu3e6eRbe2gU7QmT7/mT3rOdTlNKdNzY/XfEU+qSF9ojjQHYgP6/X3rjRq+y7DBMZQg569v8KuXt8hYiPhB0BOSR9a5G7lIuGI4wetY35tWdbhy6I2brVhIfugY4xWc18cHBA9qzprkDGDyetQG446mhJCu0jTa7Pc8Gq8lxyOeR0qkZHKghTjOM06GKS43bc44/z1pXW7KUZNpJakpuSFKjjJ5qWytLrULgQ2kTyyEZAVew6nPYD1NXU0QW1qLmeVJGGH8pTwyHPOeDxg8fQ0yDXH0m6SfT3aGRMlGQnK56rk9Rgd81Maib0NpYeUI3m7Grb+D7qaxuZGuoor23HmtbsSSY8DLDZngE/oDxW94Z0u402O0v7XVyhkCvhIwfmBwyZzyw54wAcfSuWvfHGp3M0E0D/ZpoGJjlUjcoOARn+6cdMVUk8W6xPcSyvf3CtNjzPLcxg9MEhcZIHf8A/XXQnFbHI03ufRGlawl9tjkZPN2rtZVID5Gc4JPv39elaijI47V836D4svtHmaRZFlVkZGSUbxhs5wD0OeeMc817V4P8Y2/iWAxybY7scgFid4/H8K1jUujGdPqdPikxinAZ/wA9KXFWY6DOv19M1h6nAdS1i1sC5FvEPtE4B59FB/L9a2LqeO1tZZ5SRHGhZiOuO+Pes7w+s80FxqNwoWa9kzt9FHAH04P4AUmUu5q8dAAAOMAcUYoxS44p3Jeo0UYpcYoxQKwmOKCKXFBHvQFhMe9JilxRincNBMUYpe+KMCi4DcZoxzTsCkxSAQjmkxSkCjFUA3FGKdgUcUriG0EcUtGOKAG44oxS4oxxRcBMUY96XHvRwKYhpx2pKdRgUANxRj1p1FMLGlxRxS0EYrnubbDetGKWlwRRcBmKP4umc8YI9eP60pJBya5zxprjaLopMeBNPuRTn7oxyR78ik2NK7OH+IHiI3momygYGCHK9fve5/IVxU155aEA9ue1Urq+kuLl5nPJOazrm6LtjoKweurOqOisW7jUepOMnk1jXVyZWyKbLISeuarnrQMCxPJNAbvmk5z7UnagRctrgGM20pxGx4PoaSO5NrO2zY4xtJI4xVSjNTymvtWkW5L+eVAjOVUcBVPA9arlieDTM0uR600kiJTlP4tRaUcnOKFUseBnNaNlomoXwzDbuV/vEECqJtcoK2DxWtpOr3GmXKywsVIIPHFadt4GvHBMlzDH+Z/pWhH4BwBnUM+u2H/FqOaw7HsHhDxKviXTHlfYtzCQJQONw6B/zwPyroipzjPI614/4c0/UPDF/HPZXkcygjfHKpUOO4OM16nZaraX1qrpKqP5ZLxO43px3HU8d63jNNHNUptO6M/XFkv7y10dDtWfMszDsg4/x/EVuIgjjREG1UUKo9AOKx9BlbU5rzVWXHmt5MI7rGO345ra/DryBjmquQ9rDcUuKX6jFFVcm1hvFJx6U8ikAouA2jinYox7UXAZgUYHvTvwoxmi4DT19qTvTiPajHNO4rDaKdjFJj2pXCwlFLijFO4DSKMU7FJ0ouKw00ACnYoxRcBOlNPWnYPpRTAbQRTsc0lFwExSYp34UdqLgNpKfj2pMc0XA0qMZp1J7Vz3NRKTFOo7UXAbjnjGa8n+MF6Fm062BAwjSE5/vNj/ANk/WvWcCvB/jLcBvFUUQPMdtGMenyg/5/GlMuC1PP5rj5QqHiqjMe5ppY8AUhPfqayNxrEk03FKR3ooATqKMZFJRz60AHSjFJmloAOeMVbsrKS7k2pnHc+lVl69K6XTV+y2428MeT70MaNHStJtbQ7mRZH45bt9K6KK4GCB0FYEd0SQCcHtxU4uyB945zjipKWhvi7RSOgPtR9uI52/1rGWZ3ccZ+gzUkk8dnHvuG2k9E7mlYZtR3jvjk4z27Us99b2i4uZVT0Gf5elcZe+J5sGO3URg8ZHWueutUuZHO5jk9WJPNG+xWnU9Tt/iG2nLss3Z4l52y7dg57bs459Kut8Wvs5jWXT7WRXXJMNwePrwcV4iZ3bqSfxpVkYHJJP41ackZSUGfSOifELRNZIhkl+w3BICrckBWPpu6A/XFdZ6dMEZGe4r5Lt7t4mHOf9kjIPtXqngDx69sqWV8Q9mzDLmTLQA/xY/uZxkduCOprWNTXUzlRTV4nr/WinMu1ipHIODSYrS5zWExSUppOtAWCjtS9OopMUwsJijFOpCcUXDUTGaTHvTqKNBDcCgc0poAwKdwEpMUp4paLjQ3GaSnc0UXBobzRinUc0xWG0mM04ikxzSuFhMUmOadzQfWi4huKMZpwxQevFFx2NE03vTqMVjc0ExRilxik70AN/DNfOfxIE13411KUjKrKyqfocV9G4LEAdScfj2r578aTLLr9+Y/uvM5B/E1M2aQWrPPnBBwe1M6HpVq6jCtkE+/1qoTzUGogJNA57/hSE4pQpPNA0hCCO1KQCBjPuDTtuevWp7eDexBqblcpUxjtS9q1BaI456j0qKSxUDIf6jFCkmDgyrboXkAHet+FWIUAdu4rHitnRwQcEVeQ3wG2NXI7YFDaBRZqxxNj5iOmTmpfPhjT95JznhVFYpivnO2TcFHHzGpVspQQCyjPvUuSRSg+ppHV3AEcKqik4Mkg5FVbqJ5pAxlZlxncVxu9wP8aW3sX3DcQfm65q/JCdgx+dQ6h0RpqxjSWwhHc555qkbZrqZURQMkDJz3/Ctq5CsQrPxVrRY7ZdRh3thQ4J2jccZGeAM9M1UNzKotNDD1bwxq2hmMahZSwiRBJGxAKuvqCCRWUY2HWvqqO8s7nTViu7KY2uwA/abcOhAAGSBnGaxNS+GXhXVds0Vu9qG53WcgCsD3wwI/LFdXs7HIqmp84gE8c9K6fwhZ315q4gs4fNMgKshXIwR3HpXqY+DehBgReX5HoWT+i11nh7wppfhmKRNPRy8mN8spyxHpwAP0pKGoe0XQ1LeNobaGFpPMeKNY2c9XKqATUuOaX3o6mtDDcaRQadijFMVhuPSgj3p2KMCkFhuKQilop3Cw3BoxTsUYzRcLDcc0hzT8CkPXg0XCwzGaXHFLzigZoCwmKMUuPWjFNAMPFLSkUYoAbRjmlI9KXFADMUYNKetLjPINFxWGY5o56Yp1GPrRcLF/GeelFOxxRisTWw2kJp5FRTbhG23qBkD1ouFihrOqLo+lXN+U3mBNwTON3IA57V856xdGS6lkJJJY4z6dRXumuOupWc1ruwzKyhdpOehA6eorza50OzVQ93ahjuHRip/SuerV5XqdVKlzLzPOI7a61KcR2kEsrE8Kik5P0Fa0Hgy/CN9rR4Gzt2YBOfTrxXbWOtx6NuXTNMtYZOA0rLvc47ZPaqF1q2pzzPcPOjswPBTgZ/l0rF17vQ6VhWlc5abw61s237OzAn7xYiqSaHf3Mj/ZrRnCjLYwAPxruNLvDJcKlyFZs/KzKGx+fetHV5oY7Q/bLhIYsbvKVhHvxjjaBzSVVt2IdKx5K8EsUpjdcMDhgexq/ZKEU5xVe4lEk2R061dtEVl5FayehMFqWAikZ2ijygxGRz709bdCeM1Zgtkz1cfjUcyNuXyEhskUZK5pJrhISFVRn0FPmU9BLJj0LZFUms5nbKyEqTyKE092Nqy0Qkl5IWwFGSM8moxMc/Md3fripjppkYoSAQAc+1RtYFFVS3T0qrohJ3LUM0ark5HpjikOol2MYGR/eqnJC5jwM8e/Wp9PsixbI/Sh2RS5tirLIXuQmSzNwowa1/D8scOuWokQvGJVDk9xxnOOcYpxtBFLFIACUYMMjuK1PCCW1tr1pJMMKJkJJ7fN/n8KqDVzOcJJM9tjtwYVMMyCIrmMxqSMY45LdPzqDR2ulvbuB44ltgRInltnDHHGGAwOGP1PvTtR1S1sFaOMK7j+CPt9T2qXRoZktZLi44luHMhXoABgAf1rt5nax59kaBHNGKdx160daWxNhuOaMUtFFwsJR1pcCkouFgxikwM0pHqaMUrisNIOKTFPPNJincdhCPekxT8UmKLisNxR0p3FJwaLjGkc0YpcZFG3jrTuIQikIOeDT8fjSGi4DefU0UtGBRcBvejvyacRRtycDrTuFhuKO9OxzjjNNIwSDwe4PFTe4colIKdtpQKoVi/R2oAorA2EpkhA6/mKkprDI64oBI57V9JWYPLC5UnltvIPvjPXmvMfEepX1rP9nEuUyVbCleCfqa9U8QPHBYuzHDlTgjg/pXhHim/L3vyZAX/aJz35rkqx5mdtCdkWPMC59zUjufKKqM59O9Z6SiaGGRejqCfatGRvJhUKMFu/tWDSTPSveNyi4bymVsAjkAisa8h3LnfwBwelasgJZzyc9KxrvzkIiZCv1rSmtTmq6IzCCZeWJ5rbsosKMnnHWqkVmWcMScgZrSiGwZIrSb0Maa1J1hAPGc1KqkUA8AjrViOLeM56VmdCsVjBuG45qNo3jOVBOOwrU2HaBjIpywe1C3sVZWMUvKHJKkcYpyxM4LOcCtp7JGIJ79aj+wp6/TFNySJ5TIEKSZRQSfX0q5Egto8AY9Tmp5ilvHtjX5j3NUmjuJhgdO9Ne8PlCScs2F6VasY83Csw9gT7/5/Wls9OBUtI3I7YrShs1X5kJOPbvVRdmRKN0z1jS9AsxHb3byvch0WRd4wOQOozz+dbpyeSTmsfws8j+HrcS9EJRD/s5/xyK2e3SuxO6PJlGzsA70mKUd6OlO4rDcUU6kIoEJxQRR+FLQA2ilooATFGKWigVhpGKKXvQetACd6Q/SlxS9BQA3rRS0U7gJRwaDRRcBMUhGDTs0UXAafUD8fSuT8SeLbvR757OxCLIF+Z2GTkjP8q63uOme2a5fxJ4Vk1a5N1ZyQrc7QpSbIDYGAcjPbj8Kid7F07X1PPr3xFqdy/mSXTMevQYFbXgzxffPrNvpV5I00Ny22NnY5Rz0x7Z4x71zZsrm7uI7eFA0sjqiIO5JAA/Wu58K+C7iw1CLUdRTypoDmKEMpO7BGWIJxjOfqKyg5XNpqNjt8AdKXNLijGMV0dDlsXs4pOtB5oI7g1kahQev+NHakOD3/TNMdjmvFdibm03RsUYDnsCDxXgviePyZO+SOK+iNXha4tjG3CEYOB0rxHx5YhL5RCmIACFbHXnrXPNXlc3haxzmjTmSB4Wb5k5Tjsa2ZpnkiWVRkYwRXKI7WcwlTKsDW/Z3TXUfmwnDdHHXbWc4K9zuo1Lx5WOe5BwemP0q1aQw6rEd6gOh/Onw2qeWTKgYnmpYI1tpCIV25xk+tQ5WWhpytlKex+zEAkH6VX24+laF2+9snGaotn/9VEW3uZvRioDnjNXoQR9O9VYwfarsS55znNJ3GmWkXOBjipQnPXio4+nNSnCipvqWhxVRz1qrOSHA49atb/l46msmeRmlOKa1dht2HXscbxffww5+tR290I12+US3c5qNld+oyPpSCCWM87T9TWqjYyc30LUjyzAMJni284FaGlzNM6qvztkfic1iyJMzdQFPTniuk8CWjHxBZJKAVaZWPcYBz/SqjEznNpansWn2Y0/Tra0H/LKMBvdup/WrNJk/iaX611LRHmvV3EIpQKOTSA4p3EBo70daKYWCkpaOlAWDBpMZNLSZ5ouFhCKKCeaKEFgoPNH0ooEGKSloxQFhDzRigcUUBYQ0UHrRQFgpKWkoAOuaa4UxOrZK7CDg9iORTqr30ph0+5kU4KxMR9cYpPYErnm/ha23eLrOLOdjluOmEUsP5V6cB8oHavMfCd3DF4mE87KkYWQBifu7vlGPz/LNenRssyExukg6kowYCpizSotbBilxRRj3q7mdi3nmjmm54NBPFQUDbv4Ar5I4B6/Q9PzxUcMqzr5g3hdxA3LjOOP6dqkByfUjsagtCBbR49/5mmF0yvqJLRoittLkAnPbrz+VcN4s0lbrSJFRc7IjlyeFwd2PrnIrvrpdwjcZODzxn1rMv7E6nAYXYxxt1xyWz25qGi07HzTeR7JSOvc1ThuZrSffCxB9PX2NdT4z0v8As3UQqB1Eqll3Ajrnp+Vcg4O7PJz61n5M1Ts7o63S/EEdymyePa47ryDWs9whXMQySOp7VwumsUuMDvXWQuDH6/hWE4K52UqsnGw12wpyMkGoR83anTk7sEHNEYBAIxzVJaCvqTRL2z+NXIh0qpFnGAM+wq7ECqjt9aykaosKQR9Ke2SOlNUjrxT2yRxxU9RjHzj6dcVlXJlgBZI9x7DNahyrDd+NQMFaT5quOhm22zFkvLs8Onl5HQGot1w/PBPvXRfZ42UFlH5VA9rBE27aw/Gr5kUtjHjju27gAdq774eQMuv2+7JKhnPH+yR/UVybMqkhIh+J613nw3TzNXnkYbSluRj3JArWGrMa7Vj0z2PagHjFNHIpc9a6Tzri5oNJnpRmkAA4oyc0dKQHNAC5HWjOSADzSMcDPYCo/MHnCPv5bMPoMf40CJc+tJnmjFGaADIo60lGaAF70cZpNw60maBjicUA03OaM5oEKTSUE8dKMimMDSA0tJmgTFNJRmgnmkAhGT1rN16UQ6Hdtn+DH6itM9D9KwPGEwj8PTAAZZ1X8Op/lSew47nnmloWkc7iNoCjGccn/wCtTfEF9eaZFZzWU0kUgkPMZI6AVoeHYUkSYuucuuPwz/jWrf6FY6iIVmRyq5wqtjJP/wCqpSbhodUJqM7tFPw98U1ndINcjCZGPtMK/wA15/T8q9GinhuIUmgmjlicZV4mDA/iK8n13wFCsfnae7Rtj/VuSVPTIyen+elZOg+LNW8IzyWckazQM2Wgm4wfUHqP/wBWe1LmcPiNnQhWXNS3PWbvxlptlcyQMJpGT+JFGCc9uRU+l+KLHVbkQwLPHISAodAc/iCa5AwwNnMafTaB/LFHkQBgRGgI/iWuh0meX7SJ6HFMk6MYicIzIRjGCOv+femWQb7FD1IK7s/XmuAjVYsmMsnqVZhn9anS4nhGIry5jHoszAfzo9lIPaRO+Yj257VHJEkqFXXI/wBrtXFjUtQXgapdAH/pqT/M0qapqC5A1K4Ps20/0pezkVzxOb+K2lQ2ws7hQ+XDJ8zFsY9CSeOfbp0ryWWE/KRg54yK9d8Yx3mtacomnaURElcqo69TkAV55JaQQadM877ZtwMIHVgc7uo56cj6Vzzg4vU3pyUo6GXZQxxzHztyuBlQMEH61vQH5R6VgJIzXKswHoTXQQk7VIHWsp6o6qSsEyggHn2pImx8rfnU7LlTk9KhRQTgAgd6hPQ0ki1EoKnqfpUwbafY1VBePODxS+c452gms2UmaCsM+lP81VHJFZwuflOcg/Wmed1Gc/rQkwlJF55wT1H41D5xaVSMAVUefBxgH2NKjBmq0jPme5qCUhSSAcdcdqCQ6c/gTVFT5ZB/Op42JB5ODScbDTuiJl55HH0rqvAurR2Gqy+cp8uWPYxB5UZBz+lcw+MgckE0+wl8m+3L26ZrWDIqK59CRWk0yrIi5V13Bs4BB5z1pTZXCn7mf91sn9DXO+EtcuLnTBZ7JHEZ270QttU9jjnsa6WS7aGPJhk9eVOa25zlcCq6uh+YYPoR/wDWpue9KPEEZbZ5Q2r1Vjj9DT47+wuP+WXlknkoRzTU0S4diM5zjHNJk1ZktAwzbtvUdsYP5d6q8htuOR1Hf8qtNMhqwue+efSqRcnWYUHTyH/mP8Ktk5qgh360zjAEduAOe7N/+v8AKqA0d3T3FAPPQUzPoDjtkflRnPSpsIeD1oz+lMBz0pM4Pp9adhjyaXI6UzPFGf8AIosIdmkJpueaXOCcnFIY4HijNNB49qTd270wY8nvSZPpTM5pSTzjriixI4+3NJmo0kEqbh03MB+DEf0p2aYxwPzD6iuS8eTBdKgj7uxP5AV1efmFcT8QGzHYp/sucfkKiexUNzB0SR0gkcED5ycdewNWdW8SXGlrbSiKOTexBDZAGPoah0iJfsALMw3OTjGfb+lR69psV+luDf20Eilionbbu6Dg4P8Ak10YdJtJjqXWow+Pp5I8PZQbfRXbisHVdYttTCG7sGBUffilAIH4jn8TWj/whWpyRh4Z7Ocf9MbkN/PFRTeCddCj/QHfPdGVv5GvQdKi9GYRq1Iu6Z1jsq/eyvsRikaQZwpz+IolSQyAbQxPTIOT+FNC7eG3q3XBXB/WuYwH5GM5GB1ppIOCTge9BIBIwAcdO9MLEsORwOQRn+VK4D925vlXd+FOIPA6g9M8U1I5SfLjRjlux5J/Su00jw4logkm8iS4Izh0Lrn0ABGcZHXPWonNRRpCnzXOastGu9Tk8mCDf2JPA/PpXM/EHwVPpVhbP56FXZzsjOQDgYOce/ava45TbxeS8gkC9SECj8AB0rmPiDbf2joUb8Hy3K49cjj/ANBrjnU59zrpw5Fc+cAyQSGNeSepNbEHMa/Ssi+t2tNVlRh3yPcVp2rgx8HkHFc8lodlOVy4CcYwcCkUlT6e1Kh9eKkEZbkHisbm9hRjHIpjKOOal2EjB6CmMgxzQmS00RMOwAOeKgddp44x6VYC4OQpIpQuVwfwqkTfUouDjOOlKjBQTmrTxDaQcflVaSI5LAfhVb7EtFhJ8j5sDjipYZ8dQcfjWc7NGmQB1Bxium0nRra+0y3uZ97NINxHGAQSPSrjByJckjKNwHyVG4DuvNOtHDySeoIFbtxpVvCpWKPGeST8zfmcmsryfIvQnZh39qv2fKRz3On0qST7MypI6N6qcf561fFxc4y9xMSD3kPFZelbhbnBI5rQnnErB9oVjw2KCHuWo9UvIxtW4Yj/AGuauQ6/epyVt3P+3EKxQe9SrigLnQQ+K7mNvmt4Tn/nmSlbNvrlvqqfMzRSrwRx/Xg/pXBliDgVNC7KwdXZGU8FTyPxqk9RNXR1eo3d5YuMmKaBuBIoxj2I7GsZddvo9QmmCwmKTbuB4I2jjjtVqz1tAnk3luGRuGZRuyPcE/yqO/0qGSI3emkPGvLIG3AD1Hcj2NdEJxejOepCS1QsniK5KgRpGjAD+f8A+ulXxBcHqRyePlFYTAqu44A9TTcsXwVIPpgg/wCFdHJG1zm55XsdCPEU6kBkUj3X/wCvTj4llAH7lD6jBA/9CrnGLBsEYI9TzRux3pqnEPaS7nTjxHkZ2YPcDNH/AAkKkZHlj2IauZLORnP4UbmAyT27UnSQ/aM6lfEIwdwjxjpvIpRr8YJ+RMr6S8H9K5VpeMZHI7igMSMkj8KXskHtGdcuvWzcsoB9nXn88VMusW79Q4+oB/rXF7yB3weuDgUhZf4Sp9MAUvZDVRndDUrc93x/uGlW+ik4jLFuwKHnjjtXBhgSQCBTiQ3B2lT1zxSdJD9rrsdxDPBBH5JlAZCVw3BPNO+32qsFNxHk+nOPyrhxLInAlYAY+62KX7VKP+Wj/wDfRFJUvMftPI7n7XbkjE8WM/3xXDePLlXu7RUZWAjJyDkH5iKUXtyDjzHz9ayvFMpe7tlZiSluvB9Tk/1rGrDlNaL5mXreFrextFkBGYhIDjqCSf6mub8YSCM2QVmVgJMYP+7XXXm8NbxsSBDbxRAf7qAf5+tcV4xcC4teP4W/pXTg1eogrStE55b2TzSzOQw6HOTV+DVryFSYrqZfpIayGXvmlV2AxnIr2fZx7HFzNHqAQAkfajtPG05I/RSKkSWMYKyMnc/KcD6niuij8N3Uy7o57QkDhQGfj6oDW5ZeHbSxCPfbGfIIjVCFPud3J/SvDlVitjojSkzA0/RbvUApa4kRcnYyqSz59BnBH41sReDA/EhmfjGWmVW/HAbH51uXOo2+mWzSYVeOg4z7e/8ASuYfxdNcXSp5SrH0HOSa5ZVmzojSS0Nqw8O6dZ3IJcvKpzh2zj6fKKjl1Rn1lrdGxHBGZDg46cAce5p+nzyBJZxy38I965231CCHxI0NzKIo5ozEZT0Uk8E/kB+NZuTluapJG2mqQuSpkUMW79DUl5GuoaPcW4ZG3ocEdj1GPxGPxrndTsL6ydhJETFniQDIP49P1pdLuvKPzBgBzjPH4Uge1jyjxdo7lvtkQYtGCGQDqvr/AFrEsJd2QQN31r1bWraOS6myvyOxbHpntXmuq6YNH1AeWCIJeR7N6Up7GlJ2epbiXcOasBe3aqlvKCBg1aRsmuV9jsQ8IMYAprqCR2xUgB5yKCBjpR1BlUggjjrUirwBipAo9OaXHHIqxOJC0eWPNQSpzz2q5gEY6e9QSpls8DHWmnYloz7nAiG7jkDFd3oNv5eh2Q6ExBj+OT/WuOW1+23MFomQZHHQdu5r0aOJYo1RBhVUKB9OK68OupzVmUbmPLVz92oOpx+uD+HNdNckqSa56SPzdT3YJ2jn35/+tWs9jKJr2CFbYc1Iw5Knv0p0CbECg9B0pkgIOfSuYvqPXmpVPGajTBOQakJ6UCaInIzxUkRB9ajOMGiJscUwsWc4bOOlWLO9msn3Qtg+nY1W6jrQDwapMhmpPZHUozd227zl/wBYgIGf7w+o/WsnylXKhUVh95EGD/Sr9jfzWgcRu6oxBO0kFSAQCPzp2oql28L2sqSNsIlyOR6dep/OumnU6Mwq07rmRmIi5bABwM4HJpPMRm2goAQTgHP4fWlniMJGQy5zhgx5PpkfSm7P3YbzAAcE7nA/mf6V0J6HKwVNuMdTyBTlTcgJOPaowd6f69VP+11/p/Wnho4fleWIKeN3OB9TTuNrQUxuCq8Y653H+lNVCRhpCOO1SbopFwk8TDHUSD+eaa0aCMHzVAP8TE/0BobEkNKjeu53U46Y4o8pB8wY+oxx+lRoHLbI2yWPBzyfw/8A1VPEBbKTI5UNztKDj8yD+lNCsNZDuGBuz0wDTXA3jI+UccmlaUuMbtyngbQDn64HFOj84twwz1Pyj8ulJgJ5QfHJx6rSMoU/MxwPU4J/CpuRuJRCT6ttP5YpHiAI8xBk9cfNj8abYyIRtIQA5UgelZniAh9dkiHRAqfkuK6C2ijuLiFcsFLgFlwQBkZOQT61z7A33i5VOP3l0OM9BmuXEO7OrDq12buot/xMrlR0R2UY9jXC+NSRe2q56Qk/+PH/AArtpmElw7nq5Lfnz/WuG8Z4bVYACDtgA+vzMf611YJfvEKs/dOb3EcHGKfkY4xxTCMqKUA+leyjidj6dv8AxWsKEQIue+Tmud/t2ae43yEH61hy3DOMHsag3NyQa+RPWiamr6obpgin5R1zWTHM0UyyAcg0EnrTCvzZ549KWg2dFD4smtoCgtkZuxz0rkNTuJJ7jzScE+narbYqpcoW9aYi5o2uXllxBMUI4ZRyrD3B4NbC67HLnzbC3BbOXiBjyfpnb+lcXzGTtyDnNaFrdeaSGOHHB9xT0sGpv3WJXLAFQR3Of5gfyrC1XTYr62MUmR3DenvW9EjPbxy4ypyuffrUE8W6s762ZovI8ze3k065aCQ5xyrf3h61ajkB78V0OtaNHfBXyUkXO1sfz9q5G4SfT5RHMOD91h0Ye3rWcqavdG8Kl9GbUbbh1pxTPcY7VnWd2GBG6tFCCvUVm00bIUrjtketDJxnv71IQBjt7VFIwyC3NC1J6jDjHX86gkXOSCcgVMUwCd27J69QKuadpr6jKY4xwOWbqFHvVxg29CZSSWpb8L2iSPLeMCSuY0+p6n8jiumb5AWbqe1JaWcNlbrbwqRGuTk9/Un6nNV724C8AV6dOChHU4Zy5noVb2XYCWKgEck9qztPU3EjTHgZwKbdPJdzCLlU6tWhaxCKADGD71hUlfYqKa3LAOckc1BMTjINTjgHiq0xwM8AnjFZWKJLY7kbJ5B/pT3bJ4qC1YhGzwM/0pQ3z9e9NoZLj5aauA1PA4PWmAEHGKXUlFhTuGKVqYp4p5ORT2FYcmM5PpVmOfyxGVQBgSc56/h+FVVbJxT84HWqUmJpXJJm35kghmxklxGd233xngcmlV34JZ+BgFQePwNRoxRuASOc5OB/9amK09qTtklmifkeY2Sp9K66VS+jOSrT6oJhmTZIjsG5JLAZ/I01IYEJVRGCepwxI/T+tSq0pTiMu5PCg9PcDP8AQ07a3lEyxmMj1iDD9RmtTGxA8RkI2XDsAc/d5/lQGCYVtxA4yEJH4nFRy7AQj3bEnkI23+QWnxxIMMLhNo6CRwP5L/OmA0z4OfLJGR1K9qhuLlImDCMsTwB8oP6c4/GrkipEv3pSWPG1iw6H2zUM1ruXJBJBxnYB6epB/SmJjba6ExJkjKAHgB+v4dalklxgYYjvuDcVAC8MR3bgMkDjOf505otyDc5bnkPxgex/xpsCwjKyhtwGB3zn9aY+2MhUZcMOgzxTIlbOY58IDjgKfzyM/lQ8jQOMuWD8coMGk1cLF3T4VN5G+7O0EkdcgAn+lc5pASfxCsjZxl5D9ACa37KVVSafAVI7dmUjPQ/L6e/rWH4eT/iYXLnjbC4A9CeB/OuOtrI7aOkDV2rubacfUVwXjCQnX3UYISNB+gP9f0rvW4Zuc9eO1edeIpPN1++J7SlP++Rj+ld+BjeZliNI2Mle4Ip4x05ppA65oHPWvX2OPSx6vkDiigge+aCDXyDPYDbxzTGHYU7nPNNY4NIBmOKjZM9al4zzTHA9eKYik8Iz6VGIykgZTggj8atyL+VVmABznrRcaOw8KSQ31tcWUyDd94dz0wcfp+VJqFq1lcNA+W7qcdR6/wCfeuXs72ayuVmgcpIh+Ug9K3rvxJDqcS/aYSlwnSRDwfwqWUVJ1+asbUNMhu4vJmBVGO4MOSp/+vWo90m3Oc8dBWdLqRDbPI2jsSf8MVWlg1OZuvD95ZvutFaSMDlWI3f/AF6S3ujyrgqRzg8Y/CutheO5XDBfoajn0yCZw0sKuR0OSP5VPKmXGo1uYBu4wobOc1HGl5qM4isrZ5WJ52jp9T26etdRBY6dEedOiPTlmdv0Jx+ddBaOoj2xxxxJ/diUKv6VpGimEqy6HNaf4QmdQ+oT7CefLiIJ6evb8M109tZQWUHk28QRevTkn1NWvlHJIz+dZ1/qKQJhRuZugB/nXRCEYnPKbZDfXfkKVQAkisv95LywJx2FSBZJSXc/Mam2EDGTnFZzqp6IqMbalS3t23yOxAJ/M1cx8vApkYG8qMccH64z/WpNpwOCM+1Y3uUIeBgCqtxyMHOPWrWMD2qGSPcRk4H86AIohstx6nnGKdGCXpXOMccCliX5s80AWQPlNQng9KmOMVGRk0A2ORuDwKf/AA8Gogfmx6VJwO3FO1xApIPWpc+hqBeXPOSKkBJJoWgmh+eM4FPiKONsi5Xrn0/Go1OeMfSgHDVSbTuhNJqzLcVm6RNmyvVduhQqwx+YNNeCTq1tf56E+SG/UNmtK0vJG0+TGXeEbigbkqOc9OoqBfElsxOJwx7sssR/qD+dae2l0M/ZRKMcaWpcq97HnjJtD/8AXqCe5Bfd9oVjjA3wMrfqAK2B4gt2yd0xP+yiH+TVIviCAADZKQR3ib+hpquxeyic0J4GbMt3DkdBs2/rUkDxbiftKDIznzv6ZxW+2u6aRiQ4/wB+2/xqNtU0Fx+8S3P+/a4xV/WES8PqZTRxucAxPgdQc4/UVG0XlcDcucH7xA9v85rUafwzJ96OywOuUZf5A0C38NOfk+xjI6rcOn9KaxMQ+rmHLHcxk+Vl9x+YMB/PFLE93JLtcFQv8XGOBj0FbyaTo8mGhuD/ANs75Tj86lOj2pBK3eonj/nujfyJo+sRD6vIx3H2fR71i+5jGkYPflgeuPaszw8v7u+cBSMKAffdn+ldb/YKEYF5eqAed0YYfoDnrTRocUabUunAJGf9Hxn64x/k1lOcZSvc0hFxVjGSMmRV75C59ycf415bqMpm1G6mA4eVm+gJJr2ldEjI/wCQhEp94+/r96sub4faZcfODbbs/eXfx/48RXZhcRCm25PcyqwlLY8gyc9KAMcivU5fhjCwBSVefSQj+YNVJPhjIG+Qsf8Atso/9lrvWOpPqc7oSXQ0QcjNBA9aauAacCM184z0hCvNMZc1KQDTcYPAouDRCyHNRNkdat4zTGjB601IViqzVXk5HbmrrwDsahe25zk4oumPUpkDsaUCpzb4/izSCIiiwaiKcHnFK8Sy/eHPrThHinYNKxVyt5DRH5eR9KkVpUGFkZfxqyoGKURqc0rNBcrrdTo+Hcn3I61dgvccE7cHoOlIEVhyM/hVW4ARNqtyTgfWqjJpg7NF+bUJRGAmDu6FT1FQRws7eZKctUVnbmIbnbeTyCR936VcVyfuqT71pKbaI5VcdkIKjaTJ5pkkqIf3kirntkE/pURu4QMIHb8MVnYfkSojCYuTu3Y429OMVB9rmM7okClVOMk9amSdpeAm0fWnpEA2cDP0oGCtIRyqilEZzuZsmngEDGOKRzwRQBDJy2afH61H948/WpouhpiuSN0pmKfTO9ACHPtSqcGmk05TQIToSciljbPPrSSj5Cc1FE2WwOgpN6jRcUjfSnrUYPzA0/g073DqT207wTLIn3lORSXluuoSNLbQxI55ZNgP4jjpUKn1p8cm1s4B7YPQ+xq6crMicbrQzzpiM7h44eOp24/oKim0cCIyIGKDqY4un5CtpbcMPMgQAE4YDgr64AXOKrvAkcuWnMUmecbQy/yNdqUZLY4pOUXZsxhDEgAD3ERPq5TP6HNSgxL8rNfDI4dTvX+f9K1ZJcIWTUJmPGRECc/UYx+dQG6hf5ZLidiON5t1wPy5/Sh0432HzyITYuuHSR3H8WULMPw2E0iNHET51tGwB4EqnP5Ag/pUqum7Ec5IH8JQAD8xT3tWnTKKr+pMQbH5dqXsodh+1kupWeBLjBS0jTtxlh+ZJxT0s5o/9XheP4eaQWcscgPyAr0BBH9auIHfJkwT6vKCv6nNS6EH0KVeXchnvLrS4EkfB3OwChmUcAc8H3qxZ+ILqWLcilcHAw/+P1rM8R/u4LSNSpUlnGOmOB/So9KXNnGx6kHr9TXKoLnsdfM+U3LjxNc2ds9xNHujjGWO8A9cf3feqC/EexbHmWxPbO7J/lWT4pl8vw+6gf6yVIz245b+aiuAJJ68GvQw+ChUhdnNPEODsevx+P8ASCeYyhPXCdatR+N9Ic/LK6+2XH9a8UZiBzTGcnA4P4U5ZfHowWIb3R6534ozg9Rmkz2owMV5PQ6h+SQOaWoxxwKdmiwhefSjGetJn3NL+OaRS2G4xQV4z3p1BoAjKgjoKb5Yz0qXtRjGM07jIvKGe1BjzzUpwe1GOKQaEQQL070u2pMZ7U3HI60BoMPyn2qmjG5vWH8MI/WrF2xjiZh1AP8AKm6bb+XZ7t2WkJcn607iY4yuGKoox03MM5pjRSSYLzMaueWPQe1IyYPNJq49iklmo6D8hip1tsVOBt7U4dKAuRrGF+tS4o46mhvUYosIaxwKgdiRUjtn61GATxVCYigfnU6AdKYqgH1qVenIwadwDOBxTN3PNOzTe9IYh6470q5pKUZphYcwz1xVbBik68VPn61BPjctDEicNlQR1qUcrnFQIcrwMVKjErSQMU8U9fz+tNPPWlHTFMQ4qHRo3XcjjDD1FQXGlnSDA32ubbIgkTuMH14P8qn7da07JkuoDBcOAiY2H7xH0Hce1b052MalNM59CJzkLFNj1JXP5gGiZ5Q/y2rEdz5wb+hNXLkXdtdNAEIjxkMgVldexHH6VCYWZgN8QJ6B41JP09fwrr3OXVbkts8WwedHNFkZ3bVYH6Ec1PJdWsCqxYEHuB8w/EDP4VmPayM5+aME/eyioce3FSDzIU2uYDxww28fiOPzotcXMWxf2z5WF4Cx6xvIAT+dDSXBK4sIwOxGTx7YSqJuoMkSNbsONxdkwfxzg/gaG1KwtlzAfmPP7ohR+BpN2BIr+JmxJYRsoXbDnaOgy5//AF1JYoFtLfof3Y57dKq+LJP+JgqkEmO3QHJyc7QSPzOK0UjEKCHPCKEJ7cCuKD95noNaJHN+Np/LtbSHrvdnP4AD/wBmri2YEE9K6zxm2bmzi/6ZlvzOP/Za5Xy8DLd697CRfskefVlHm1IgCxzUqxU9VUDHenfKBXYqS3ZjKo+h6ketAPp2oOetMDYNfHXPYsSHHWkGRyaBz0pTlhmqQBnNKBTRxShu1ADqOB/hQKMYNFgQuKTFHejv2pFCdaXGBxRznpS0AJ9KQ0uKQ0CKl8CYGPsat264tox6KKgul3Qv9DViP/VJ/uigB+30NBGfrR06Ud/eiwCClopQOcYosAdqiYipCahdqYDGNCj3NJjJ609RxxTFYeox+NPAxTM9Mc/0p+eKQAcVGR+VPOM009+9AhAoowfWgdKXGaCr6B+VQTEkipTkA1EfmA9aT2BBGSVqZDg+lRR8Eg8U4H5qSY2ixnP0o6e9NB4pc8YqxDskjpUkUrwtuQ7T35qIHjGaUN2oTsJo2rJNP1FwlyksU7sAskRyvTuP/r/Ss/VdNTTp2ieNJ0YZV97YYZ/iHOCO4I9KvWGvy2QCm3glwOGZBkD6/jVpNTg1G88qeEBZSclTyhHQg/pg+tbwqtMxqU1JHLGAP920ZR/0zGQP0pp09sjy7YY98A/410GrxnTJowh82ORdyMBjvggjsRWW2rQp1YfRgRXWpKWxyOLWhAlg5wXtYD74FSxaNFIeYLfk8EYbHI/DP4UkuqL1hUyLxuY4AH8zU+mXH27VUNwzRQR5dp5JWCogGchRjOfw61E37rLgtbGBq1rcajrTyQwSvbpKA8iISqKGxycY6VfKSO5YdDzwcgVY1C7t40WG1uJjZszmV933iW3ZK5wDgAdfxrMZ7MyM29AzEnksrfoOeK5aSW7OmpNxdjkvFv2w6uZBbS+QkaqjlGweCeuOOSfyFc8Lsk/dB+hzXpE2qWcELokkswAyFb5Vc5HHPfGe3apbaHRtSQs0Ue7oyyLj+lenSxTjFRRxySlq0eYm6BIyvfoKkE8bDkkfhXoMvgvSJJC2G5OcIdo/pUDeBNJbG25nix1w6nP5it44uRDhE3twxTMYyRTs4NNbrxXziPVAHtTwTn6VGvTOaeCTVXExx5zTeg60uadjPNMBBx3FKCeuaaeelGSR0piHdeaUYPPpTM+9OXP4VLKFBzTgOtNHNKfamAhxnpk0H8qMdweaQ554zQAxwHRlPcEfnx/WpQTgADgACmg56U7LY70ALzjsPoKTBAyTRycdaMe1FgFH3c0pJAOKaMDIpjSYpWAGb1qPPpS5JNLtyaoQgB69alUUKuBTs8UrghAAeO9KRS9sUlADTnHFNAPWnnHNN70DQmc80GlHPFB4HNFwsRuSUZR3GKhhkDs49Gwfyp8x/dE/kT2rLS4MOpTA5xJtcD0OMH+VQ2UjUXhj/jTs/PTFdTyCKUtz1FCAs5yPUUmfSolfAp4NUS1qSE80gOKaDQCM80wJM+opyOVZWQ4II6VXL5+gpk1zHb27yyMERVyWp3E1qar6p55jgllXeFLAMCSecHGD6cU1nO7cBErfU/1rza91ua81IXMX7pY/lTB5xknn8z+ldRpmpyX8JkEvluPvrnr+tdVGavynPXoy+JGzcXN8gwYyV9cZx+v8qgtdRvbNrpohCgkt3RsqCCpGcFSPUDsTUZmmhVmYLgcEhev6UyS8MumXjsgUrEAOe5IH8jWtRrlZjS+JGLLfS3a+VHBErgctGNuQPX/Pes2WGdOSSwP+e9aOkLGzys65AAXuAMnP9K1JLWOeMrwpI4Oen41lSp3jcutL37HKlnTG5SeCOnPt29afHcTKARtGOjY5H44rbn092kBjUBT7YxTBpruq5jUnjjr+XFV7NmfMrFO31W5i3BsyZ5w3f361owalFOpE1uR3JRj/AFpqaewbaIzt6AAjn8OKsrpxZAfKOR2OQR+taxi0S2i+T9KYeeM0ueKAK8k9OwgGKd3pCcGgn2FMVhacDUeM96cB71VwsPo520Dk4o6nnpTuJoTA7HNJ0NOwKTFMBR6ilB5poGKM5oEO79qXOKZjPSgtjigdx2cdOKePfNRbjkUhlI6UWHcnyPWmM3oahLkjrSZJoUe4XHFyaTrRjBp4WmLoIBjtT8Uc0uD3xSAcMYoAoFKMetIdg59KPxpe+KCKBjce9J0NO+tNPtSYWG44yKQn1FKPl/rUU8mwccselJ2Ao314qSJCRy+cCsa6nMd8snHGBVnVlAEBL7ZDJkEn2I/rWZOS3ztjPYVLGjo7WQMAR3qyTn0rK0mYugHXFaZ4PFNAx6njmpA3HFQq3NPDDjOKYh+cDJqNpD24qKafGeQAOpNY+pa5DYDDAs55CDr+PpT0GrmzLPHBC00jBEUZJNcZrGsPqjqF+WFOVH94+pqnf65PqO0MBHGpyEU9/c1VUgDP9alsuMSzbRebJgH/APVXaWelyWIjaORijj5w34H1+tYHhizW+1aKFz8meffjOK9JubVQucjn0rGVVxehuqakrMxh5G3DwzAkcsg3gH3qpqjxJpUvknKyEKTjB9uDVuSU2k4OcL3+lUtcuUntouhO75uMcYP9a7lWU6djgnQcKmmxV0aVo0uGy6g7eVB6jP8AjV57+BlZiC49TGT/AEqjpD2kKOZ0Pmh+GUHjHatCW7sZFXdKrbc4DI3Tqe1dFJ2gcla3PoQx3MRB2RBQD0q4jqTtKFV9yKzJLsM7vaWsLKoyNrt+gY1LE1uOJbN4snO4SA/j2IrXm1M+U1QY1OSxA78g/wBaaRDIcrO3AxwoqKKC3YgRsxXuc4wfwGP1qzHAqI3lh+OxJI/DmquJor27box6gU9yB9KW0gMoyvf2qSa3aNsNn8q8hQdrtHp86vYgHI60oGeQaeAMdO1NIwPSktwE5pckdqbn060ZPrVAPB7805cE81FuxxT1Oe1AEnOcUoHBpgJ65py8mmAFfSk2/Snj3puBnNNMGhvQ8U3k96lxmm4xTuSNweeKaFz19akx70Y5o0HYZtpQuKftwO1GCBTuFhMA04AigA07p1pNghMUoFFGTSYxccUY44ox70UrgLSGlOKSlcoTOTgdKCMc5pCaaT6dKTARmCgmqrksd5PsKncZAzUDHLcAYFHQDG1mAzeSAeF3E/pWdHbzXL+SuGIGR6n2rdvIw7rnqB2qFYSsqyKASGB6d/ypJXYr2WhDp6PbSFGyD6Vqb/Q1PqNiBCl1ABu4JXPJUjg/yH41npMSPUVUouLFGSkmWt2OTSFt446CoN+5sE1YghklbCID9elNJvZFOyWpl6tfCwtw2zdIxwgI/UiuDuJpJ5zJKxZ25JJzXo19oL6ipWUlSoO0xjdj69K4XU9Omsbx4pk2spz+HaqnSlHUUKkZaFSLHepwcdBUKjA+tSDOOtZG2x0HhXUFsNZglYKVJ2tnsCMf1r1S72lWAzt5x7j1rxKFisikE5Fer6FqB1XREkfPmQ/uiR/Fxkfpn8qwqRadzog1axlao+3r6d6xnunliWIqDsJxkdQea2dVUsDuU4rn5C0M4wQMjoTV0pdDOsro1dMIFm3mYzvZ8MxAHGPT2qw9sWZmWRWYHGN6Y/LFW9FitYLSBpoFlJGcA7cktkA4GTxmldxG3MYIB64xkfnXrw+FHjVPiKsUHlfdUoc8mPByPTgf1q35yK2TM6HGMeVwfzJNMkuLfBLWpIGScgZAGOcE9OalAsi5AiTgdQ3+FXoRrces1tj5blc9v3fQfjmrQdWYgTb/AMMfyqFBaADbEuSOvNOWRW4XKe4AouLUsvElqAhMiIOykED8qgZ4JUBifeM8kMvy/XnNNsbe41KZkiuLZCvJ3uCTwTwu459On5Vcg1S4tLOW2bUXjdWx5MYBA6dcnr9PzrnlaSsjWN0+ZmZN8pwOff8A/VUBDH1q6H+0BmdHDA8lgBn34NMchOg+lcjVnY7E7oq4K+9HNSFj12j8qRSp7EUrjsGO9LS7QehpMGgSFBp2eKbige9GwySlpo604ZpsA6d6DntRxQDihMA/CkxmlzSk8UwEHSlHzUY96WgAHFFGDS4pAJigc0ucUYJ7YoGkBxnJNFIc5pRU3CwhNGcGg/Smk80ABOaYx7ZxTsYAqKQgcDFA7kbt2pEGTkikAJPSpMbVNNoLlZwDIc8GmlBnkD64qdkB571G+QhOOgpLcLF60itZrfJkkRgdpUZ/TilfRY2YMjSHPsB+lZmmXr229AFIbHVc/StsXE4GRDj1G4D+Yruhy1I3ZwzcqciFNIjUfMHz6EipfsMi/ccqOygcfpU0V1cn5fIbnrlgcVbVmA/fFE9ATVqKjqkQ5yluzO+xy9WZM+4x/Sua8XaS0tit4rbngwGxknaTjv6da7bzAw6qce9U72E3MEkTBSkilSBjoeOpH1/Kqn7ysEPddzxojDHufcUhPIzVzVLKTT76W1fOY2xkdD7/AI9apZ5xXmNNOx6sXdXJVbBx2rtvAt4wkuLUt8rjzBz3H/7X6VwysRnAzXW+CpQurlCw+dSFPqcH/P5VE1oawaudFqpInJ4z2x3rnNSG2RHU/X2NdbeI0zlgoK4+6RnGa5rVouCqqcgdO9ZQfvFzV0bem34eyiBSTAUDKgHpx3Ip8rwyNiOMjscgr/IGszw/eLFatG6qSDkE8cD/AOuTW0GWXJEqxgdVOGI/nXtU5LlPDqx5ZFKSzjb7oKZ7cEVXGlqjk5OMYwrY5+uDWzKhLnyXTGecDI98dKie3nIIZYyRx3FXddSNehjW+nT+eAZhGhPLZZsD6Dn9PrV+SF4JttvO7pj7zIFI/DJ/T9KLi2u1+5J+BYkfgT/jTdmoBMMyHPfJo03QdCwkNleEWzWEenzRqMma7aMsSPR/6YxXPXGlyKWaOY5PGQ27B9Aa1bOLUZ1ZTNPJF95guATx64z/ADq5JBIkMFuRNaohyGeNS/4Hg4rm5TW7auV9EuZlh+wy+aRgshY5C45wOvUfrn1q63JyO1WI/DWp2yLeS7Y4uu+VlGR7AnNJIg3sVYMDz8vQVjVilqbU5O2pVIB5JphFTvxjNRMueQaxTNrkfIHFODc8igjFNxTAkycYFGT60wHHejgmgCQE96cGOD0qMHtSg8U0xpDx+H5U7Ipq+tOBzRcQo46Uc0ZpCM0wFHHUUuM/WmjINOA9TQAADNKBnNLgZPpSAYpNjCkp2KQdM0gE/CjjB4pcZ70Y5pAIDxxTeh5FOIxTScCgdxrnPSoG5xTy3pSbc96aRN2IFxzSt1A/GnBc55phO5jz06UNjQvBqGZf3TjHBFT9qgmI2f73+f6UlqNGZEdvzYB5z+NbsepTSIoZFbtk4xmsOMDLAevFadgQxMeDzyB2rfDyalZmGIipRuXpL8qAokCtjGEOf5VA7NICzLO57kuP6mkkjUZzGFHcAY/lVYiNf9Wh4PdjXoJdjg1JS1wx2x25465kI/pioZ5L1eCjbvQEHNLvuME7yAeg35/pTczkYG1z6YH9al6FXOZ8RWs8pFzKo3AbW5H4HiucKkHHb3r0eZWmiaN4cgjBAAA/Q1xWraa2nXflOOCMr7iuKvTt7x34eomuVmYuM5GK6XwhDLc6zAIhjawJwcfKOTXNEA8dBXffDMxvfXcW0eaYsxkjp6/SuWfwnXD4jqdTeOAsqj/61cjrU4nVWwOMjIrodcdknYE8jt61zN4gaBuuev0rnitbm72sW/DMkSrOk1ms5LDlnI28cnjrkVpzM4YtDawxj0Hp+JNc1pNwIJ2LyFMjG4HpW8u29BkgmVwM5zj+or16DThY8fEK0xtzc3kq7BKqDoNpwevY8YrNMlwrfNcznB6J0H61rC32oRKjsRxwygD8l5qi0khcolvjHU5HH9a2aRhdpCw6i0QbeLmRcdTgAn+dSf2uoO4WbEDuXJ/lVQlDuUqc56g1BJsbAKFiOhJ6UarYVz//2Q=='
        #     snapstatus['metadata']['artData']['extension'] = 'jpg'
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
