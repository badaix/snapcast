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


from time import sleep
import websocket
import logging
import threading
import json

from configparser import ConfigParser
import os
import sys
import re
import shlex
import getopt
import socket
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
import logging
import gettext
import requests

__version__ = "@version@"
__git_version__ = "@gitversion@"


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

identity = "Snapcast"

params = {
    'progname': sys.argv[0],
    # Connection
    'host': None,
    'port': None,
    'password': None,
    'bus_name': None,
    'client': None,
    # Bling
    'mmkeys': True,
    'notify': (using_gi_notify or using_old_notify),
    'notify_urgency': 0,
}

defaults = {
    # Connection
    'host': 'localhost',
    'port': 1780,
    'client': None,
    'password': None,
    'bus_name': None,
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


class SnapcastRpcListener:
    def on_snapserver_stream_pause(self):
        pass

    def on_snapserver_stream_start(self, stream_name, stream_group):
        pass

    def on_snapserver_volume_change(self, volume_level):
        pass

    def on_snapserver_mute(self):
        pass

    def on_snapserver_unmute(self):
        pass


tag_mapping = {
    'trackId': 'mpris:trackid',
    'artUrl': 'mpris:artUrl',
    'duration': 'mpris:length',
    'album': 'xesam:album',
    'albumArtist': 'xesam:albumArtist',
    'artist': 'xesam:artist',
    'asText': 'xesam:asText',
    'audioBPM': 'xesam:audioBPM',
    'autoRating': 'xesam:autoRating',
    'comment': 'xesam:comment',
    'composer': 'xesam:composer',
    'contentCreated': 'xesam:contentCreated',
    'discNumber': 'xesam:discNumber',
    'firstUsed': 'xesam:firstUsed',
    'genre': 'xesam:genre',
    'lastUsed': 'xesam:lastUsed',
    'lyricist': 'xesam:lyricist',
    'title': 'xesam:title',
    'trackNumber': 'xesam:trackNumber',
    'url': 'xesam:url',
    'useCount': 'xesam:useCount',
    'userRating': 'xesam:userRating',
}


class MPDWrapper(object):
    """ Wrapper of mpd.MPDClient to handle socket
        errors and similar
    """

    def __init__(self, params):
        # self.client = mpd.MPDClient()

        self._dbus = dbus
        self._params = params
        self._dbus_service = None

        self._can_single = False

        self._errors = 0

        self._status = {
            'state': None,
            'volume': None,
            'random': None,
            'repeat': None,
        }
        self._metadata = {}
        self._position = 0
        self._time = 0
        self._req_id = 0

        self._bus = dbus.SessionBus()
        if self._params['mmkeys']:
            self.setup_mediakeys()

        self.websocket = websocket.WebSocketApp("ws://" + self._params['host'] + ":" + str(self._params['port']) + "/jsonrpc",
                                                on_message=self.on_ws_message,
                                                on_error=self.on_ws_error,
                                                on_open=self.on_ws_open,
                                                on_close=self.on_ws_close)

        self.websocket_thread = threading.Thread(
            target=self.websocket_loop, args=())
        self.websocket_thread.name = "SnapcastRpcWebsocketWrapper"
        self.websocket_thread.start()

    def websocket_loop(self):
        logger.info("Started SnapcastRpcWebsocketWrapper loop")
        while True:
            self.websocket.run_forever()
            sleep(1)
        logger.info("Ending SnapcastRpcWebsocketWrapper loop")

    def on_ws_message(self, ws, message):
        logger.debug("Snapcast RPC websocket message received")
        logger.debug(message)
        jmsg = json.loads(message)
        if jmsg["method"] == "Stream.OnMetadata":
            logger.info(f'Stream meta changed for "{jmsg["params"]["id"]}"')
            meta = jmsg["params"]["meta"]
            logger.info(f'Meta: "{meta}"')

            self._metadata = {}
            self._metadata['xesam:artist'] = ['Unknown Artist']
            self._metadata['xesam:title'] = 'Unknown Title'

            for key, value in meta.items():
                if key in tag_mapping:
                    self._metadata[tag_mapping[key]] = value

            if 'mpris:length' in self._metadata:
                self._metadata['mpris:length'] = int(1000 * 1000 *
                                                     self._metadata['mpris:length'])
            if 'mpris:trackid' in self._metadata:
                self._metadata[
                    'mpris:trackid'] = f'/org/mpris/MediaPlayer2/Track/{self._metadata["mpris:trackid"]}'

            logger.info(f'mpris meta: {self._metadata}')

            self.notify_about_track(self._metadata)
            new_meta = self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
                                                          'Metadata')
            logger.info(f'new meta {new_meta}')

    def on_ws_error(self, ws, error):
        logger.error("Snapcast RPC websocket error")
        logger.error(error)

    def on_ws_open(self, ws):
        logger.info("Snapcast RPC websocket opened")

        # Export our DBUS service
        if not self._dbus_service:
            self._dbus_service = MPRISInterface(self._params)
        else:
            # Add our service to the session bus
            # self._dbus_service.add_to_connection(dbus.SessionBus(),
            #    '/org/mpris/MediaPlayer2')
            self._dbus_service.acquire_name()

        self.websocket.send(json.dumps(
            {"id": 1, "jsonrpc": "2.0", "method": "Server.GetStatus"}))

    def on_ws_close(self, ws):
        logger.info("Snapcast RPC websocket closed")
        if self._dbus_service is not None:
            self._dbus_service.release_name()
        # self._dbus_service.remove_from_connection()

    def stop(self):
        self.websocket.keep_running = False
        logger.info("Waiting for websocket thread to exit")
        # self.websocket_thread.join()

    # def run(self):
    #     """
    #     Try to connect to MPD; retry every 5 seconds on failure.
    #     """
    #     if self.my_connect():
    #         GLib.timeout_add_seconds(5, self.my_connect)
    #         return False
    #     else:
    #         return True

    # @property
    # def connected(self):
    #     return self.client._sock is not None

    # def init_state(self):
    #     # Get current state
    #     self._status = self.client.status()
    #     # Invalid some fields to throw events at start
    #     self._status['state'] = 'invalid'
    #     self._status['songid'] = '-1'
    #     self._position = 0

    # Events

    # def socket_callback(self, fd, event):
    #     logger.debug("Socket event %r on fd %r" % (event, fd))
    #     if event & GLib.IO_HUP:
    #         self.reconnect()
    #         return True
    #     elif event & GLib.IO_IN:
    #         if self._idling:
    #             self._idling = False
    #             data = fd._fetch_objects("changed")
    #             logger.debug("Idle events: %r" % data)
    #             updated = False
    #             for item in data:
    #                 subsystem = item["changed"]
    #                 # subsystem list: <http://www.musicpd.org/doc/protocol/ch03.html>
    #                 if subsystem in ("player", "mixer", "options", "playlist"):
    #                     if not updated:
    #                         self._update_properties(force=True)
    #                         updated = True
    #             self.idle_enter()
    #     return True

    def mediakey_callback(self, appname, key):
        """ GNOME media key handler """
        logger.debug('Got GNOME mmkey "%s" for "%s"' % (key, appname))
        # if key == 'Play':
        #     if self._status['state'] == 'play':
        #         self.client.pause(1)
        #         self.notify_about_state('pause')
        #     else:
        #         self.play()
        #         self.notify_about_state('play')
        # elif key == 'Next':
        #     self.client.next()
        # elif key == 'Previous':
        #     self.client.previous()
        # elif key == 'Stop':
        #     self.client.stop()
        #     self.notify_about_state('stop')

    # def last_currentsong(self):
    #     return self._currentsong.copy()

    def control(self, command, params={}):
        j = {"id": self._req_id, "jsonrpc": "2.0", "method": "Stream.Control",
             "params": {"id": "Pipe", "command": command, "params": params}}
        logger.info(f'Control: {command}, json: {j}')
        url = f'http://{self._params["host"]}:{self._params["port"]}/jsonrpc'
        logger.info(f'url: {url}')
        self._req_id += 1
        requests.post(url, json=j)

    @property
    def metadata(self):
        return self._metadata

    def notify_about_track(self, meta, state='play'):
        uri = 'sound'
        if 'mpris:artUrl' in meta:
            uri = meta['mpris:artUrl']

        title = 'Unknown Title'
        if 'xesam:title' in meta:
            title = meta['xesam:title']
        elif 'xesam:url' in meta:
            title = meta['xesam:url'].split('/')[-1]

        artist = ['Unknown Artist']
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

    # def _update_properties(self, force=False):
    #     old_status = self._status
    #     old_position = self._position
    #     old_time = self._time
    #     self._currentsong = self.client.currentsong()
    #     new_status = self.client.status()
    #     self._time = new_time = int(time.time())

    #     if not new_status:
    #         logger.debug("_update_properties: failed to get new status")
    #         return

    #     self._status = new_status
    #     logger.debug("_update_properties: current song = %r" %
    #                  self._currentsong)
    #     logger.debug("_update_properties: current status = %r" % self._status)

    #     if 'elapsed' in new_status:
    #         new_position = float(new_status['elapsed'])
    #     elif 'time' in new_status:
    #         new_position = int(new_status['time'].split(':')[0])
    #     else:
    #         new_position = 0

    #     self._position = new_position

    #     # "player" subsystem

    #     if old_status['state'] != new_status['state']:
    #         self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
    #                                            'PlaybackStatus')

    #     if not force:
    #         old_id = old_status.get('songid', None)
    #         new_id = new_status.get('songid', None)
    #         force = (old_id != new_id)

    #     if not force:
    #         if new_status['state'] == 'play':
    #             expected_position = old_position + (new_time - old_time)
    #         else:
    #             expected_position = old_position
    #         if abs(new_position - expected_position) > 0.6:
    #             logger.debug("Expected pos %r, actual %r, diff %r" % (
    #                 expected_position, new_position, new_position - expected_position))
    #             logger.debug("Old position was %r at %r (%r seconds ago)" % (
    #                 old_position, old_time, new_time - old_time))
    #             self._dbus_service.Seeked(new_position * 1000000)

    #     else:
    #         # Update current song metadata
    #         old_meta = self._metadata.copy()
    #         self.update_metadata()
    #         new_meta = self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
    #                                                       'Metadata')

    #         if self._params['notify'] and new_status['state'] != 'stop':
    #             if old_meta.get('xesam:artist', None) != new_meta.get('xesam:artist', None) \
    #                     or old_meta.get('xesam:album', None) != new_meta.get('xesam:album', None) \
    #                     or old_meta.get('xesam:title', None) != new_meta.get('xesam:title', None) \
    #                     or old_meta.get('xesam:url', None) != new_meta.get('xesam:url', None):
    #                 self.notify_about_track(new_meta, new_status['state'])

    #     # "mixer" subsystem
    #     if old_status.get('volume') != new_status.get('volume'):
    #         self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
    #                                            'Volume')

    #     # "options" subsystem
    #     # also triggered if consume, crossfade or ReplayGain are updated

    #     if old_status['random'] != new_status['random']:
    #         self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
    #                                            'Shuffle')

    #     if (old_status['repeat'] != new_status['repeat']
    #             or old_status.get('single', 0) != new_status.get('single', 0)):
    #         self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
    #                                            'LoopStatus')

    #     if ("nextsongid" in old_status) != ("nextsongid" in new_status):
    #         self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
    #                                            'CanGoNext')

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


