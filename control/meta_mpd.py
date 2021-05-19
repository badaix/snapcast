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


from configparser import ConfigParser
import os
import sys
import re
import shlex
import socket
import getopt
import mpd
from dbus.mainloop.glib import DBusGMainLoop
import logging
import gettext
import time
import tempfile
import base64
import musicbrainzngs
import requests

__version__ = "@version@"
__git_version__ = "@gitversion@"

musicbrainzngs.set_useragent(
    "python-musicbrainzngs-example",
    "0.1",
    "https://github.com/alastair/python-musicbrainzngs/",
)

try:
    import mutagen
except ImportError:
    mutagen = None

using_gi_glib = False

try:
    from gi.repository import GLib
    using_gi_glib = True
except ImportError:
    import glib as GLib


_ = gettext.gettext

identity = "Music Player Daemon"

params = {
    'progname': sys.argv[0],
    # Connection
    'host': None,
    'port': None,
    'password': None,
    # Library
    'music_dir': '',
    'cover_regex': None,
}

defaults = {
    # Connection
    'host': 'localhost',
    'port': 6600,
    'password': None,
    'bus_name': None,
    # Library
    'cover_regex': r'^(album|cover|\.?folder|front).*\.(gif|jpeg|jpg|png)$',
}


# MPD to Snapcast tag mapping
tag_mapping = {
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
            # GLib.timeout_add_seconds(5, self.my_connect)
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

            self.client.connect(self._params['host'], self._params['port'])
            if self._params['password']:
                try:
                    self.client.password(self._params['password'])
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
            self.reconnect()
            return False
        self._update_properties(force=False)
        if was_idle:
            self.idle_enter()
        return True

    def socket_callback(self, fd, event):
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

    def last_currentsong(self):
        return self._currentsong.copy()

    @property
    def metadata(self):
        return self._metadata

    def update_metadata(self):
        """
        Translate metadata returned by MPD to the MPRIS v2 syntax.
        http://www.freedesktop.org/wiki/Specifications/mpris-spec/metadata
        """

        mpd_meta = self.last_currentsong()
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
                logger.warn(f'tag "{key}" not supported')
            except (ValueError, TypeError):
                logger.warn("Can't cast value %r to %s" %
                            (value, tag_mapping[key][1]))

                # Stream: populate some missings tags with stream's name
        logger.debug(f'snapcast meta: {snapmeta}')

        album_key = 'musicbrainzAlbumId'
        try:
            if not album_key in snapmeta:
                mbartist = None
                mbrelease = None
                if 'title' in mpd_meta:
                    mbartist = mpd_meta['title']
                if 'album' in mpd_meta:
                    mbrelease = mpd_meta['album']

                if 'name' in mpd_meta and mbartist is not None and mbrelease is None:
                    fields = mbartist.split(' - ', 1)
                    if len(fields) == 2:
                        mbartist = fields[0]
                        mbrelease = fields[1]

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

        requests.post('http://127.0.0.1:1780/jsonrpc', json={"id": 4, "jsonrpc": "2.0", "method": "Stream.SetMeta", "params": {
            "id": "Spotify", "meta": snapmeta}})

    def find_cover(self, song_url):
        if song_url.startswith('file://'):
            song_path = song_url[7:]
            song_dir = os.path.dirname(song_path)

            # Try existing temporary file
            if self._temp_cover:
                if song_url == self._temp_song_url:
                    logger.debug("find_cover: Reusing old image at %r" %
                                 self._temp_cover.name)
                    return 'file://' + self._temp_cover.name
                else:
                    logger.debug(
                        "find_cover: Cleaning up old image at %r" % self._temp_cover.name)
                    self._temp_song_url = None
                    self._temp_cover.close()

            # Search for embedded cover art
            song = None
            if mutagen and os.path.exists(song_path):
                try:
                    song = mutagen.File(song_path)
                except mutagen.MutagenError as e:
                    logger.error("Can't extract covers from %r: %r" %
                                 (song_path, e))
            if song is not None:
                if song.tags:
                    # present but null for some file types
                    for tag in song.tags.keys():
                        if tag.startswith("APIC:"):
                            for pic in song.tags.getall(tag):
                                if pic.type == mutagen.id3.PictureType.COVER_FRONT:
                                    self._temp_song_url = song_url
                                    return self._create_temp_cover(pic)
                if hasattr(song, "pictures"):
                    # FLAC
                    for pic in song.pictures:
                        if pic.type == mutagen.id3.PictureType.COVER_FRONT:
                            self._temp_song_url = song_url
                            return self._create_temp_cover(pic)
                elif song.tags and 'metadata_block_picture' in song.tags:
                    # OGG
                    for b64_data in song.get("metadata_block_picture", []):
                        try:
                            data = base64.b64decode(b64_data)
                        except (TypeError, ValueError):
                            continue

                        try:
                            pic = mutagen.flac.Picture(data)
                        except mutagen.flac.error:
                            continue

                        if pic.type == mutagen.id3.PictureType.COVER_FRONT:
                            self._temp_song_url = song_url
                            return self._create_temp_cover(pic)

            # Look in song directory for common album cover files
            if os.path.exists(song_dir) and os.path.isdir(song_dir):
                for f in os.listdir(song_dir):
                    if self._params['cover_regex'].match(f):
                        return 'file://' + os.path.join(song_dir, f)

            # Search the shared cover directories
            if 'xesam:artist' in self._metadata and 'xesam:album' in self._metadata:
                artist = ",".join(self._metadata['xesam:artist'])
                album = self._metadata['xesam:album']
                for template in downloaded_covers:
                    f = os.path.expanduser(template % (artist, album))
                    if os.path.exists(f):
                        return 'file://' + f
        return None

    def _create_temp_cover(self, pic):
        """
        Create a temporary file containing pic, and return it's location
        """
        extension = {'image/jpeg': '.jpg',
                     'image/png': '.png',
                     'image/gif': '.gif'}

        self._temp_cover = tempfile.NamedTemporaryFile(
            prefix='cover-', suffix=extension.get(pic.mime, '.jpg'))
        self._temp_cover.write(pic.data)
        self._temp_cover.flush()
        logger.debug("find_cover: Storing embedded image at %r" %
                     self._temp_cover.name)
        return 'file://' + self._temp_cover.name

    def last_status(self):
        if time.time() - self._time >= 2:
            self.timer_callback()
        return self._status.copy()

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


def each_xdg_config(suffix):
    """
    Return each location matching XDG_CONFIG_DIRS/suffix in descending
    priority order.
    """
    config_home = os.environ.get('XDG_CONFIG_HOME',
                                 os.path.expanduser('~/.config'))
    config_dirs = os.environ.get('XDG_CONFIG_DIRS', '/etc/xdg').split(':')
    return ([os.path.join(config_home, suffix)] +
            [os.path.join(d, suffix) for d in config_dirs])


def open_first_xdg_config(suffix):
    """
    Try to open each location matching XDG_CONFIG_DIRS/suffix as a file.
    Return the first that can be opened successfully, or None.
    """
    for filename in each_xdg_config(suffix):
        try:
            f = open(filename, 'r')
        except IOError:
            pass
        else:
            return f
    else:
        return None


def find_music_dir():
    if 'XDG_MUSIC_DIR' in os.environ:
        return os.environ['XDG_MUSIC_DIR']

    conf = open_first_xdg_config('user-dirs.dirs')
    if conf is not None:
        for line in conf:
            if not line.startswith('XDG_MUSIC_DIR='):
                continue
            # use shlex to handle "shell escaping"
            path = shlex.split(line[14:])[0]
            if path.startswith('$HOME/'):
                return os.path.expanduser('~' + path[5:])
            elif path.startswith('/'):
                return path
            else:
                # other forms are not supported
                break

    paths = '~/Music', '~/music'
    for path in map(os.path.expanduser, paths):
        if os.path.isdir(path):
            return path

    return None


def usage(params):
    print("""\
Usage: %(progname)s [OPTION]...

     -c, --config=PATH      Read a custom configuration file

     -h, --host=ADDR        Set the mpd server address
         --port=PORT        Set the TCP port
         --music-dir=PATH   Set the music library path

     -d, --debug            Run in debug mode
     -j, --use-journal      Log to systemd journal instead of stderr
     -v, --version          mpDris2 version

Environment variables MPD_HOST and MPD_PORT can be used.

Report bugs to https://github.com/eonpatapon/mpDris2/issues""" % params)


if __name__ == '__main__':
    DBusGMainLoop(set_as_default=True)

    gettext.bindtextdomain('mpDris2', '@datadir@/locale')
    gettext.textdomain('mpDris2')

    log_format_stderr = '%(asctime)s %(module)s %(levelname)s: %(message)s'

    log_journal = False
    log_level = logging.INFO
    config_file = None
    music_dir = None

    # Parse command line
    try:
        (opts, args) = getopt.getopt(sys.argv[1:], 'c:dh:jp:v',
                                     ['help', 'config=',
                                      'debug', 'host=', 'music-dir=',
                                      'use-journal', 'path=', 'port=',
                                      'version'])
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
        elif opt in ['-c', '--config']:
            config_file = arg
        elif opt in ['-d', '--debug']:
            log_level = logging.DEBUG
        elif opt in ['-h', '--host']:
            params['host'] = arg
        elif opt in ['-j', '--use-journal']:
            log_journal = True
        elif opt in ['-p', '--path', '--music-dir']:
            music_dir = arg
        elif opt in ['--port']:
            params['port'] = int(arg)
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

    # Pick up the server address (argv -> environment -> config)
    for arg in args[:2]:
        if arg.isdigit():
            params['port'] = arg
        else:
            params['host'] = arg

    if not params['host']:
        if 'MPD_HOST' in os.environ:
            params['host'] = os.environ['MPD_HOST']
    if not params['port']:
        if 'MPD_PORT' in os.environ:
            params['port'] = os.environ['MPD_PORT']

    # Read configuration
    config = ConfigParser()
    if config_file:
        with open(config_file) as fh:
            config.read(config_file)
    else:
        config.read(['/etc/mpDris2.conf'] +
                    list(reversed(each_xdg_config('mpDris2/mpDris2.conf'))))

    for p in ['host', 'port', 'password']:
        if not params[p]:
            # TODO: switch to get(fallback=…) when possible
            if config.has_option('Connection', p):
                params[p] = config.get('Connection', p)
            else:
                params[p] = defaults[p]

    if '@' in params['host']:
        params['password'], params['host'] = params['host'].rsplit('@', 1)

    params['host'] = os.path.expanduser(params['host'])

    if not music_dir:
        if config.has_option('Library', 'music_dir'):
            music_dir = config.get('Library', 'music_dir')
        elif config.has_option('Connection', 'music_dir'):
            music_dir = config.get('Connection', 'music_dir')
        else:
            music_dir = find_music_dir()

    if music_dir:
        # Ensure that music_dir starts with an URL scheme.
        if not re.match('^[0-9A-Za-z+.-]+://', music_dir):
            music_dir = 'file://' + music_dir
        if music_dir.startswith('file://'):
            music_dir = music_dir[:7] + os.path.expanduser(music_dir[7:])
            if not os.path.exists(music_dir[7:]):
                logger.error(
                    'Music library path %s does not exist!' % music_dir)
        # Non-local URLs can still be useful to MPRIS clients, so accept them.
        params['music_dir'] = music_dir
        logger.info('Using %s as music library path.' % music_dir)
    else:
        logger.warning('By not supplying a path for the music library '
                       'this program will break the MPRIS specification!')

    if config.has_option('Library', 'cover_regex'):
        cover_regex = config.get('Library', 'cover_regex')
    else:
        cover_regex = defaults['cover_regex']
    params['cover_regex'] = re.compile(cover_regex, re.I | re.X)

    logger.debug('Parameters: %r' % params)

    if mutagen:
        logger.info('Using Mutagen to read covers from music files.')
    else:
        logger.info(
            'Mutagen not available, covers in music files will be ignored.')

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
