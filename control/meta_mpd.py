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
# Authors: Jean-Philippe Braun <eon@patapon.info>,
#          Mantas Mikulėnas <grawity@gmail.com>
# Based on mpDris from: Erik Karlsson <pilo@ayeon.org>
# Some bits taken from quodlibet mpris plugin by <christoph.reiter@gmx.at>


# Dependencies:
# - python-mpd2
# - musicbrainzngs

from configparser import ConfigParser
import os
import sys
import socket
import getopt
import mpd
from dbus.mainloop.glib import DBusGMainLoop
import logging
import gettext
import time
import base64
import musicbrainzngs
import requests

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


# MPD to Snapcast tag mapping: <mpd tag>: [<snapcast tag>, <type>, <is list?>]
tag_mapping = {
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
    'file': ['file', str, False],
    'url': ['url', str, False],
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
downloaded_covers = ['~/.covers/%s-%s.jpg']


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

        self._status = {
            'state': None,
            'volume': None,
            'random': None,
            'repeat': None,
        }
        self._metadata = {}
        self._temp_song_url = None
        self._temp_cover = None
        self._position = 0
        self._time = 0

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
            # Reset error counter
            self._errors = 0

            self.timer_callback()
            self.idle_enter()
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
        self._temp_song_url = None
        if self._temp_cover:
            self._temp_cover.close()
            self._temp_cover = None

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

    def update_metadata(self):
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
                logger.warning(f'tag "{key}" not supported')
            except (ValueError, TypeError):
                logger.warning("Can't cast value %r to %s" %
                               (value, tag_mapping[key][1]))

                # Stream: populate some missings tags with stream's name
        logger.debug(f'snapcast meta: {snapmeta}')

        # Hack for web radio:
        # "name" and "title" are set, but not "album" and not "artist"
        # where
        #  - "name" containts the name of the radio station
        #  - "title" has the format "<artist> - <title>"
        # {'file': 'http://wdr-wdr2-aachenundregion.icecast.wdr.de/wdr/wdr2/aachenundregion/mp3/128/stream.mp3', 'title': 'Johannes Oerding - An guten Tagen', 'name': 'WDR 2 Aachen und die Region aktuell, Westdeutscher Rundfunk Koeln', 'trackId': '1'}

        if 'title' in snapmeta and 'name' in snapmeta and 'file' in snapmeta and not 'album' in snapmeta and not 'artist' in snapmeta:
            if snapmeta['file'].find('http') == 0:
                fields = snapmeta['title'].split(' - ', 1)
                if len(fields) == 2:
                    snapmeta['artist'] = [fields[0]]
                    snapmeta['title'] = fields[1]

        album_key = 'musicbrainzAlbumId'
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
                        snapmeta['artUrl'] = image["thumbnails"]["small"]
                        logger.debug(
                            f'{snapmeta["artUrl"]} is an approved front image')
                        break

        except musicbrainzngs.musicbrainz.ResponseError as e:
            logger.error(
                f'Error while getting cover for {snapmeta[album_key]}: {e}')

        logger.info(f'Snapmeta: {snapmeta}')
        requests.post(f'http://{params["snapcast-host"]}:{params["snapcast-port"]}/jsonrpc', json={
                      "id": 4, "jsonrpc": "2.0", "method": "Stream.SetMeta", "params": {"id": params['stream'], "meta": snapmeta}})

    def _update_properties(self, force=False):
        old_status = self._status
        old_position = self._position
        old_time = self._time
        self._currentsong = self.client.currentsong()
        new_status = self.client.status()
        logger.info(f'new status: {new_status}')
        self._time = new_time = int(time.time())

        if not new_status:
            logger.debug("_update_properties: failed to get new status")
            return

        self._status = new_status
        logger.debug("_update_properties: current song = %r" %
                     self._currentsong)
        logger.debug("_update_properties: current status = %r" % self._status)

        if 'elapsed' in new_status:
            new_position = float(new_status['elapsed'])
        elif 'time' in new_status:
            new_position = int(new_status['time'].split(':')[0])
        else:
            new_position = 0

        self._position = new_position

        # "player" subsystem

        if old_status['state'] != new_status['state']:
            logger.info('state changed')

        if not force:
            old_id = old_status.get('songid', None)
            new_id = new_status.get('songid', None)
            force = (old_id != new_id)

        if not force:
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
            old_meta = self._metadata.copy()
            self.update_metadata()

        # "mixer" subsystem
        if old_status.get('volume') != new_status.get('volume'):
            logger.info('volume changed')

        # "options" subsystem
        # also triggered if consume, crossfade or ReplayGain are updated

        if old_status['random'] != new_status['random']:
            logger.info('random changed')

        if (old_status['repeat'] != new_status['repeat']
                or old_status.get('single', 0) != new_status.get('single', 0)):
            logger.info('repeat changed')

        if ("nextsongid" in old_status) != ("nextsongid" in new_status):
            logger.info('nextsongid changed')

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
     --snapcast-host=ADDR   Set the mpd server address
     --snapcast-port=PORT   Set the TCP port
     --stream=ID            Set the stream id
     --command=CMD          Issue a command to MPD and exit

     -d, --debug            Run in debug mode
     -j, --use-journal      Log to systemd journal instead of stderr
     -v, --version          mpDris2 version

Report bugs to https://github.com/eonpatapon/mpDris2/issues""" % params)


if __name__ == '__main__':
    DBusGMainLoop(set_as_default=True)

    gettext.bindtextdomain('mpDris2', '@datadir@/locale')
    gettext.textdomain('mpDris2')

    log_format_stderr = '%(asctime)s %(module)s %(levelname)s: %(message)s'

    log_journal = False
    log_level = logging.INFO

    # Parse command line
    try:
        (opts, args) = getopt.getopt(sys.argv[1:], 'hdjv',
                                     ['help', 'mpd-host=', 'mpd-port=', 'snapcast-host=', 'snapcast-port=', 'stream=', 'command=', 'debug', 'use-journal', 'version'])
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
        elif opt in ['--command']:
            params['command'] = arg
        elif opt in ['-d', '--debug']:
            log_level = logging.DEBUG
        elif opt in ['-j', '--use-journal']:
            log_journal = True
        elif opt in ['-v', '--version']:
            v = __version__
            if __git_version__:
                v = __git_version__
            print("mpDris2 version %s" % v)
            sys.exit()

    if len(args) > 2:
        usage(params)
        sys.exit()

    logger = logging.getLogger('mpDris2')
    logger.propagate = False
    logger.setLevel(log_level)

    # Attempt to configure systemd journal logging, if enabled
    if log_journal:
        try:
            from systemd.journal import JournalHandler
            log_handler = JournalHandler(SYSLOG_IDENTIFIER='mpDris2')
        except ImportError:
            log_journal = False

    # Log to stderr if journal logging was not enabled, or if setup failed
    if not log_journal:
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

    if 'command' in params:
        try:
            cmd = params['command']
            if cmd not in ['next', 'previous', 'play', 'pause', 'playpause', 'stop']:
                logger.error(f'Command not supported: {cmd}')
                sys.exit(1)

            client = mpd.MPDClient()
            client.connect(params['mpd-host'], params['mpd-port'])
            if params['mpd-password']:
                client.password(params['mpd-password'])

            if cmd == 'next':
                client.next()
            elif cmd == 'previous':
                client.previous()
            elif cmd == 'play':
                client.play()
            elif cmd == 'pause':
                client.pause(1)
            elif cmd == 'playpause':
                if client.status()['state'] == 'play':
                    client.pause(1)
                else:
                    client.play()
            elif cmd == 'stop':
                client.stop()

            client.close()
            client.disconnect()
        except mpd.CommandError as e:
            logger.error(e)
            sys.exit(1)
        sys.exit(0)

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
