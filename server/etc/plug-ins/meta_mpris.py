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
# Stream plugin that proxies a local MPRIS player to Snapcast.
#
# Reads/writes MPRIS over the session D-Bus and speaks the
# Plugin.Stream.Player JSON-RPC protocol on stdin/stdout, similar to
# meta_mpd.py but without talking to MPD at all.
#
# Dependencies:
#   - python-dbus (dbus-python)
#   - PyGObject / GLib (python3-gobject)
#

import sys
import os
import json
import getopt
import logging
import fcntl
import base64
from typing import Any, Dict, List, Optional
from urllib.parse import unquote, urlparse

import dbus
from dbus.mainloop.glib import DBusGMainLoop

try:
    from gi.repository import GLib
    using_gi_glib = True
except ImportError:
    import glib as GLib  # type: ignore
    using_gi_glib = False


__version__ = "@version@"
__git_version__ = "@gitversion@"

logger = logging.getLogger("meta_mpris")

params: Dict[str, Any] = {
    "progname": sys.argv[0],
    "snapcast-host": None,
    "snapcast-port": None,
    "stream": None,
    "mpris-name": None,
}

defaults: Dict[str, Any] = {
    "snapcast-host": "localhost",
    "snapcast-port": 1780,
    "stream": "default",
    "mpris-name": None,
}

DISCONNECTED_STATUS: Dict[str, Any] = {
    "playbackStatus": "stopped",
    "loopStatus": "none",
    "shuffle": False,
    "rate": 1.0,
    "volume": 100,
    "position": 0.0,
    "canGoNext": False,
    "canGoPrevious": False,
    "canPlay": False,
    "canPause": False,
    "canSeek": False,
    "canControl": False,
}


def send(json_msg: Dict[str, Any]) -> None:
    """Send a JSON-RPC message to Snapserver (stdout, newline-delimited)."""
    print(json.dumps(json_msg), flush=True)


# ---------- MPRIS helpers (stateless) ----------

def find_mpris_names(bus: dbus.bus.BusConnection) -> List[str]:
    """Return all org.mpris.MediaPlayer2.* bus names currently on the session bus."""
    dbus_obj = bus.get_object("org.freedesktop.DBus", "/org/freedesktop/DBus")
    dbus_iface = dbus.Interface(dbus_obj, "org.freedesktop.DBus")
    return [
        str(n)
        for n in dbus_iface.ListNames()
        if isinstance(n, str) and n.startswith("org.mpris.MediaPlayer2.")
    ]


def select_player(bus: dbus.bus.BusConnection, requested: Optional[str]) -> Optional[str]:
    """Pick an MPRIS player name, or None if nothing suitable is available."""
    names = find_mpris_names(bus)
    if not names:
        return None

    if requested:
        full = requested if requested.startswith("org.mpris.MediaPlayer2.") else "org.mpris.MediaPlayer2." + requested
        if full in names:
            return full
        logger.warning("Requested MPRIS player '%s' not found, falling back to autodetect", full)

    for name in names:
        try:
            obj = bus.get_object(name, "/org/mpris/MediaPlayer2")
            props = dbus.Interface(obj, "org.freedesktop.DBus.Properties")
            status = str(props.Get("org.mpris.MediaPlayer2.Player", "PlaybackStatus"))
            if status in ("Playing", "Paused"):
                return name
        except Exception:
            continue

    return names[0]


def file_url_to_art_data(file_url: str) -> Optional[Dict[str, str]]:
    """Read a file:// URL and return an artData dict with base64 data + extension."""
    try:
        parsed = urlparse(file_url)
        path = unquote(parsed.path)
        if not os.path.isfile(path):
            logger.warning("Art file not found: %s", path)
            return None
        ext = os.path.splitext(path)[1].lstrip(".")
        if not ext:
            ext = "png"
        with open(path, "rb") as f:
            data = base64.b64encode(f.read()).decode("ascii")
        return {"data": data, "extension": ext}
    except Exception as e:
        logger.error("Failed to read art file %s: %s", file_url, str(e))
        return None


