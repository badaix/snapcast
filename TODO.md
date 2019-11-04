TODO
====

General
-------

- Server ping client?
- LastSeen: relative time [s] or [ms]?
- Android crash: Empty latency => app restart => empty client list
- Android clean data structures after changing the Server
- UDP based audio streaming

Server
------

- Provide io_context to stream-readers
- [fd stream](https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gstreamer-plugins/html/gstreamer-plugins-fdsink.html)
- UDP/TCP stream
- "sync" option for streams (realtime reading vs read as much as available)
- Override conf file settings on command line

Client
------

- revise asio stuff