class NotifyWrapper(object):

    def __init__(self, params):
        self._last_notification = None
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

        if self._last_notification == [title, body, uri]:
            return

        self._last_notification = [title, body, uri]

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
        self._name = self._params["bus_name"] or "org.mpris.MediaPlayer2.snapcast"
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
        # status = mpd_wrapper.last_status()
        # return {'play': 'Playing', 'pause': 'Paused', 'stop': 'Stopped'}[status['state']]
        return 'Playing'

    def __set_loop_status(value):
        logger.debug(f'set_loop_status "{value}"')
        # if value == "Playlist":
        #     mpd_wrapper.repeat(1)
        #     if mpd_wrapper._can_single:
        #         mpd_wrapper.single(0)
        # elif value == "Track":
        #     if mpd_wrapper._can_single:
        #         mpd_wrapper.repeat(1)
        #         mpd_wrapper.single(1)
        # elif value == "None":
        #     mpd_wrapper.repeat(0)
        #     if mpd_wrapper._can_single:
        #         mpd_wrapper.single(0)
        # else:
        #     raise dbus.exceptions.DBusException("Loop mode %r not supported" %
        #                                         value)
        return

    def __get_loop_status():
        logger.debug(f'get_loop_status')
        # status = mpd_wrapper.last_status()
        # if int(status['repeat']) == 1:
        #     if int(status.get('single', 0)) == 1:
        #         return "Track"
        #     else:
        #         return "Playlist"
        # else:
        # return "None"
        return "None"

    def __set_shuffle(value):
        logger.debug(f'set_shuffle "{value}"')
        # mpd_wrapper.random(value)
        return

    def __get_shuffle():
        logger.debug(f'get_shuffle')
        # if int(mpd_wrapper.last_status()['random']) == 1:
        #     return True
        # else:
        #     return False
        return False

    def __get_metadata():
        logger.debug(f'get_metadata: {snapcast_wrapper.metadata}')
        return dbus.Dictionary(snapcast_wrapper.metadata, signature='sv')

    def __get_volume():
        logger.debug(f'get_volume')
        # vol = float(mpd_wrapper.last_status().get('volume', 0))
        # if vol > 0:
        #     return vol / 100.0
        # else:
        #     return 0.0
        return 0.0

    def __set_volume(value):
        logger.debug(f'set_voume: {value}')
        # if value >= 0 and value <= 1:
        #     mpd_wrapper.setvol(int(value * 100))
        return

    def __get_position():
        logger.debug(f'get_position')
        # status = mpd_wrapper.last_status()
        # if 'time' in status:
        #     current, end = status['time'].split(':')
        #     return dbus.Int64((int(current) * 1000000))
        # else:
        #     return dbus.Int64(0)
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

    __prop_mapping = {
        __player_interface: __player_props,
        __root_interface: __root_props,
    }

    @ dbus.service.method(__introspect_interface)
    def Introspect(self):
        return MPRIS2_INTROSPECTION

    @ dbus.service.signal(__prop_interface, signature="sa{sv}as")
    def PropertiesChanged(self, interface, changed_properties,
                          invalidated_properties):
        pass

    @ dbus.service.method(__prop_interface,
                          in_signature="ss", out_signature="v")
    def Get(self, interface, prop):
        getter, setter = self.__prop_mapping[interface][prop]
        if callable(getter):
            return getter()
        return getter

    @ dbus.service.method(__prop_interface,
                          in_signature="ssv", out_signature="")
    def Set(self, interface, prop, value):
        getter, setter = self.__prop_mapping[interface][prop]
        if setter is not None:
            setter(value)

    @ dbus.service.method(__prop_interface,
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
    @ dbus.service.method(__root_interface, in_signature='', out_signature='')
    def Raise(self):
        logger.info('Raise')
        return

    @ dbus.service.method(__root_interface, in_signature='', out_signature='')
    def Quit(self):
        logger.info('Quit')
        return

    # Player methods
    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Next(self):
        snapcast_wrapper.control("Next")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Previous(self):
        snapcast_wrapper.control("Previous")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Pause(self):
        snapcast_wrapper.control("Pause")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def PlayPause(self):
        snapcast_wrapper.control("PlayPause")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Stop(self):
        snapcast_wrapper.control("Stop")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Play(self):
        snapcast_wrapper.control("Play")
        return

    @ dbus.service.method(__player_interface, in_signature='x', out_signature='')
    def Seek(self, offset):
        logger.info(f'Seek {offset}')
        snapcast_wrapper.control("Seek", {"Offset": offset})
        # status = mpd_wrapper.status()
        # current, end = status['time'].split(':')
        # current = int(current)
        # end = int(end)
        # offset = int(offset) / 1000000
        # if current + offset <= end:
        #     position = current + offset
        #     if position < 0:
        #         position = 0
        #     mpd_wrapper.seekid(int(status['songid']), position)
        #     self.Seeked(position * 1000000)
        return

    @ dbus.service.method(__player_interface, in_signature='ox', out_signature='')
    def SetPosition(self, trackid, position):
        logger.info(f'SetPosition TrackId: {trackid}, Position: {position}')
        snapcast_wrapper.control(
            "SetPosition", {"TrackId": trackid, "Position": position})
        self.Seeked(position)

        # song = mpd_wrapper.last_currentsong()
        # # FIXME: use real dbus objects
        # if str(trackid) != '/org/mpris/MediaPlayer2/Track/%s' % song['id']:
        #     return
        # # Convert position to seconds
        # position = int(position) / 1000000
        # if position <= int(song['time']):
        #     mpd_wrapper.seekid(int(song['id']), position)
        #     self.Seeked(position * 1000000)
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def OpenUri(self):
        logger.info('OpenUri')
        # TODO
        return

    # Player signals
    @ dbus.service.signal(__player_interface, signature='x')
    def Seeked(self, position):
        logger.debug("Seeked to %i" % position)
        return float(position)


def __get_client_from_server_status(status):
    client = None
    try:
        for group in status['result']['server']['groups']:
            for client in group['clients']:
                if client['host']['name'] == hostname:
                    active = client["connected"]
                    logger.info(
                        f'Client with id "{client["id"]}" active: {active}')
                    client = client['id']
                    if active:
                        return client
    except:
        logger.error('Failed to parse server status')
    return client


def usage(params):
    print("""\
Usage: %(progname)s [OPTION]...

     -c, --config=PATH      Read a custom configuration file

     -h, --host=ADDR        Set the mpd server address
         --port=PORT        Set the TCP port
         --client=ID        Set the client id
     -d, --debug            Run in debug mode
     -j, --use-journal      Log to systemd journal instead of stderr
     -v, --version          mpDris2 version

Environment variables MPD_HOST and MPD_PORT can be used.

Report bugs to https://github.com/eonpatapon/mpDris2/issues""" % params)


if __name__ == '__main__':
    DBusGMainLoop(set_as_default=True)

    # TODO:
    # -cleanup: remove mpd-ish stuff
    # -stream id: keep track of the client's stream
    gettext.bindtextdomain('mpDris2', '@datadir@/locale')
    gettext.textdomain('mpDris2')

    log_format_stderr = '%(asctime)s %(module)s %(levelname)s: %(message)s'

    log_journal = False
    log_level = logging.INFO
    config_file = None

    # Parse command line
    try:
        (opts, args) = getopt.getopt(sys.argv[1:], 'c:dh:jp:v',
                                     ['help', 'bus-name=', 'config=',
                                     'debug', 'host=', 'client='
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
        elif opt in ['--port']:
            params['port'] = int(arg)
        elif opt in ['--client']:
            params['client'] = int(arg)
        elif opt in ['-v', '--version']:
            v = __version__
            if __git_version__:
                v = __git_version__
            print("mpDris2 version %s" % v)
            sys.exit()

    if len(args) > 2:
        usage(params)
        sys.exit()

    logger = logging.getLogger('snapcast_mpris')
    logger.propagate = False
    logger.setLevel(log_level)

    # Attempt to configure systemd journal logging, if enabled
    if log_journal:
        try:
            from systemd.journal import JournalHandler
            log_handler = JournalHandler(SYSLOG_IDENTIFIER='snapcast_mpris')
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

    for p in ['host', 'port', 'password', 'bus_name', 'client']:
        if not params[p]:
            params[p] = defaults[p]

    if '@' in params['host']:
        params['password'], params['host'] = params['host'].rsplit('@', 1)

    params['host'] = os.path.expanduser(params['host'])

    # for p in ['mmkeys', 'notify']:
    #     if config.has_option('Bling', p):
    #         params[p] = config.getboolean('Bling', p)

    # if config.has_option('Bling', 'notify_urgency'):
    #     params['notify_urgency'] = int(config.get('Bling', 'notify_urgency'))

    if params['client'] is None:
        hostname = socket.gethostname()
        logger.info(
            f'No client id specified, trying to find a client running on host "{hostname}"')
        resp = requests.post(f'http://{params["host"]}:{params["port"]}/jsonrpc', json={
            "id": 1, "jsonrpc": "2.0", "method": "Server.GetStatus"})
        if resp.ok:
            params['client'] = __get_client_from_server_status(
                json.loads(resp.text))

    if params['client'] is None:
        logger.error('Client not found or not configured')

    logger.info(f'Client: {params["client"]}')
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
    # wrapper = SnapcastRpcWebsocketWrapper("127.0.0.1", 1780, "id", any)
    snapcast_wrapper = MPDWrapper(params)
    # mpd_wrapper = MPDWrapper(params)
    # mpd_wrapper.run()

    # Run idle loop
    try:
        loop.run()
    except KeyboardInterrupt:
        logger.debug('Caught SIGINT, exiting.')

    # Clean up
    # try:
    #     mpd_wrapper.client.close()
    #     mpd_wrapper.client.disconnect()
    #     logger.debug('Exiting')
    # except mpd.ConnectionError:
    #     logger.error('Failed to disconnect properly')