def volume_to_percent(volume: Any) -> int:
    try:
        v = float(volume)
    except Exception:
        return 100
    if v <= 0:
        return 0
    if v >= 1:
        return 100
    return int(round(v * 100))


def percent_to_volume(percent: Any) -> float:
    try:
        p = int(percent)
    except Exception:
        return 1.0
    if p <= 0:
        return 0.0
    if p >= 100:
        return 1.0
    return float(p) / 100.0


def position_to_seconds(position: Any) -> float:
    """MPRIS Position is microseconds (int64)."""
    try:
        return int(position) / 1_000_000.0
    except Exception:
        return 0.0


def metadata_to_snap(meta: dbus.Dictionary) -> Dict[str, Any]:
    """Convert MPRIS/xesam metadata to Snapcast stream metadata keys."""
    snap: Dict[str, Any] = {}

    def get_str(key: str) -> Optional[str]:
        val = meta.get(key)
        if isinstance(val, (dbus.String, str)):
            return str(val)
        return None

    def get_str_list(key: str) -> Optional[list]:
        val = meta.get(key)
        if isinstance(val, (dbus.Array, list)):
            return [str(v) for v in val]
        if isinstance(val, (dbus.String, str)):
            return [str(val)]
        return None

    track_id = get_str("mpris:trackid")
    if track_id is not None:
        snap["trackId"] = track_id

    url = get_str("xesam:url")
    if url is not None:
        snap["url"] = url

    length = meta.get("mpris:length")
    if length is not None:
        try:
            snap["duration"] = int(length) / 1_000_000.0
        except Exception:
            pass

    title = get_str("xesam:title")
    if title is not None:
        snap["title"] = title

    album = get_str("xesam:album")
    if album is not None:
        snap["album"] = album

    artist = get_str_list("xesam:artist")
    if artist is not None:
        snap["artist"] = artist

    album_artist = get_str_list("xesam:albumArtist")
    if album_artist is not None:
        snap["albumArtist"] = album_artist

    genre = get_str_list("xesam:genre")
    if genre is not None:
        snap["genre"] = genre

    art_url = get_str("mpris:artUrl")
    if art_url is not None:
        if art_url.startswith("file://"):
            art_data = file_url_to_art_data(art_url)
            if art_data is not None:
                snap["artData"] = art_data
            else:
                snap["artUrl"] = art_url
        else:
            snap["artUrl"] = art_url

    return snap


def get_player_status(props_iface: dbus.Interface) -> Dict[str, Any]:
    """Read all Player properties and return a Snapcast-style status dict."""
    props = props_iface.GetAll("org.mpris.MediaPlayer2.Player")

    playback_status = str(props.get("PlaybackStatus", "Stopped"))
    playback_map = {"Playing": "playing", "Paused": "paused", "Stopped": "stopped"}
    loop_status = str(props.get("LoopStatus", "None"))
    loop_map = {"None": "none", "Track": "track", "Playlist": "playlist"}

    snap: Dict[str, Any] = {
        "playbackStatus": playback_map.get(playback_status, "stopped"),
        "loopStatus": loop_map.get(loop_status, "none"),
        "shuffle": bool(props.get("Shuffle", False)),
        "rate": float(props.get("Rate", 1.0)),
        "volume": volume_to_percent(props.get("Volume", 1.0)),
        "position": position_to_seconds(props.get("Position", dbus.Int64(0))),
        "canGoNext": True,
        "canGoPrevious": True,
        "canPlay": bool(props.get("CanPlay", False)),
        "canPause": bool(props.get("CanPause", False)),
        "canSeek": bool(props.get("CanSeek", False)),
        "canControl": bool(props.get("CanControl", False)),
    }

    metadata = props.get("Metadata")
    if isinstance(metadata, dbus.Dictionary):
        snap["metadata"] = metadata_to_snap(metadata)

    return snap


