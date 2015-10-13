#!/bin/bash
#Example script that watches the MPD volume and sets the snapclient volume
#accordingly via the remote control protocol. Set up MPD in the following way:
#
## Snapcast gets the audio stream unmodified
# audio_output {
#     type            "fifo"
#     name            "snapcast"
#     path            "/tmp/snapfifo"
#     format          "44100:16:2"
#     mixer_type      "null"
# }
## This dummy output allows to set the MPD volume without actually playing
# audio_output {
#     type            "pipe"
#     name            "volume dummy"
#     command         "cat >/dev/null"
#     format          "44100:16:2"
#     mixer_type      "software"
# }
#
# Let this script run, and voila - you can set the volume of your MPD-local
# snapcast client with any MPD client!

mpd_host=localhost
mpd_port=6600
snapclient_host=localhost
snapclient_port=3333

oldvol=-1
while true; do
  vol=$(mpc -h $mpd_host -p $mpd_port volume | sed 's/[^0-9]*\([0-9]*\).*/\1/')
  if [ "$oldvol" != "$vol" ]; then
    oldvol=$vol
    exec 3<>/dev/tcp/$snapclient_host/$snapclient_port
    echo "volume set $vol" >&3
    exec 3>&-
  fi
  sleep 0.5
done
