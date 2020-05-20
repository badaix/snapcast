TODO
====

General
-------

- Update Readme.md
- JsonRPC documentation
- Server ping client?
- UDP based audio streaming

Server
------

- [fd stream](https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gstreamer-plugins/html/gstreamer-plugins-fdsink.html)
- gstreamer snapcast sink plugin?
- "sync" option for streams (realtime reading vs read as much as available)
- #402: Add meta stream that reads the input of other streams (in some fallback hierarchy manner: when the first one plays, use this signal, else if the second is playing, take this, ...).

Client
------

- Add option to not play in sync, i.e. not change the tempo

Issues
------

- Android clean data structures after changing the Server