class MprisProxy:
    """Manages a connection to an MPRIS player.

    Tolerates starting without a player and (re)connects when one appears.
    """

    def __init__(self, bus: dbus.bus.BusConnection, target_name: Optional[str]) -> None:
        self._bus = bus
        self._target_name = target_name
        self._last_status: Optional[Dict[str, Any]] = None

        # Current connection state (None = disconnected)
        self._name: Optional[str] = None
        self._player: Optional[dbus.proxies.ProxyObject] = None
        self._props: Optional[dbus.Interface] = None
        self._player_iface: Optional[dbus.Interface] = None
        self._signal_match: Optional[dbus.connection.SignalMatch] = None

        # Watch for new MPRIS players appearing on the bus
        self._bus.add_signal_receiver(
            self._on_name_owner_changed,
            signal_name="NameOwnerChanged",
            dbus_interface="org.freedesktop.DBus",
        )

        self._try_connect()

    @property
    def connected(self) -> bool:
        return self._name is not None

    def _try_connect(self) -> bool:
        """Attempt to find and connect to an MPRIS player. Returns True on success."""
        if self.connected:
            return True

        name = select_player(self._bus, self._target_name)
        if name is None:
            logger.info("No MPRIS player found yet, will keep scanning...")
            return False

        try:
            player = self._bus.get_object(name, "/org/mpris/MediaPlayer2")
            props = dbus.Interface(player, "org.freedesktop.DBus.Properties")
            player_iface = dbus.Interface(player, "org.mpris.MediaPlayer2.Player")

            # Verify we can actually read from it
            props.GetAll("org.mpris.MediaPlayer2.Player")

            self._name = name
            self._player = player
            self._props = props
            self._player_iface = player_iface

            self._signal_match = self._props.connect_to_signal(
                "PropertiesChanged", self._on_properties_changed
            )

            logger.info("Connected to MPRIS player: %s", name)
            return True
        except Exception as e:
            logger.warning("Failed to connect to %s: %s", name, str(e))
            self._disconnect()
            return False

    def _disconnect(self) -> None:
        """Clean up the current player connection."""
        if self._signal_match is not None:
            self._signal_match.remove()
            self._signal_match = None
        self._name = None
        self._player = None
        self._props = None
        self._player_iface = None

    def _on_name_owner_changed(self, name: str, old_owner: str, new_owner: str) -> None:
        """Fired when any bus name changes owner (appears/disappears)."""
        if not isinstance(name, str) or not name.startswith("org.mpris.MediaPlayer2."):
            return

        if self.connected and name == self._name and new_owner == "":
            logger.info("MPRIS player %s disappeared", name)
            self._disconnect()
            self._emit_status(DISCONNECTED_STATUS.copy())
            return

        if not self.connected and new_owner != "":
            logger.info("New MPRIS player appeared: %s, trying to connect...", name)
            if self._try_connect():
                status = self.get_status()
                self._emit_status(status)

    # ---------- Public API ----------

    def get_status(self) -> Dict[str, Any]:
        if not self.connected or self._props is None:
            return DISCONNECTED_STATUS.copy()
        try:
            return get_player_status(self._props)
        except dbus.exceptions.DBusException as e:
            logger.warning("DBus error reading status, disconnecting: %s", str(e))
            self._disconnect()
            return DISCONNECTED_STATUS.copy()

    def set_properties(self, updates: Dict[str, Any]) -> None:
        if not self.connected or self._props is None:
            logger.warning("No player connected, ignoring SetProperty")
            return
        try:
            for key, value in updates.items():
                if key == "loopStatus":
                    inv_map = {"none": "None", "track": "Track", "playlist": "Playlist"}
                    self._props.Set(
                        "org.mpris.MediaPlayer2.Player", "LoopStatus",
                        dbus.String(inv_map.get(str(value), "None")),
                    )
                elif key == "shuffle":
                    self._props.Set(
                        "org.mpris.MediaPlayer2.Player", "Shuffle",
                        dbus.Boolean(bool(value)),
                    )
                elif key == "volume":
                    self._props.Set(
                        "org.mpris.MediaPlayer2.Player", "Volume",
                        dbus.Double(percent_to_volume(value)),
                    )
                elif key == "rate":
                    self._props.Set(
                        "org.mpris.MediaPlayer2.Player", "Rate",
                        dbus.Double(float(value)),
                    )
        except dbus.exceptions.DBusException as e:
            logger.warning("DBus error in set_properties: %s", str(e))

    def control(self, command: str, cmd_params: Dict[str, Any]) -> None:
        if not self.connected or self._player_iface is None:
            logger.warning("No player connected, ignoring Control '%s'", command)
            return
        try:
            if command == "play":
                self._player_iface.Play()
            elif command == "pause":
                self._player_iface.Pause()
            elif command == "playPause":
                self._player_iface.PlayPause()
            elif command == "stop":
                self._player_iface.Stop()
            elif command == "next":
                self._player_iface.Next()
            elif command == "previous":
                self._player_iface.Previous()
            elif command == "seek":
                offset = float(cmd_params.get("offset", 0.0))
                self._player_iface.Seek(dbus.Int64(int(offset * 1_000_000)))
            elif command == "setPosition":
                position = float(cmd_params.get("position", 0.0))
                self._player_iface.SetPosition(
                    dbus.ObjectPath("/org/mpris/MediaPlayer2/Track/0"),
                    dbus.Int64(int(position * 1_000_000)),
                )
        except dbus.exceptions.DBusException as e:
            logger.warning("DBus error in control '%s': %s", command, str(e))

    def poll(self) -> None:
        """Periodic check: reconnect if needed, emit status if changed."""
        if not self.connected:
            if self._try_connect() and self.connected:
                status = self.get_status()
                self._emit_status(status)
            return

        try:
            status = self.get_status()
        except Exception:
            self._disconnect()
            self._emit_status(DISCONNECTED_STATUS.copy())
            return

        if self._last_status != status:
            self._emit_status(status)

    def _emit_status(self, status: Dict[str, Any]) -> None:
        self._last_status = status
        send({
            "jsonrpc": "2.0",
            "method": "Plugin.Stream.Player.Properties",
            "params": status,
        })

    def _on_properties_changed(self, interface: str, changed: Dict[str, Any], invalidated: Any) -> None:
        if interface != "org.mpris.MediaPlayer2.Player":
            return
        try:
            status = self.get_status()
            self._emit_status(status)
        except Exception:
            pass


