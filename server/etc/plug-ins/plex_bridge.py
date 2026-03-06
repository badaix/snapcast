#!/usr/bin/env python3

import json
import sys
import logging
import argparse
import requests
import time
from typing import Dict, Any, Optional, Union
from plexapi.server import PlexServer
from plexapi.client import PlexClient
from plexapi.exceptions import NotFound
import threading

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    filename="/tmp/plex_bridge.log"
)


class PlexSnapcastBridge:
    def __init__(self, config: Dict[str, Union[str, Optional[str]]]):
        self.config = config
        self.plex: Optional[PlexServer] = None
        self.current_player: Optional[PlexClient] = None
        self.snapcast_url: Optional[str] = None
        self.last_metadata: Dict[str, Any] = {}
        self.running = True
        if self.config.get("snapcast_host") and self.config.get("snapcast_port"):
            self.snapcast_url = f"http://{self.config['snapcast_host']}:{self.config['snapcast_port']}/jsonrpc"
        self.connect_to_plex()
        self.send_ready_notification()
        self.monitor_thread = threading.Thread(
            target=self.monitor_sessions, daemon=True)
        self.monitor_thread.start()

    def connect_to_plex(self) -> None:
        try:
            baseurl = f"http://{self.config['ip']}:32400"
            token = self.config["token"]
            self.plex = PlexServer(baseurl, token)
            self.current_player = self.plex.client(self.config["player"])
            self.send_log("Connected to Plex server")
        except NotFound:
            self.send_log(f"Player {self.config['player']} not found", "error")
            raise
        except Exception as e:
            self.send_log(f"Failed to connect to Plex: {str(e)}", "error")
            raise

    def update_snapcast_status(self, properties: Dict[str, Any]) -> None:
        if not self.snapcast_url:
            return

        try:
            payload = {
                "id": 1,
                "jsonrpc": "2.0",
                "method": "Stream.OnProperties",
                "params": {
                    "id": self.config.get("stream", "Plex"),
                    "properties": properties
                }
            }
            requests.post(self.snapcast_url, json=payload)
        except requests.exceptions.RequestException as e:
            self.send_log(
                f"Failed to update Snapcast status: {str(e)}", "error")

    def send_ready_notification(self) -> None:
        notification = {
            "jsonrpc": "2.0",
            "method": "Plugin.Stream.Ready"
        }
        self._send_json(notification)

    def _send_json(self, data: Dict[str, Any]) -> None:
        try:
            json_str = json.dumps(data)
            print(json_str, flush=True)
            logging.debug(f"Sent: {json_str}")
        except Exception as e:
            logging.error(f"Error sending JSON: {str(e)}")

    def send_properties_notification(self, properties: Dict[str, Any]) -> None:
        notification = {
            "jsonrpc": "2.0",
            "method": "Plugin.Stream.Player.Properties",
            "params": properties
        }
        self._send_json(notification)
        self.update_snapcast_status(properties)

    def send_log(self, message: str, severity: str = "info") -> None:
        notification = {
            "jsonrpc": "2.0",
            "method": "Plugin.Stream.Log",
            "params": {
                "severity": severity,
                "message": message
            }
        }
        self._send_json(notification)

    def get_current_properties(self) -> Dict[str, Any]:
        if not self.current_player or not self.plex:
            return self.get_default_properties()

        try:
            metadata: Dict[str, Any] = {}
            playback_status = "stopped"

            now_playing = self.plex.sessions()
            player_name = self.config["player"]

            active_session = None
            for session in now_playing:
                if hasattr(session, "players") and session.players:
                    if session.players[0].title == player_name:
                        active_session = session
                        raw_state = getattr(
                            session.players[0], "state", "stopped")
                        playback_status = "playing" if raw_state == "playing" else "paused"
                        break

            if active_session:
                art_url = None
                if hasattr(active_session, "thumb") and active_session.thumb:
                    art_url = f"http://{self.config['ip']}:32400{active_session.thumb}?X-Plex-Token={self.config['token']}"

                metadata = {
                    "title": getattr(active_session, "title", "Unknown Title"),
                    "artist": [getattr(active_session, "originalTitle", None) or
                               getattr(active_session, "grandparentTitle", "Unknown Artist")],
                    "album": getattr(active_session, "parentTitle", "Unknown Album"),
                    "duration": getattr(active_session, "duration", 0) / 1000,
                    "trackNumber": int(getattr(active_session, "index", 0)) if getattr(active_session, "index", None) else 0,
                    "artUrl": art_url
                }

                if art_url:
                    metadata["artUrl"] = art_url
                if hasattr(active_session, "key"):
                    metadata["url"] = f"http://{self.config['ip']}:32400{active_session.key}?X-Plex-Token={self.config['token']}"

            return {
                "canControl": True,
                "canGoNext": True,
                "canGoPrevious": True,
                "canPlay": True,
                "canPause": True,
                "canSeek": False,
                "playbackStatus": playback_status,
                "loopStatus": "none",
                "shuffle": False,
                "volume": getattr(self.current_player, "volume", 100),
                "mute": getattr(self.current_player, "isMuted", False),
                "position": getattr(self.current_player, "viewOffset", 0) / 1000,
                "rate": 1.0,
                "metadata": metadata
            }

        except Exception as e:
            self.send_log(f"Error getting properties: {str(e)}", "error")
            return self.get_default_properties()

    def get_default_properties(self) -> Dict[str, Any]:
        return {
            "canControl": True,
            "canGoNext": True,
            "canGoPrevious": True,
            "canPlay": True,
            "canPause": True,
            "canSeek": False,
            "playbackStatus": "stopped",
            "loopStatus": "none",
            "shuffle": False,
            "volume": 100,
            "mute": False,
            "position": 0
        }

    def handle_control_command(self, command: str, params: Dict[str, Any]) -> Dict[str, Any]:
        try:
            if self.current_player is None:
                raise Exception("No player connected")

            self.send_log(
                f"Executing command: {command} with params: {params}", "debug")

            if command == "play":
                self.current_player.play()
            elif command == "pause":
                self.current_player.pause()
            elif command == "stop":
                self.current_player.stop()
            elif command == "next":
                self.current_player.skipNext()
            elif command == "previous":
                self.current_player.skipPrevious()
            elif command == "seek":
                if "offset" in params:
                    current_time = getattr(
                        self.current_player, "viewOffset", 0)
                    new_time = int(
                        (current_time / 1000 + params["offset"]) * 1000)
                    self.current_player.seekTo(new_time)
            elif command == "setPosition":
                if "position" in params:
                    new_time = int(params["position"] * 1000)
                    self.current_player.seekTo(new_time)

            time.sleep(0.2)
            properties = self.get_current_properties()
            self.send_properties_notification(properties)
            return {"status": "ok"}

        except Exception as e:
            self.send_log(
                f"Error executing control command ({type(e).__name__}): {str(e)}", "error")
            return {"error": {"code": -32000, "message": str(e)}}

    def handle_set_property(self, property_name: str, value: Any) -> Dict[str, Any]:
        try:
            if self.current_player is None:
                raise Exception("No player connected")
            if property_name == "volume":
                self.current_player.setVolume(value)
            elif property_name == "mute":
                if value:
                    self.current_player.mute()
                else:
                    self.current_player.unmute()

            self.send_properties_notification(self.get_current_properties())
            return {"status": "ok"}
        except Exception as e:
            self.send_log(f"Error setting property: {str(e)}", "error")
            return {"error": {"code": -32000, "message": str(e)}}

    def handle_command(self, command: Dict[str, Any]) -> Dict[str, Any]:
        try:
            method = command.get("method")
            cmd_id = command.get("id")
            response: Dict[str, Any] = {
                "jsonrpc": "2.0",
                "id": cmd_id
            }

            if method == "Plugin.Stream.Player.Control":
                params = command.get("params", {})
                result = self.handle_control_command(
                    params.get("command", ""), params.get("params", {}))
                response["result"] = result
            elif method == "Plugin.Stream.Player.SetProperty":
                params = command.get("params", {})
                property_name = next(iter(params.keys()))
                result = self.handle_set_property(
                    property_name, params[property_name])
                response["result"] = result
            elif method == "Plugin.Stream.Player.GetProperties":
                response["result"] = self.get_current_properties()
            else:
                response["error"] = {
                    "code": -32601,
                    "message": f"Method not found: {method}"
                }

            return response
        except Exception as e:
            self.send_log(f"Error processing command: {str(e)}", "error")
            return {
                "jsonrpc": "2.0",
                "id": command.get("id"),
                "error": {
                    "code": -32000,
                    "message": str(e)
                }
            }

    def monitor_sessions(self) -> None:
        while self.running:
            try:
                properties = self.get_current_properties()
                current_metadata = properties.get("metadata", {})

                if current_metadata != self.last_metadata:
                    self.send_log(
                        "Track changed, updating properties", "debug")
                    self.send_properties_notification(properties)
                    self.last_metadata = current_metadata.copy()

                time.sleep(1)
            except Exception as e:
                self.send_log(f"Error in session monitor: {str(e)}", "error")
                time.sleep(1)

    def run(self) -> None:
        self.send_log("Starting Plex bridge")
        try:
            while self.running:
                try:
                    line = sys.stdin.readline()
                    if not line:
                        self.send_log("Received EOF, exiting", "notice")
                        break

                    command = json.loads(line)
                    logging.debug(f"Received command: {command}")
                    response = self.handle_command(command)
                    self._send_json(response)
                except json.JSONDecodeError as e:
                    self.send_log(f"Invalid JSON received: {str(e)}", "error")
                except Exception as e:
                    self.send_log(
                        f"Error processing command: {str(e)}", "error")
        finally:
            self.running = False
            self.monitor_thread.join(timeout=1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plex to Snapcast Bridge")
    parser.add_argument("--token", type=str, required=True, help="Plex token")
    parser.add_argument("--ip", type=str, required=True,
                        help="Plex server IP address")
    parser.add_argument("--player", type=str, required=True,
                        help="Plex player name")
    parser.add_argument("--stream", type=str, help="Stream ID")
    parser.add_argument("--snapcast-port", type=str, help="Snapcast HTTP port")
    parser.add_argument("--snapcast-host", type=str, help="Snapcast HTTP host")

    args = parser.parse_args()

    try:
        config: Dict[str, Union[str, Optional[str]]] = {
            "token": args.token,
            "ip": args.ip,
            "player": args.player,
            "stream": args.stream,
            "snapcast_host": args.snapcast_host,
            "snapcast_port": args.snapcast_port
        }
        bridge = PlexSnapcastBridge(config)
        bridge.run()
    except Exception as e:
        logging.error(f"Fatal error: {str(e)}")
        sys.exit(1)
