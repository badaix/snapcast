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
import dbus
import dbus.service
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

try:
    import gi
    gi.require_version('Notify', '0.7')
except (ImportError, ValueError):
    pass

using_gi_glib = False

try:
    from gi.repository import GLib
    using_gi_glib = True
except ImportError:
    import glib as GLib

using_gi_notify = False
using_old_notify = False

try:
    from gi.repository import Notify
    using_gi_notify = True
except ImportError:
    try:
        import pynotify
        using_old_notify = True
    except ImportError:
        pass

_ = gettext.gettext

identity = "Music Player Daemon"

params = {
    'progname': sys.argv[0],
    # Connection
    'host': None,
    'port': None,
    'password': None,
    'bus_name': None,
    # Library
    'music_dir': '',
    'cover_regex': None,
    # Bling
    'mmkeys': True,
    'notify': (using_gi_notify or using_old_notify),
    'notify_urgency': 0,
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

notification = None

# MPRIS allowed metadata tags
allowed_tags = {
    'mpris:trackid': dbus.ObjectPath,
    'mpris:length': dbus.Int64,
    'mpris:artUrl': str,
    'xesam:album': str,
    'xesam:albumArtist': list,
    'xesam:artist': list,
    'xesam:asText': str,
    'xesam:audioBPM': int,
    'xesam:comment': list,
    'xesam:composer': list,
    'xesam:contentCreated': str,
    'xesam:discNumber': int,
    'xesam:firstUsed': str,
    'xesam:genre': list,
    'xesam:lastUsed': str,
    'xesam:lyricist': str,
    'xesam:title': str,
    'xesam:trackNumber': int,
    'xesam:url': str,
    'xesam:useCount': int,
    'xesam:userRating': float,
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
    'usecount': ['useCount', int, False],
    'userating': ['useRating', float, False],

    'duration': ['duration', float, False],
    'track': ['track', int, False],
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
    'musicbrainz_releasetrackid': ['musicbrainzReleasetrackId', str, False],
    'musicbrainz_workid': ['musicbrainzWorkId', str, False],
}

# python dbus bindings don't include annotations and properties
MPRIS2_INTROSPECTION = """<node name="/org/mpris/MediaPlayer2">
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect">
      <arg direction="out" name="xml_data" type="s"/>
    </method>
  </interface>
  <interface name="org.freedesktop.DBus.Properties">
    <method name="Get">
      <arg direction="in" name="interface_name" type="s"/>
      <arg direction="in" name="property_name" type="s"/>
      <arg direction="out" name="value" type="v"/>
    </method>
    <method name="GetAll">
      <arg direction="in" name="interface_name" type="s"/>
      <arg direction="out" name="properties" type="a{sv}"/>
    </method>
    <method name="Set">
      <arg direction="in" name="interface_name" type="s"/>
      <arg direction="in" name="property_name" type="s"/>
      <arg direction="in" name="value" type="v"/>
    </method>
    <signal name="PropertiesChanged">
      <arg name="interface_name" type="s"/>
      <arg name="changed_properties" type="a{sv}"/>
      <arg name="invalidated_properties" type="as"/>
    </signal>
  </interface>
  <interface name="org.mpris.MediaPlayer2">
    <method name="Raise"/>
    <method name="Quit"/>
    <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="false"/>
    <property name="CanQuit" type="b" access="read"/>
    <property name="CanRaise" type="b" access="read"/>
    <property name="HasTrackList" type="b" access="read"/>
    <property name="Identity" type="s" access="read"/>
    <property name="DesktopEntry" type="s" access="read"/>
    <property name="SupportedUriSchemes" type="as" access="read"/>
    <property name="SupportedMimeTypes" type="as" access="read"/>
  </interface>
  <interface name="org.mpris.MediaPlayer2.Player">
    <method name="Next"/>
    <method name="Previous"/>
    <method name="Pause"/>
    <method name="PlayPause"/>
    <method name="Stop"/>
    <method name="Play"/>
    <method name="Seek">
      <arg direction="in" name="Offset" type="x"/>
    </method>
    <method name="SetPosition">
      <arg direction="in" name="TrackId" type="o"/>
      <arg direction="in" name="Position" type="x"/>
    </method>
    <method name="OpenUri">
      <arg direction="in" name="Uri" type="s"/>
    </method>
    <signal name="Seeked">
      <arg name="Position" type="x"/>
    </signal>
    <property name="PlaybackStatus" type="s" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="LoopStatus" type="s" access="readwrite">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="Rate" type="d" access="readwrite">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="Shuffle" type="b" access="readwrite">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="Metadata" type="a{sv}" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="Volume" type="d" access="readwrite">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="false"/>
    </property>
    <property name="Position" type="x" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="false"/>
    </property>
    <property name="MinimumRate" type="d" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="MaximumRate" type="d" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="CanGoNext" type="b" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="CanGoPrevious" type="b" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="CanPlay" type="b" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="CanPause" type="b" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="CanSeek" type="b" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
    <property name="CanControl" type="b" access="read">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="false"/>
    </property>
  </interface>
</node>"""

# Default url handlers if MPD doesn't support 'urlhandlers' command
urlhandlers = ['http://']
downloaded_covers = ['~/.covers/%s-%s.jpg']


class MPDWrapper(object):
    """ Wrapper of mpd.MPDClient to handle socket
        errors and similar
    """

    def __init__(self, params):
        self.client = mpd.MPDClient()

        self._dbus = dbus
        self._params = params
        self._dbus_service = None

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

        self._bus = dbus.SessionBus()
        if self._params['mmkeys']:
            self.setup_mediakeys()

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
                notification.notify(identity, _('Reconnected'))
                logger.info('Reconnected to MPD server.')
            else:
                logger.debug('Connected to MPD server.')

            # Make the socket non blocking to detect deconnections
            self.client._sock.settimeout(5.0)
            # Export our DBUS service
            if not self._dbus_service:
                self._dbus_service = MPRISInterface(self._params)
            else:
                # Add our service to the session bus
                # self._dbus_service.add_to_connection(dbus.SessionBus(),
                #    '/org/mpris/MediaPlayer2')
                self._dbus_service.acquire_name()

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
        notification.notify(identity, _('Disconnected'), 'error')

        # Release the DBus name and disconnect from bus
        if self._dbus_service is not None:
            self._dbus_service.release_name()
        # self._dbus_service.remove_from_connection()

        # Stop monitoring
        if self._poll_id:
            GLib.source_remove(self._poll_id)
            self._poll_id = None
        if self._watch_id:
            GLib.source_remove(self._watch_id)
            self._watch_id = None

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
                            self._update_properties(force=True)
                            updated = True
                self.idle_enter()
        return True

    def mediakey_callback(self, appname, key):
        """ GNOME media key handler """
        logger.debug('Got GNOME mmkey "%s" for "%s"' % (key, appname))
        if key == 'Play':
            if self._status['state'] == 'play':
                self.client.pause(1)
                self.notify_about_state('pause')
            else:
                self.play()
                self.notify_about_state('play')
        elif key == 'Next':
            self.client.next()
        elif key == 'Previous':
            self.client.previous()
        elif key == 'Stop':
            self.client.stop()
            self.notify_about_state('stop')

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
        print(mpd_meta)
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
                        print('cast')
                        value = [tag_mapping[key][1](values)]
                    else:
                        value = tag_mapping[key][1](values)
                snapmeta[tag_mapping[key][0]] = value
                print(
                    f'key: {key}, value: {value}, mapped key: {tag_mapping[key][0]}, mapped value: {snapmeta[tag_mapping[key][0]]}')
            except KeyError:
                print(f'tag "{key}" not supported')
            except (ValueError, TypeError):
                print("Can't cast value %r to %s" %
                      (value, tag_mapping[key][1]))

        r = requests.post('http://127.0.0.1:1780/jsonrpc', json={"id": 4, "jsonrpc": "2.0", "method": "Stream.SetMeta", "params": {
            "id": "Spotify", "meta": snapmeta}})
        print(r)

        self._metadata = {}

        for tag in ('album', 'title'):
            if tag in mpd_meta:
                self._metadata['xesam:%s' % tag] = mpd_meta[tag]

        if 'id' in mpd_meta:
            self._metadata['mpris:trackid'] = "/org/mpris/MediaPlayer2/Track/%s" % \
                                              mpd_meta['id']

        if 'time' in mpd_meta:
            self._metadata['mpris:length'] = int(mpd_meta['time']) * 1000000

        if 'date' in mpd_meta:
            self._metadata['xesam:contentCreated'] = mpd_meta['date'][0:4]

        if 'track' in mpd_meta:
            # TODO: Is it even *possible* for mpd_meta['track'] to be a list?
            if type(mpd_meta['track']) == list and len(mpd_meta['track']) > 0:
                track = str(mpd_meta['track'][0])
            else:
                track = str(mpd_meta['track'])

            m = re.match('^([0-9]+)', track)
            if m:
                self._metadata['xesam:trackNumber'] = int(m.group(1))
                # Ensure the integer is signed 32bit
                if self._metadata['xesam:trackNumber'] & 0x80000000:
                    self._metadata['xesam:trackNumber'] += -0x100000000
            else:
                self._metadata['xesam:trackNumber'] = 0

        if 'disc' in mpd_meta:
            # TODO: Same as above. When is it a list?
            if type(mpd_meta['disc']) == list and len(mpd_meta['disc']) > 0:
                disc = str(mpd_meta['disc'][0])
            else:
                disc = str(mpd_meta['disc'])

            m = re.match('^([0-9]+)', disc)
            if m:
                self._metadata['xesam:discNumber'] = int(m.group(1))

        if 'artist' in mpd_meta:
            if type(mpd_meta['artist']) == list:
                self._metadata['xesam:artist'] = mpd_meta['artist']
            else:
                self._metadata['xesam:artist'] = [mpd_meta['artist']]

        if 'composer' in mpd_meta:
            if type(mpd_meta['composer']) == list:
                self._metadata['xesam:composer'] = mpd_meta['composer']
            else:
                self._metadata['xesam:composer'] = [mpd_meta['composer']]

        # Stream: populate some missings tags with stream's name
        if 'name' in mpd_meta:
            if 'xesam:title' not in self._metadata:
                self._metadata['xesam:title'] = mpd_meta['name']
            elif 'xesam:album' not in self._metadata:
                self._metadata['xesam:album'] = mpd_meta['name']

        if 'file' in mpd_meta:
            song_url = mpd_meta['file']
            if not any([song_url.startswith(prefix) for prefix in urlhandlers]):
                song_url = os.path.join(self._params['music_dir'], song_url)
            self._metadata['xesam:url'] = song_url
            cover = self.find_cover(song_url)
            if cover:
                self._metadata['mpris:artUrl'] = cover

        # Cast self._metadata to the correct type, or discard it
        for key, value in self._metadata.items():
            try:
                self._metadata[key] = allowed_tags[key](value)
            except ValueError:
                del self._metadata[key]
                logger.error("Can't cast value %r to %s" %
                             (value, allowed_tags[key]))

        # if 'xesam:title' in self._metadata and 'xesam:album' in self._metadata:
        #     result = musicbrainzngs.search_releases(artist=self._metadata['xesam:title'], release=self._metadata['xesam:album'],
        #                                             limit=1)
        #     if result['release-list']:
        #         self._metadata[
        #             'mpris:artUrl'] = f"http://coverartarchive.org/release/{result['release-list'][0]['id']}/front-250"
        #         print(self._metadata['mpris:artUrl'])

    def notify_about_track(self, meta, state='play'):
        uri = 'sound'
        if 'mpris:artUrl' in meta:
            uri = meta['mpris:artUrl']

        title = 'Unknown Title'
        if 'xesam:title' in meta:
            title = meta['xesam:title']
        elif 'xesam:url' in meta:
            title = meta['xesam:url'].split('/')[-1]

        artist = 'Unknown Artist'
        if 'xesam:artist' in meta:
            artist = ", ".join(meta['xesam:artist'])

        body = _('by %s') % artist

        if state == 'pause':
            uri = 'media-playback-pause-symbolic'
            body += ' (%s)' % _('Paused')

        notification.notify(title, body, uri)

    def notify_about_state(self, state):
        if state == 'stop':
            notification.notify(identity, _('Stopped'),
                                'media-playback-stop-symbolic')
        else:
            self.notify_about_track(self.metadata, state)

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
            self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
                                               'PlaybackStatus')

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
                self._dbus_service.Seeked(new_position * 1000000)

        else:
            # Update current song metadata
            old_meta = self._metadata.copy()
            self.update_metadata()
            new_meta = self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
                                                          'Metadata')

            if self._params['notify'] and new_status['state'] != 'stop':
                if old_meta.get('xesam:artist', None) != new_meta.get('xesam:artist', None) \
                        or old_meta.get('xesam:album', None) != new_meta.get('xesam:album', None) \
                        or old_meta.get('xesam:title', None) != new_meta.get('xesam:title', None) \
                        or old_meta.get('xesam:url', None) != new_meta.get('xesam:url', None):
                    self.notify_about_track(new_meta, new_status['state'])

        # "mixer" subsystem
        if old_status.get('volume') != new_status.get('volume'):
            self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
                                               'Volume')

        # "options" subsystem
        # also triggered if consume, crossfade or ReplayGain are updated

        if old_status['random'] != new_status['random']:
            self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
                                               'Shuffle')

        if (old_status['repeat'] != new_status['repeat']
                or old_status.get('single', 0) != new_status.get('single', 0)):
            self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
                                               'LoopStatus')

        if ("nextsongid" in old_status) != ("nextsongid" in new_status):
            self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
                                               'CanGoNext')

    # Media keys

    def setup_mediakeys(self):
        self.register_mediakeys()
        self._dbus_obj = self._bus.get_object("org.freedesktop.DBus",
                                              "/org/freedesktop/DBus")
        self._dbus_obj.connect_to_signal("NameOwnerChanged",
                                         self.gsd_name_owner_changed_callback,
                                         arg0="org.gnome.SettingsDaemon")

    def register_mediakeys(self):
        try:
            try:
                gsd_object = self._bus.get_object("org.gnome.SettingsDaemon.MediaKeys",
                                                  "/org/gnome/SettingsDaemon/MediaKeys")
            except:
                # Try older name.
                gsd_object = self._bus.get_object("org.gnome.SettingsDaemon",
                                                  "/org/gnome/SettingsDaemon/MediaKeys")
            gsd_object.GrabMediaPlayerKeys("mpDris2", 0,
                                           dbus_interface="org.gnome.SettingsDaemon.MediaKeys")
        except:
            logger.warning(
                "Failed to connect to GNOME Settings Daemon. Media keys won't work.")
        else:
            self._bus.remove_signal_receiver(self.mediakey_callback)
            gsd_object.connect_to_signal(
                "MediaPlayerKeyPressed", self.mediakey_callback)

    def gsd_name_owner_changed_callback(self, bus_name, old_owner, new_owner):
        if bus_name == "org.gnome.SettingsDaemon" and new_owner != "":
            def reregister():
                logger.debug(
                    "Re-registering with GNOME Settings Daemon (owner %s)" % new_owner)
                self.register_mediakeys()
                return False
            # Timeout is necessary since g-s-d takes some time to load all plugins.
            GLib.timeout_add(600, reregister)

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