class JsonRpcLoop:
    """Reads JSON-RPC commands from stdin, dispatches to MprisProxy."""

    def __init__(self, proxy: MprisProxy) -> None:
        self._proxy = proxy
        self._buffer = ""

        flags = fcntl.fcntl(sys.stdin.fileno(), fcntl.F_GETFL)
        flags |= os.O_NONBLOCK
        fcntl.fcntl(sys.stdin.fileno(), fcntl.F_SETFL, flags)
        GLib.io_add_watch(sys.stdin, GLib.IO_IN | GLib.IO_HUP, self._io_callback)

        send({"jsonrpc": "2.0", "method": "Plugin.Stream.Ready"})

        status = self._proxy.get_status()
        self._proxy._emit_status(status)

        GLib.timeout_add_seconds(1, self._poll_callback)

    def _poll_callback(self) -> bool:
        try:
            self._proxy.poll()
        except Exception as e:
            logger.error('Exception in poll_callback: "%s"', str(e))
        return True

    def _io_callback(self, fd, event) -> bool:  # type: ignore[override]
        try:
            if event & GLib.IO_HUP:
                logger.debug("stdin HUP, exiting IO loop")
                return False
            if event & GLib.IO_IN:
                chunk = fd.read()
                if not chunk:
                    return True
                for ch in chunk:
                    if ch == "\n":
                        line = self._buffer.strip()
                        self._buffer = ""
                        if line:
                            self._handle_line(line)
                    else:
                        self._buffer += ch
                return True
        except Exception as e:
            logger.error('Exception in io_callback: "%s"', str(e))
            return True

    def _handle_line(self, line: str) -> None:
        logger.info("Received: %s", line)
        req_id = None
        try:
            request = json.loads(line)
            req_id = request.get("id")
            method = request.get("method", "")
            if not isinstance(method, str):
                raise ValueError("Invalid method")

            interface, _, cmd = method.rpartition(".")
            if interface != "Plugin.Stream.Player":
                send({"jsonrpc": "2.0", "error": {"code": -32601, "message": "Method not found"}, "id": req_id})
                return

            if cmd == "Control":
                rpc_params = request.get("params", {})
                command = rpc_params.get("command")
                command_params = rpc_params.get("params", {}) or {}
                if not isinstance(command, str):
                    raise ValueError("Missing or invalid 'command'")
                self._proxy.control(command, command_params)
                send({"jsonrpc": "2.0", "result": "ok", "id": req_id})
            elif cmd == "SetProperty":
                updates = request.get("params", {})
                if not isinstance(updates, dict):
                    raise ValueError("params must be an object")
                self._proxy.set_properties(updates)
                send({"jsonrpc": "2.0", "result": "ok", "id": req_id})
            elif cmd == "GetProperties":
                status = self._proxy.get_status()
                send({"jsonrpc": "2.0", "id": req_id, "result": status})
            elif cmd == "GetMetadata":
                status = self._proxy.get_status()
                meta = status.get("metadata", {})
                send({"jsonrpc": "2.0", "id": req_id, "result": meta})
            else:
                send({"jsonrpc": "2.0", "error": {"code": -32601, "message": "Method not found"}, "id": req_id})
        except Exception as e:
            logger.exception("Error while handling JSON-RPC line")
            send({"jsonrpc": "2.0", "error": {"code": -32700, "message": "Parse or dispatch error", "data": str(e)}, "id": req_id})


