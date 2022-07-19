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
# - websocket-client

from time import sleep
import websocket
import logging
import threading
import json
import webbrowser
import os
import sys
import getopt
import time
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
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
    </property>
  </interface>
</node>"""


tag_mapping = {
    'trackId': ['mpris:trackid', lambda val: dbus.ObjectPath(f'/org/mpris/MediaPlayer2/Track/{val}')],
    'artUrl': ['mpris:artUrl', str],
    'duration': ['mpris:length', lambda val: int(val * 1000000)],
    'album': ['xesam:album', str],
    'albumArtist': ['xesam:albumArtist', list],
    'artist': ['xesam:artist', list],
    'asText': ['xesam:asText', str],
    'audioBPM': ['xesam:audioBPM', int],
    'autoRating': ['xesam:autoRating', float],
    'comment': ['xesam:comment', list],
    'composer': ['xesam:composer', list],
    'contentCreated': ['xesam:contentCreated', str],
    'discNumber': ['xesam:discNumber', int],
    'firstUsed': ['xesam:firstUsed', str],
    'genre': ['xesam:genre', list],
    'lastUsed': ['xesam:lastUsed', str],
    'lyricist': ['xesam:lyricist', str],
    'title': ['xesam:title', str],
    'trackNumber': ['xesam:trackNumber', int],
    'url': ['xesam:url', str],
    'useCount': ['xesam:useCount', int],
    'userRating': ['xesam:userRating', float]
}


property_mapping = {
    'playbackStatus': 'PlaybackStatus',
    'loopStatus': 'LoopStatus',
    'shuffle': 'Shuffle',
    'volume': 'Volume',
    'canGoNext': 'CanGoNext',
    'canGoPrevious': 'CanGoPrevious',
    'canPlay': 'CanPlay',
    'canPause': 'CanPause',
    'canSeek': 'CanSeek',
    'canControl': 'CanControl'
}


class SnapcastWrapper(object):
    """ Wrapper of mpd.MPDClient to handle socket
        errors and similar
    """

    def __init__(self, params):
        self._dbus = dbus
        self._params = params
        self._dbus_service = None

        self._metadata = {}
        self._properties = {}
        self._req_id = 0
        self._stream_id = ''
        self._request_map = {}

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
            new_meta = self._dbus_service.update_property('org.mpris.MediaPlayer2.Player',
                                                          'Metadata')
            logger.debug(f'new meta {new_meta}')
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
            for key, value in changed_properties.items():
                if key in property_mapping:
                    self._dbus_service.update_property(
                        'org.mpris.MediaPlayer2.Player', property_mapping[key])
            if 'metadata' in changed_properties:
                self.__update_metadata(props.get('metadata', None))
        except Exception as e:
            logger.error(f'Error in update_properties: {str(e)}')

    def on_ws_message(self, ws, message):
        # TODO: error handling
        logger.info(f'Snapcast RPC websocket message received: {message}')
        jmsg = json.loads(message)
        if 'id' in jmsg:
            id = jmsg['id']
            if id in self._request_map:
                request = self._request_map[id]
                del self._request_map[id]
                logger.info(f'Received response to {request}')
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

        # Export our DBUS service
        if not self._dbus_service:
            self._dbus_service = MPRISInterface(self._params)
        else:
            # Add our service to the session bus
            # self._dbus_service.add_to_connection(dbus.SessionBus(),
            #    '/org/mpris/MediaPlayer2')
            self._dbus_service.acquire_name()

        self.send_request("Server.GetStatus")

    def on_ws_close(self, ws):
        logger.info("Snapcast RPC websocket closed")
        if self._dbus_service is not None:
            self._dbus_service.release_name()
        # self._dbus_service.remove_from_connection()

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

    def stop(self):
        self.websocket.keep_running = False
        logger.info("Waiting for websocket thread to exit")
        # self.websocket_thread.join()

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
        self.send_request("Stream.Control", {
                          "id": self._stream_id, "command": command, "params": params})

    def set_property(self, property, value):
        logger.info(f'set_property {property} = {value}')
        self.send_request("Stream.SetProperty", {
                          "id": self._stream_id, "property": property, "value": value})

    @property
    def metadata(self):
        return self._metadata

    @property
    def properties(self):
        return self._properties

    def position(self):
        logger.debug(
            f'Position props: {self._properties}, meta: {self._metadata}')
        if not 'position' in self._properties:
            return 0
        if not 'mpris:length' in self._metadata:
            return 0
        position = self._properties['position']
        if '_received' in self._properties:
            position += (time.time() - self._properties['_received'])
        return position * 1000000

    def property(self, name, default):
        return self._properties.get(name, default)

    def notify_about_track(self, meta, state='play'):
        uri = 'sound'
        # if 'mpris:artUrl' in meta:
        #     uri = meta['mpris:artUrl']

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

    #     if 'elapsed' in new_status:
    #         new_position = float(new_status['elapsed'])
    #     elif 'time' in new_status:
    #         new_position = int(new_status['time'].split(':')[0])
    #     else:
    #         new_position = 0

    #     self._position = new_position

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
            gsd_object.GrabMediaPlayerKeys("snapcast_mpris", 0,
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
                notif.set_hint("desktop-entry",
                               GLib.Variant("s", "snapcast_mpris"))
                notif.set_hint("transient", GLib.Variant("b", True))
            else:
                logger.error(
                    "Failed to init libnotify; disabling notifications")
        elif using_old_notify:
            logger.debug("Initializing old pynotify")
            if pynotify.init(identity):
                notif = pynotify.Notification("", "", "")
                notif.set_hint("desktop-entry", "snapcast_mpris")
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
        "CanRaise": (True, None),
        "DesktopEntry": ("snapcast_mpris", None),
        "HasTrackList": (False, None),
        "Identity": (identity, None),
        "SupportedUriSchemes": (dbus.Array(signature="s"), None),
        "SupportedMimeTypes": (dbus.Array(signature="s"), None)
    }

    def __get_playback_status():
        status = snapcast_wrapper.property('playbackStatus', 'stopped')
        logger.debug(f'get_playback_status "{status}"')
        return {'playing': 'Playing', 'paused': 'Paused', 'stopped': 'Stopped'}[status]

    def __set_loop_status(value):
        logger.debug(f'set_loop_status "{value}"')
        snapcast_wrapper.set_property(
            'loopStatus', {'None': 'none', 'Track': 'track', 'Playlist': 'playlist'}[value])
        return

    def __get_loop_status():
        status = snapcast_wrapper.property('loopStatus', 'none')
        logger.debug(f'get_loop_status "{status}"')
        return {'none': 'None', 'track': 'Track', 'playlist': 'Playlist'}[status]

    def __set_shuffle(value):
        logger.debug(f'set_shuffle "{value}"')
        snapcast_wrapper.set_property('shuffle', bool(value))
        return

    def __get_shuffle():
        shuffle = snapcast_wrapper.property('shuffle', False)
        logger.debug(f'get_shuffle "{shuffle}"')
        return shuffle

    def __get_metadata():
        logger.debug(f'get_metadata: {snapcast_wrapper.metadata}')
        return dbus.Dictionary(snapcast_wrapper.metadata, signature='sv')

    def __get_volume():
        volume = snapcast_wrapper.property('volume', 100)
        logger.debug(f'get_volume "{volume}"')
        if volume > 0:
            return volume / 100.0
        else:
            return 0.0

    def __set_volume(value):
        logger.debug(f'set_voume: {value}')
        if value >= 0 and value <= 1:
            snapcast_wrapper.set_property('volume', int(value * 100))
        return

    def __get_position():
        position = snapcast_wrapper.position()
        logger.debug(f'get_position: {position}')
        return dbus.Int64(position)

    def __get_can_go_next():
        can_go_next = snapcast_wrapper.property('canGoNext', False)
        logger.debug(f'get_can_go_next "{can_go_next}"')
        return can_go_next

    def __get_can_go_previous():
        can_go_previous = snapcast_wrapper.property('canGoPrevious', False)
        logger.debug(f'get_can_go_previous "{can_go_previous}"')
        return can_go_previous

    def __get_can_play():
        can_play = snapcast_wrapper.property('canPlay', False)
        logger.debug(f'get_can_play "{can_play}"')
        return can_play

    def __get_can_pause():
        can_pause = snapcast_wrapper.property('canPause', False)
        logger.debug(f'get_can_pause "{can_pause}"')
        return can_pause

    def __get_can_seek():
        can_seek = snapcast_wrapper.property('canSeek', False)
        logger.debug(f'get_can_seek "{can_seek}"')
        return can_seek

    def __get_can_control():
        can_control = snapcast_wrapper.property('canControl', False)
        logger.debug(f'get_can_control "{can_control}"')
        return can_control

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
        "CanGoNext": (__get_can_go_next, None),
        "CanGoPrevious": (__get_can_go_previous, None),
        "CanPlay": (__get_can_play, None),
        "CanPause": (__get_can_pause, None),
        "CanSeek": (__get_can_seek, None),
        "CanControl": (__get_can_control, None),
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
        logger.debug('Raise')
        webbrowser.open(url=f'http://{params["host"]}:{params["port"]}', new=1)
        return

    @ dbus.service.method(__root_interface, in_signature='', out_signature='')
    def Quit(self):
        logger.debug('Quit')
        return

    # Player methods
    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Next(self):
        snapcast_wrapper.control("next")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Previous(self):
        snapcast_wrapper.control("previous")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Pause(self):
        snapcast_wrapper.control("pause")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def PlayPause(self):
        snapcast_wrapper.control("playPause")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Stop(self):
        snapcast_wrapper.control("stop")
        return

    @ dbus.service.method(__player_interface, in_signature='', out_signature='')
    def Play(self):
        snapcast_wrapper.control("play")
        return

    @ dbus.service.method(__player_interface, in_signature='x', out_signature='')
    def Seek(self, offset):
        logger.debug(f'Seek {offset}')
        snapcast_wrapper.control("seek", {"offset": float(offset) / 1000000})
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
        logger.debug(f'setPosition trackId: {trackid}, position: {position}')
        snapcast_wrapper.control(
            "setPosition", {"position": float(position) / 1000000})
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
        logger.debug('OpenUri')
        # TODO
        return

    # Player signals
    @ dbus.service.signal(__player_interface, signature='x')
    def Seeked(self, position):
        logger.debug(f'Seeked to {position}')
        snapcast_wrapper.properties['position'] = float(position) / 1000000
        return float(position)


def __get_client_from_server_status(status):
    client_id = None
    last_seen = 0
    try:
        for group in status['result']['server']['groups']:
            for client in group['clients']:
                if client['host']['name'] == hostname:
                    active = client["connected"]
                    logger.info(
                        f'Client with id "{client["id"]}" active: {active}')
                    if int(client["lastSeen"]["sec"]) > last_seen:
                        client_id = client['id']
                        last_seen = int(client["lastSeen"]["sec"])
                    if active:
                        return client_id
    except:
        logger.error('Failed to parse server status')
    return client_id


def usage(params):
    print("""\