class NotifyWrapper(object):

    def __init__(self, params):
        self._notification = None
        self._enabled = True

        if params["notify"]:
            self._notification = self._bootstrap_notifications()
            if not self._notification:
                logger.error(
                    "No notification service provider could be found; disabling notifications")
        else:
            self._enabled = False

    def _bootstrap_notifications(self):
        # Check if someone is providing the notification service
        bus = dbus.SessionBus()
        try:
            bus.get_name_owner("org.freedesktop.Notifications")
        except dbus.exceptions.DBusException:
            return None

        notif = None

        # Bootstrap whatever notifications system we are using
        if using_gi_notify:
            logger.debug("Initializing GObject.Notify")
            if Notify.init(identity):
                notif = Notify.Notification()
                notif.set_hint("desktop-entry", GLib.Variant("s", "mpdris2"))
                notif.set_hint("transient", GLib.Variant("b", True))
            else:
                logger.error(
                    "Failed to init libnotify; disabling notifications")
        elif using_old_notify:
            logger.debug("Initializing old pynotify")
            if pynotify.init(identity):
                notif = pynotify.Notification("", "", "")
                notif.set_hint("desktop-entry", "mpdris2")
                notif.set_hint("transient", True)
            else:
                logger.error(
                    "Failed to init libnotify; disabling notifications")

        return notif

    def notify(self, title, body, uri=''):
        if not self._enabled:
            return

        # If we did not yet manage to get a notification service,
        # try again
        if not self._notification:
            logger.info(
                'Retrying to acquire a notification service provider...')
            self._notification = self._bootstrap_notifications()
            if self._notification:
                logger.info('Notification service provider acquired!')

        if self._notification:
            try:
                self._notification.set_urgency(params['notify_urgency'])
                self._notification.update(title, body, uri)
                self._notification.show()
            except GLib.GError as err:
                logger.error("Failed to show notification: %s" % err)