def usage(p: Dict[str, Any]) -> None:
    print(
        """\
Usage: %(progname)s [OPTION]...

     --mpris-name=NAME       MPRIS bus name or suffix (e.g. 'firefox' or
                             'org.mpris.MediaPlayer2.firefox').
                             If omitted, the plugin auto-detects a suitable player.
     --snapcast-host=ADDR    (currently unused) Snapcast server address
     --snapcast-port=PORT    (currently unused) Snapcast server port
     --stream=ID             (currently unused) Stream id

     -h, --help              Show this help message
     -d, --debug             Run in debug mode
     -v, --version           meta_mpris version

Report bugs to https://github.com/snapcast/snapcast/issues"""
        % p
    )


def main() -> int:
    DBusGMainLoop(set_as_default=True)

    log_format_stderr = "%(asctime)s %(module)s %(levelname)s: %(message)s"
    log_level = logging.INFO

    try:
        (opts, args) = getopt.getopt(
            sys.argv[1:], "hdv",
            ["help", "mpris-name=", "snapcast-host=", "snapcast-port=", "stream=", "debug", "version"],
        )
    except getopt.GetoptError as ex:
        (msg, opt) = ex.args
        print("%s: %s" % (sys.argv[0], msg), file=sys.stderr)
        print(file=sys.stderr)
        usage(params)
        return 2

    for (opt, arg) in opts:
        if opt in ["-h", "--help"]:
            usage(params)
            return 0
        elif opt in ["--mpris-name"]:
            params["mpris-name"] = arg
        elif opt in ["--snapcast-host"]:
            params["snapcast-host"] = arg
        elif opt in ["--snapcast-port"]:
            params["snapcast-port"] = int(arg)
        elif opt in ["--stream"]:
            params["stream"] = arg
        elif opt in ["-d", "--debug"]:
            log_level = logging.DEBUG
        elif opt in ["-v", "--version"]:
            v = __version__
            if __git_version__:
                v = __git_version__
            print("meta_mpris version %s" % v)
            return 0

    if len(args) > 0:
        usage(params)
        return 2

    logger.propagate = False
    logger.setLevel(log_level)

    log_handler = logging.StreamHandler()
    log_handler.setFormatter(logging.Formatter(log_format_stderr))
    logger.addHandler(log_handler)

    for p in ["snapcast-host", "snapcast-port", "stream", "mpris-name"]:
        if params[p] is None:
            params[p] = defaults[p]

    logger.debug("Parameters: %s", params)

    bus = dbus.SessionBus()
    proxy = MprisProxy(bus, params["mpris-name"])
    JsonRpcLoop(proxy)

    loop = GLib.MainLoop()
    try:
        loop.run()
    except KeyboardInterrupt:
        logger.debug("Caught SIGINT, exiting.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
