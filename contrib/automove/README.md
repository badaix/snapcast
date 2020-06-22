# Play new stream on all groups

## The Problem:
When I start spotify I want it to be playing via snapcast.
But snapcast currently is playing the stream from MPD.
So this scripts listens on stream events and tells all
Snapcast groups to play the new stream when a new stream
starts playing

## Install
`python3 -m pip install snapcast`

copy `snapswitch.service` to `/etc/systemd/system`
copy `snapswitch.py` to `/usr/local/bin`
run `systemctl daemon-reload`
run `systemctl start snapswitch`

When you start playing, all groups should switch to that stream
