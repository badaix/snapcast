#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# This file is part of snapcast
# Copyright (C) 2014-2025  Johannes Pohl
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


import logging
import json
import sys
import getopt
import logging

__version__ = "@version@"
__git_version__ = "@gitversion@"


identity = "Snapcast"

params = {
    "progname": sys.argv[0],
    # Connection
    "snapcast-host": None,
    "snapcast-port": None,
    "stream": None,
}

defaults = {
    # Connection
    "snapcast-host": "localhost",
    "snapcast-port": 1780,
    "stream": "default",
}


def send(json_msg):
    sys.stdout.write(json.dumps(json_msg))
    sys.stdout.write("\n")
    sys.stdout.flush()


logo_spotify = "PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+CjxzdmcgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIiBoZWlnaHQ9IjE2OHB4IiB3aWR0aD0iMTY4cHgiIHZlcnNpb249IjEuMSIgdmlld0JveD0iMCAwIDE2OCAxNjgiPgogPHBhdGggZmlsbD0iIzFFRDc2MCIgZD0ibTgzLjk5NiAwLjI3N2MtNDYuMjQ5IDAtODMuNzQzIDM3LjQ5My04My43NDMgODMuNzQyIDAgNDYuMjUxIDM3LjQ5NCA4My43NDEgODMuNzQzIDgzLjc0MSA0Ni4yNTQgMCA4My43NDQtMzcuNDkgODMuNzQ0LTgzLjc0MSAwLTQ2LjI0Ni0zNy40OS04My43MzgtODMuNzQ1LTgzLjczOGwwLjAwMS0wLjAwNHptMzguNDA0IDEyMC43OGMtMS41IDIuNDYtNC43MiAzLjI0LTcuMTggMS43My0xOS42NjItMTIuMDEtNDQuNDE0LTE0LjczLTczLjU2NC04LjA3LTIuODA5IDAuNjQtNS42MDktMS4xMi02LjI0OS0zLjkzLTAuNjQzLTIuODEgMS4xMS01LjYxIDMuOTI2LTYuMjUgMzEuOS03LjI5MSA1OS4yNjMtNC4xNSA4MS4zMzcgOS4zNCAyLjQ2IDEuNTEgMy4yNCA0LjcyIDEuNzMgNy4xOHptMTAuMjUtMjIuODA1Yy0xLjg5IDMuMDc1LTUuOTEgNC4wNDUtOC45OCAyLjE1NS0yMi41MS0xMy44MzktNTYuODIzLTE3Ljg0Ni04My40NDgtOS43NjQtMy40NTMgMS4wNDMtNy4xLTAuOTAzLTguMTQ4LTQuMzUtMS4wNC0zLjQ1MyAwLjkwNy03LjA5MyA0LjM1NC04LjE0MyAzMC40MTMtOS4yMjggNjguMjIyLTQuNzU4IDk0LjA3MiAxMS4xMjcgMy4wNyAxLjg5IDQuMDQgNS45MSAyLjE1IDguOTc2di0wLjAwMXptMC44OC0yMy43NDRjLTI2Ljk5LTE2LjAzMS03MS41Mi0xNy41MDUtOTcuMjg5LTkuNjg0LTQuMTM4IDEuMjU1LTguNTE0LTEuMDgxLTkuNzY4LTUuMjE5LTEuMjU0LTQuMTQgMS4wOC04LjUxMyA1LjIyMS05Ljc3MSAyOS41ODEtOC45OCA3OC43NTYtNy4yNDUgMTA5LjgzIDExLjIwMiAzLjczIDIuMjA5IDQuOTUgNy4wMTYgMi43NCAxMC43MzMtMi4yIDMuNzIyLTcuMDIgNC45NDktMTAuNzMgMi43Mzl6Ii8+Cjwvc3ZnPgo="
logo_snapcast = "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxNDMwIiBoZWlnaHQ9IjE0MzAiIHZpZXdCb3g9IjAgMCAxMzQwLjYyNSAxMzQwLjYyNSI+PGcgdHJhbnNmb3JtPSJtYXRyaXgoMS4wMzAyIDAgMCAxLjAzMDIgMTc1LjQ5NSA4Mi43MzIpIj48Y2lyY2xlIGN4PSItNzM0LjA5MyIgY3k9IjEzMC43ODQiIHI9IjQzMS4yNSIgc3R5bGU9ImZpbGw6I2ZmYzEwNztmaWxsLW9wYWNpdHk6MTtzdHJva2Utd2lkdGg6MTAuNDgxNiIgdHJhbnNmb3JtPSJyb3RhdGUoLTEyMCkiLz48cGF0aCBmaWxsPSJub25lIiBkPSJNLTQyMi4wNjYtNDcuMzM1YTM2MC4zIDM2MC4zIDAgMCAxIDAgMzYwLjI5N00tNDY2Ljg3NS0zLjM3NWEyOTkuOTggMjk5Ljk4IDAgMCAxIDAgMjcyLjM3N00tNTE0Ljc1OCAzNS4xM2EyNDAuMTcgMjQwLjE3IDAgMCAxIDAgMTk1LjM2OCIgc3R5bGU9ImZpbGw6IzAwMDtmaWxsLW9wYWNpdHk6MDtmaWxsLXJ1bGU6ZXZlbm9kZDtzdHJva2U6I2ZmZjtzdHJva2Utd2lkdGg6MzU7c3Ryb2tlLWxpbmVjYXA6cm91bmQ7c3Ryb2tlLWxpbmVqb2luOm1pdGVyO3N0cm9rZS1taXRlcmxpbWl0OjQ7c3Ryb2tlLWRhc2hhcnJheTpub25lO3N0cm9rZS1vcGFjaXR5OjEiIHRyYW5zZm9ybT0icm90YXRlKC0xMjApIi8+PHBhdGggZD0ibTU5MS42MDUgNDA3LjA4MS0xMTQuMzU5IDk4LjkxMkgzNjkuMDEyVjYzMy4yOGgxMDYuNTkzbDExNiAxMDAuMzR6IiBzdHlsZT0iZmlsbDojYzhjOGM4O2ZpbGwtb3BhY2l0eToxO2ZpbGwtcnVsZTpldmVub2RkO3N0cm9rZTojZmZmO3N0cm9rZS13aWR0aDozMy4zMzM7c3Ryb2tlLWxpbmVjYXA6cm91bmQ7c3Ryb2tlLWxpbmVqb2luOnJvdW5kO3N0cm9rZS1taXRlcmxpbWl0OjQ7c3Ryb2tlLWRhc2hhcnJheTpub25lO3N0cm9rZS1vcGFjaXR5OjEiLz48cGF0aCBmaWxsPSJub25lIiBkPSJNNzkyLjMzNSAzOTIuMjMyYTM2MC4zIDM2MC4zIDAgMCAxIDAgMzYwLjI5N003NDcuNTk0IDQzNi4xOTJhMjk5Ljk4IDI5OS45OCAwIDAgMSAwIDI3Mi4zNzdNNjk5LjcxIDQ3NC42OTZhMjQwLjE3IDI0MC4xNyAwIDAgMSAwIDE5NS4zNjgiIHN0eWxlPSJmaWxsOiMwMDA7ZmlsbC1vcGFjaXR5OjA7ZmlsbC1ydWxlOmV2ZW5vZGQ7c3Ryb2tlOiNmZmY7c3Ryb2tlLXdpZHRoOjM1O3N0cm9rZS1saW5lY2FwOnJvdW5kO3N0cm9rZS1saW5lam9pbjptaXRlcjtzdHJva2UtbWl0ZXJsaW1pdDo0O3N0cm9rZS1kYXNoYXJyYXk6bm9uZTtzdHJva2Utb3BhY2l0eToxIi8+PHBhdGggZmlsbD0ibm9uZSIgZD0iTTU2NS43NDMtODc5LjI1NGEzNjAuMyAzNjAuMyAwIDAgMSAwIDM2MC4yOTdNNTIxLjAwMi04MzUuMjk1YTI5OS45OCAyOTkuOTggMCAwIDEgMCAyNzIuMzc4TTQ3My4xMTgtNzk2Ljc5YTI0MC4xNyAyNDAuMTcgMCAwIDEgMCAxOTUuMzY4IiBzdHlsZT0iZmlsbDojMDAwO2ZpbGwtb3BhY2l0eTowO2ZpbGwtcnVsZTpldmVub2RkO3N0cm9rZTojZmZmO3N0cm9rZS13aWR0aDozNTtzdHJva2UtbGluZWNhcDpyb3VuZDtzdHJva2UtbGluZWpvaW46bWl0ZXI7c3Ryb2tlLW1pdGVybGltaXQ6NDtzdHJva2UtZGFzaGFycmF5Om5vbmU7c3Ryb2tlLW9wYWNpdHk6MSIgdHJhbnNmb3JtPSJyb3RhdGUoMTIwKSIvPjwvZz48L3N2Zz4="