Usage: %(progname)s [OPTION]...

     -h, --host=ADDR        Set the mpd server address
     -p, --port=PORT        Set the TCP port
         --client=ID        Set the client id
     -d, --debug            Run in debug mode
     -v, --version          snapcast_mpris version

Report bugs to https://github.com/badaix/snapcast/issues""" % params)


if __name__ == '__main__':
    DBusGMainLoop(set_as_default=True)

    # TODO:
    # -cleanup: remove mpd-ish stuff
    # -use zeroconf to find the snapserver IP and port

    gettext.bindtextdomain('snapcast_mpris', '@datadir@/locale')
    gettext.textdomain('snapcast_mpris')

    log_format_stderr = '%(asctime)s %(module)s %(levelname)s: %(message)s'

    log_level = logging.INFO

    # Parse command line
    try:
        (opts, args) = getopt.getopt(sys.argv[1:], 'dh:p:v',
                                     ['help', 'bus-name=',
                                     'debug', 'host=', 'client=',
                                      'port=', 'version'])
    except getopt.GetoptError as ex:
        (msg, opt) = ex.args
        print("%s: %s" % (sys.argv[0], msg), file=sys.stderr)
        print(file=sys.stderr)
        usage(params)
        sys.exit(2)

    for (opt, arg) in opts:
        print(f"opt: {opt}, arg: {arg}")
        if opt in ['--help']:
            usage(params)
            sys.exit()
        elif opt in ['--bus-name']:
            params['bus_name'] = arg
        elif opt in ['-d', '--debug']:
            log_level = logging.DEBUG
        elif opt in ['-h', '--host']:
            params['host'] = arg
        elif opt in ['-p', '--port']:
            params['port'] = int(arg)
        elif opt in ['--client']:
            params['client'] = arg
        elif opt in ['-v', '--version']:
            v = __version__
            if __git_version__:
                v = __git_version__
            print("snapcast_mpris version %s" % v)
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

    for p in ['host', 'port', 'password', 'bus_name', 'client']:
        if not params[p]:
            params[p] = defaults[p]

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
        sys.exit()

    logger.info(f'Client: {params["client"]}')
    logger.debug(f'Parameters: {params}')

    # Set up the main loop
    if using_gi_glib:
        logger.debug('Using GObject-Introspection main loop.')
    else:
        logger.debug('Using legacy pygobject2 main loop.')
    loop = GLib.MainLoop()

    # Wrapper to send notifications
    notification = NotifyWrapper(params)

    snapcast_wrapper = SnapcastWrapper(params)

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