class MPRISInterface(dbus.service.Object):
    ''' The base object of an MPRIS player '''

    __path = "/org/mpris/MediaPlayer2"
    __introspect_interface = "org.freedesktop.DBus.Introspectable"
    __prop_interface = dbus.PROPERTIES_IFACE

    def __init__(self, params):
        dbus.service.Object.__init__(self, dbus.SessionBus(),
                                     MPRISInterface.__path)
        self._params = params or {}
        self._name = self._params["bus_name"] or "org.mpris.MediaPlayer2.mpd"
        if not self._name.startswith("org.mpris.MediaPlayer2."):
            logger.warn(
                "Configured bus name %r is outside MPRIS2 namespace" % self._name)

        self._bus = dbus.SessionBus()
        self._uname = self._bus.get_unique_name()
        self._dbus_obj = self._bus.get_object("org.freedesktop.DBus",
                                              "/org/freedesktop/DBus")
        self._dbus_obj.connect_to_signal("NameOwnerChanged",
                                         self._name_owner_changed_callback,
                                         arg0=self._name)

        self.acquire_name()

    def _name_owner_changed_callback(self, name, old_owner, new_owner):
        if name == self._name and old_owner == self._uname and new_owner != "":
            try:
                pid = self._dbus_obj.GetConnectionUnixProcessID(new_owner)
            except:
                pid = None
            logger.info("Replaced by %s (PID %s)" %
                        (new_owner, pid or "unknown"))
            loop.quit()

    def acquire_name(self):
        self._bus_name = dbus.service.BusName(self._name,
                                              bus=self._bus,
                                              allow_replacement=True,
                                              replace_existing=True)

    def release_name(self):
        if hasattr(self, "_bus_name"):
            del self._bus_name

    __root_interface = "org.mpris.MediaPlayer2"
    __root_props = {
        "CanQuit": (False, None),
        "CanRaise": (False, None),
        "DesktopEntry": ("mpdris2", None),
        "HasTrackList": (False, None),
        "Identity": (identity, None),
        "SupportedUriSchemes": (dbus.Array(signature="s"), None),
        "SupportedMimeTypes": (dbus.Array(signature="s"), None)
    }

    def __get_playback_status():
        status = mpd_wrapper.last_status()
        return {'play': 'Playing', 'pause': 'Paused', 'stop': 'Stopped'}[status['state']]

    def __set_loop_status(value):
        if value == "Playlist":
            mpd_wrapper.repeat(1)
            if mpd_wrapper._can_single:
                mpd_wrapper.single(0)
        elif value == "Track":
            if mpd_wrapper._can_single:
                mpd_wrapper.repeat(1)
                mpd_wrapper.single(1)
        elif value == "None":
            mpd_wrapper.repeat(0)
            if mpd_wrapper._can_single:
                mpd_wrapper.single(0)
        else:
            raise dbus.exceptions.DBusException("Loop mode %r not supported" %
                                                value)
        return

    def __get_loop_status():
        status = mpd_wrapper.last_status()
        if int(status['repeat']) == 1:
            if int(status.get('single', 0)) == 1:
                return "Track"
            else:
                return "Playlist"
        else:
            return "None"

    def __set_shuffle(value):
        mpd_wrapper.random(value)
        return

    def __get_shuffle():
        if int(mpd_wrapper.last_status()['random']) == 1:
            return True
        else:
            return False

    def __get_metadata():
        return dbus.Dictionary(mpd_wrapper.metadata, signature='sv')

    def __get_volume():
        vol = float(mpd_wrapper.last_status().get('volume', 0))
        if vol > 0:
            return vol / 100.0
        else:
            return 0.0

    def __set_volume(value):
        if value >= 0 and value <= 1:
            mpd_wrapper.setvol(int(value * 100))
        return

    def __get_position():
        status = mpd_wrapper.last_status()
        if 'time' in status:
            current, end = status['time'].split(':')
            return dbus.Int64((int(current) * 1000000))
        else:
            return dbus.Int64(0)

    __player_interface = "org.mpris.MediaPlayer2.Player"
    __player_props = {
        "PlaybackStatus": (__get_playback_status, None),
        "LoopStatus": (__get_loop_status, __set_loop_status),
        "Rate": (1.0, None),
        "Shuffle": (__get_shuffle, __set_shuffle),
        "Metadata": (__get_metadata, None),
        "Volume": (__get_volume, __set_volume),
        "Position": (__get_position, None),
        "MinimumRate": (1.0, None),
        "MaximumRate": (1.0, None),
        "CanGoNext": (True, None),
        "CanGoPrevious": (True, None),
        "CanPlay": (True, None),
        "CanPause": (True, None),
        "CanSeek": (True, None),
        "CanControl": (True, None),
    }

    __tracklist_interface = "org.mpris.MediaPlayer2.TrackList"

    __prop_mapping = {
        __player_interface: __player_props,
        __root_interface: __root_props,
    }

    @dbus.service.method(__introspect_interface)
    def Introspect(self):
        return MPRIS2_INTROSPECTION

    @dbus.service.signal(__prop_interface, signature="sa{sv}as")
    def PropertiesChanged(self, interface, changed_properties,
                          invalidated_properties):
        pass

    @dbus.service.method(__prop_interface,
                         in_signature="ss", out_signature="v")
    def Get(self, interface, prop):
        getter, setter = self.__prop_mapping[interface][prop]
        if callable(getter):
            return getter()
        return getter

    @dbus.service.method(__prop_interface,
                         in_signature="ssv", out_signature="")
    def Set(self, interface, prop, value):
        getter, setter = self.__prop_mapping[interface][prop]
        if setter is not None:
            setter(value)

    @dbus.service.method(__prop_interface,
                         in_signature="s", out_signature="a{sv}")
    def GetAll(self, interface):
        read_props = {}
        props = self.__prop_mapping[interface]
        for key, (getter, setter) in props.items():
            if callable(getter):
                getter = getter()
            read_props[key] = getter
        return read_props

    def update_property(self, interface, prop):
        getter, setter = self.__prop_mapping[interface][prop]
        if callable(getter):
            value = getter()
        else:
            value = getter
        logger.debug('Updated property: %s = %s' % (prop, value))
        self.PropertiesChanged(interface, {prop: value}, [])
        return value

    # Root methods
    @dbus.service.method(__root_interface, in_signature='', out_signature='')
    def Raise(self):
        return

    @dbus.service.method(__root_interface, in_signature='', out_signature='')
    def Quit(self):
        return

    # Player methods
    @dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Next(self):
        mpd_wrapper.next()
        return

    @dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Previous(self):
        mpd_wrapper.previous()
        return

    @dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Pause(self):
        mpd_wrapper.pause(1)
        mpd_wrapper.notify_about_state('pause')
        return

    @dbus.service.method(__player_interface, in_signature='', out_signature='')
    def PlayPause(self):
        status = mpd_wrapper.status()
        if status['state'] == 'play':
            mpd_wrapper.pause(1)
            mpd_wrapper.notify_about_state('pause')
        else:
            mpd_wrapper.play()
            mpd_wrapper.notify_about_state('play')
        return

    @dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Stop(self):
        mpd_wrapper.stop()
        mpd_wrapper.notify_about_state('stop')
        return

    @dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Play(self):
        mpd_wrapper.play()
        mpd_wrapper.notify_about_state('play')
        return

    @dbus.service.method(__player_interface, in_signature='x', out_signature='')
    def Seek(self, offset):
        status = mpd_wrapper.status()
        current, end = status['time'].split(':')
        current = int(current)
        end = int(end)
        offset = int(offset) / 1000000
        if current + offset <= end:
            position = current + offset
            if position < 0:
                position = 0
            mpd_wrapper.seekid(int(status['songid']), position)
            self.Seeked(position * 1000000)
        return

    @dbus.service.method(__player_interface, in_signature='ox', out_signature='')
    def SetPosition(self, trackid, position):
        song = mpd_wrapper.last_currentsong()
        # FIXME: use real dbus objects
        if str(trackid) != '/org/mpris/MediaPlayer2/Track/%s' % song['id']:
            return
        # Convert position to seconds
        position = int(position) / 1000000
        if position <= int(song['time']):
            mpd_wrapper.seekid(int(song['id']), position)
            self.Seeked(position * 1000000)
        return

    @dbus.service.signal(__player_interface, signature='x')
    def Seeked(self, position):
        logger.debug("Seeked to %i" % position)
        return float(position)

    @dbus.service.method(__player_interface, in_signature='', out_signature='')
    def OpenUri(self):
        # TODO
        return


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
                                     ['help', 'bus-name=', 'config=',
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
        elif opt in ['--bus-name']:
            params['bus_name'] = arg
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

    for p in ['host', 'port', 'password', 'bus_name']:
        if not params[p]:
            # TODO: switch to get(fallback=…) when possible
            if config.has_option('Connection', p):
                params[p] = config.get('Connection', p)
            else:
                params[p] = defaults[p]

    if '@' in params['host']:
        params['password'], params['host'] = params['host'].rsplit('@', 1)

    params['host'] = os.path.expanduser(params['host'])

    for p in ['mmkeys', 'notify']:
        if config.has_option('Bling', p):
            params[p] = config.getboolean('Bling', p)

    if config.has_option('Bling', 'notify_urgency'):
        params['notify_urgency'] = int(config.get('Bling', 'notify_urgency'))

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

    # Wrapper to send notifications
    notification = NotifyWrapper(params)

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