class exampleControl(object):
    """Example remote control"""

    track = 1

    def __init__(self, params):
        self._params = params

        self._metadata = {}
        self._properties = {}
        self._properties["playbackStatus"] = "playing"
        self._properties["loopStatus"] = "none"
        self._properties["shuffle"] = False
        self._properties["volume"] = 100
        self._properties["mute"] = False
        self._properties["rate"] = 1.0
        self._properties["position"] = 0

        self._properties["canGoNext"] = True
        self._properties["canGoPrevious"] = True
        self._properties["canPlay"] = True
        self._properties["canPause"] = True
        self._properties["canSeek"] = False
        self._properties["canControl"] = True
        self.updateProperties

    def updateProperties(self):
        metadata = {}
        metadata["title"] = f"track {self.track}"
        metadata["artist"] = ["artist"]
        metadata["artData"] = {}
        logo = logo_snapcast
        if self.track % 2 == 0:
            logo = logo_spotify
        metadata["artData"]["data"] = logo
        metadata["artData"]["extension"] = "svg"
        self._metadata = metadata
        self._properties["metadata"] = self._metadata
        send(
            {
                "jsonrpc": "2.0",
                "method": "Plugin.Stream.Player.Properties",
                "params": self._properties,
            }
        )

    def control(self, cmd):
        logger.debug(f"Received command: {cmd}")
        request = json.loads(cmd)
        id = request["id"]
        command = request["params"]["command"]
        if command == "next":
            self.track += 1
        elif command == "previous":
            self.track -= 1
        self.updateProperties()
        send({"jsonrpc": "2.0", "result": "ok", "id": id})


def usage(params):
    print(
        """\
Usage: %(progname)s [OPTION]...

     --snapcast-host=ADDR      Set the snapcast server address
     --snapcast-port=PORT      Set the snapcast server port
     --stream=ID               Set the stream id

     -h, --help             Show this help message
     -d, --debug            Run in debug mode
     -v, --version          meta_example version

Report bugs to https://github.com/badaix/snapcast/issues"""
        % params
    )


if __name__ == "__main__":

    log_format_stderr = "%(asctime)s %(module)s %(levelname)s: %(message)s"

    log_level = logging.INFO

    # Parse command line
    try:
        (opts, args) = getopt.getopt(
            sys.argv[1:],
            "hdv",
            ["help", "snapcast-host=", "snapcast-port=",
                "stream=", "debug", "version"],
        )

    except getopt.GetoptError as ex:
        (msg, opt) = ex.args
        print("%s: %s" % (sys.argv[0], msg), file=sys.stderr)
        print(file=sys.stderr)
        usage(params)
        sys.exit(2)

    for opt, arg in opts:
        if opt in ["-h", "--help"]:
            usage(params)
            sys.exit()
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
            print("meta_example version %s" % v)
            sys.exit()

    if len(args) > 2:
        usage(params)
        sys.exit()

    logger = logging.getLogger("meta_example")
    logger.propagate = False
    logger.setLevel(log_level)

    # Log to stderr
    log_handler = logging.StreamHandler()
    log_handler.setFormatter(logging.Formatter(log_format_stderr))

    logger.addHandler(log_handler)

    for p in ["snapcast-host", "snapcast-port", "stream"]:
        if not params[p]:
            params[p] = defaults[p]

    logger.debug(f"Parameters: {params}")

    example_ctrl = exampleControl(params)
    example_ctrl.updateProperties()
    for line in sys.stdin:
        example_ctrl.control(line)
