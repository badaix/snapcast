# Snapcast changelog

## Version 0.19.0

### Features

- Option to not kill all running librespot instances (PR #539)
- Add Airplay cover art to metadata (PR #543)
- Anroid: add support for Oboe audio backend
- Server: configurable PID file (Issue #554)
- Server: configurable persistant storage directory (Issue #554)
- Server: config file options can be overwritten on command line
- Client: PCM stream can be resampled "--sampleformat" option

### Bugfixes

- Fix Airplay metadata, port selection and device names (PR #537)
- Fix cmake build when libatomic is needed (PR #540)
- Control: fix random crash (PR #543)

### General

- ALSA: improved latency estimation for better sync
- Improved audio synchronization
- Faster initial sync after client start and reconnect
- Less playback tempo adaptions and jitter (Issue #525)
- Playback is robust against system time changes (Issue #522)
- Less "resyncs" in stream reader that were causing audio dropouts
- Control: quicker response to group volume changes
- Server uses less memory when sending PCM data to a stalled connection

_Johannes Pohl <snapcast@badaix.de>  Tue, 03 Mar 2020 00:13:37 +0200_

## Version 0.18.1

### Bugfixes

- Fix random server crash or deadlock during stream client disconnect
- Fix random server crash or deadlock during control client disconnect
- Fix airplay stream buffer allocation (PR #536)

_Johannes Pohl <snapcast@badaix.de>  Tue, 28 Jan 2020 00:13:37 +0200_

## Version 0.18.0

### Features

- Add TCP stream reader
- Configurable number of server worker threads

### Bugfixes

- Client: fix hostname reporting on Android
- Fix some small memory leaks
- Fix Librespot stream causing zombie processes (Issue #530)
- Process stream watchdog is configurable (Issue #517)
- Fix Makefile for macOS (Issues #510, #514)

### General

- Refactored stream readers
- Server can run on a single thread

_Johannes Pohl <snapcast@badaix.de>  Wed, 22 Jan 2020 00:13:37 +0200_

## Version 0.17.1

### Bugfixes

- Fix compile error if u_char is not defined (Issue #506)
- Fix error "exception unknown codec ogg" (Issue #504)
- Fix random crash during client disconnect

_Johannes Pohl <snapcast@badaix.de>  Sat, 23 Nov 2019 00:13:37 +0200_

## Version 0.17.0

### Features

- Support for Opus low-latency codec (PR #4)

### Bugfixes

- CMake: fix check for libatomic (Issue #490, PR #494)
- WebUI: interface.html uses the server's IP for the websocket connection
- Fix warnings (Issue #484)
- Fix lock order inversions and data races identified by thread sanitizer
- Makefiles: fix install targets (PR #493)
- Makefiles: LDFLAGS are added from environment (PR #492)
- CMake: required Boost version is raised to 1.70 (Issue #488)
- Fix crash in websocket server

### General

- Stream server uses less threads (one in total, instead of 1+2n)
- Changing group volume is much more responsive for larger groups
- Unknown snapserver.conf options are logged as warning (Issue #487)
- debian scripts: change usernames back to snapclient and snapserver

_Johannes Pohl <snapcast@badaix.de>  Wed, 20 Nov 2019 00:13:37 +0200_

## Version 0.16.0

### Features

- Control server: websocket support for JSON RPC
- Control server: simple webserver integrated to host web UIs
- Control server: bind-to addresses are configurable
- Control server: streams can be added and Removed (PR #443)
- Control server: group names can be changed (PR #467)
- Librespot: add onevent parameter (PR #465)

### Bugfixes

- Fix build on macOS (Issue #474)

### General

- Control server uses less threads (one in total, instead of 1+2n)
- Snapserver: Configuration is moved into a file
- Removed submodules "popl" and "aixlog" to ease build
- Snapcast depends on boost::asio and boost::beast (header only)
- Tidy up code base
- Makefile doesn't statically link libgcc and libstdc++
- debian scripts: change usernames to _snapclient and _snapserver

_Johannes Pohl <snapcast@badaix.de>  Sat, 13 Oct 2019 00:13:37 +0200_

## Version 0.15.0

### Bugfixes

- Snapserver: fix occasional deadlock
- Snapclient: make systemd dependeny to avahi-daemon optional
- cmake: fix build on FreeBSD

### General

- additional linker flags "ADD_LDFLAGS" can be passed to makefile
- update man pages, fix lintian warnings
- Support Android NDK r17

_Johannes Pohl <snapcast@badaix.de>  Sat, 07 Jul 2018 00:13:37 +0200_

## Version 0.14.0

### Features

- Snapserver supports IPv4v6 dual stack and IPv4 + IPv6

### Bugfixes

- cmake: fix check for big endian (Issue #367)
- cmake: fix linking against libatomic (PR #368)
- Snapclient compiles with Android NDK API level 16 (Issue #365)
- Fix compilation errors on FreeBSD (Issue #374, PR #375)

### General

- cmake: make dependency on avahi optional (PR #378)
- cmake: Options to build snapserver and snapclient
- cmake: works on FreeBSD
- Update external libs (JSON for modern C++, ASIO, AixLog)
- Restructured source tree
- Moved Android app into seperate project [snapdroid](https://github.com/badaix/snapdroid)

_Johannes Pohl <snapcast@badaix.de>  Fri, 27 Apr 2018 00:13:37 +0200_

## Version 0.13.0

### Features

- Support "volume" parameter for Librespot (PR #273)
- Add support for metatags (PR #319)

### Bugfixes

- Fix overflow in timesync when the system time is many years off
- Zeroconf works with IPv6
- Snapclient unit file depends on avahi-daemon.service (PR #348)

### General

- Drop dependency to external "jsonrpc++"
- Move OpenWrt and Buildroot support into separate project "[SnapOS](https://github.com/badaix/snapos)"
- Add CMake support (not fully replacing Make yet) (PR #212)
- Remove MIPS support for Android (deprecated by Google)
- Use MAC address as default client id (override with "--hostID")

_Johannes Pohl <snapcast@badaix.de>  Tue, 04 Mar 2018 00:13:37 +0200_

## Version 0.12.0

### Features

- Support for IPv6 (PR #273, #279)
- Spotify plugin: onstart and onstop parameter (PR #225)
- Snapclient: configurable client id (Issue #249)
- Android: Snapclient support for ARM, MIPS and X86
- Snapserver: Play some silence before going idle (PR #284)
 
### Bugfixes

- Fix linker error (Issue #255, #274)
- Snapclient: more reliable unique client id (Issue #249, #246)
- Snapserver: fix config file permissions (Issue #251)
- Snapserver: fix crash on `bye` from control client (Issue #238)
- Snapserver: fix crash on port scan (Issue #267)
- Snapserver: fix crash when a malformed message is received (Issue #264)
  
### General

- Improved logging: Use `--debug` for debug logging
- Log to file: Use `--debug=<filename>`
- Improved exception handling and error logging (Issue #276)
- Android: update to NDK r16 and clang++ 
- hide spotify credentials in json control message (Issue #282)

_Johannes Pohl <snapcast@badaix.de>  Tue, 17 Oct 2017 00:13:37 +0200_

## Version 0.11.1

### Bugfixes

- Snapserver produces high CPU load on some systems (Issue #174)
- Snapserver compile error on FreeBSD

### General

- Updated Markdown files (PR #216, #218)

_Johannes Pohl <snapcast@badaix.de>  Tue, 21 Mar 2017 00:13:37 +0200_

## Version 0.11.0

### Features

- Don't send audio to muted clients (Issue #109, #150)
- Snapclient multi-instance support (Issue #73)
- daemon can run as different user (default: snapclient, Issue #89)
- Spotify plugin does not require username and password (PR #181)
- Spotify plugin is compatible to librespot's pipe backend (PR #158)
- Clients are organized in Groups
- JSON RPC API: Support batch requests

### Bugfixes

- Compilation error on recent GCC versions (Issues #146, #170)
- Crash when frequently connecting to the control port (Issue #200)
- Snapcast App crashes on Android 4.x (Issue #207)
- Resync issue on macOS (Issue #156)
- Id in JSON RPC API can be String, Number or NULL (Issue #161)
- Use "which" instead of "whereis" to find binaries (PR #196, Issue #185)
- Compiles on lede (Issue #203)

### General

- JSON RPC API is versionized (current version is 2.0.0)
- Restructured Android App to support Groups

_Johannes Pohl <snapcast@badaix.de>  Thu, 16 Mar 2017 00:13:37 +0200_

## Version 0.11.0-beta-1

### Features

- Snapclient multi-instance support (Issue #73)
- daemon can run as different user (default: snapclient, Issue #89)
- Spotify plugin does not require username and password (PR #181)
- Spotify plugin is compatible to librespot's pipe backend (PR #158)
- Clients are organized in Groups
- JSON RPC API: Support batch requests

### Bugfixes

- Resync issue on macOS (Issue #156)
- Id in JSON RPC API can be String, Number or NULL (Issue #161)
- Use "which" instead of "whereis" to find binaries (PR #196, Issue #185)
- Compiles on lede (Issue #203)

### General

- JSON RPC API is versionized (current version is 2.0.0)
- Restructured Android App to support Groups

_Johannes Pohl <snapcast@badaix.de>  Sat, 04 Mar 2017 00:13:37 +0200_

## Version 0.10.0

### Features

- Added support [process](https://github.com/badaix/snapcast/blob/master/doc/player_setup.md#process) streams:  
  Snapserver starts a process and reads PCM data from stdout
- Specialized versions for [Spotify](https://github.com/badaix/snapcast/blob/master/doc/player_setup.md#spotify) and [AirPlay](https://github.com/badaix/snapcast/blob/master/doc/player_setup.md#airplay)

### Bugfixes

- Fixed crash during server shutdown
- Fixed Makefile for FreeBSD
- Fixed building of dpk (unsigned .changes file)

### General

- Speed up client and server shutdown

_Johannes Pohl <snapcast@badaix.de>  Wed, 16 Nov 2016 00:13:37 +0200_

## Version 0.9.0

### Features

- Added (experimental) support for macOS (make TARGET=MACOS)

### Bugfixes

- Android client: Fixed crash on Nougat (Issue #97)
- Fixed FreeBSD compile error for Snapserver (Issue #107)
- Snapserver randomly dropped JSON control messages
- Long command line options (like `--sampleformat`) didn't work on some systems (Issue #103) 

### General

- Updated Android NDK to revision 13

_Johannes Pohl <snapcast@badaix.de>  Sat, 22 Oct 2016 00:13:37 +0200_

## Version 0.8.0

### Features

- Added support for FreeBSD (Issue #67)
- Android client: Added Japanese and German translation
- Android client: Added support for ogg (Issue #83)

### Bugfixes

- OpenWRT: makefile automatically sets correct endian (Issue #91)

_Johannes Pohl <snapcast@badaix.de>  Sun, 24 Jul 2016 00:13:37 +0200_

## Version 0.7.0

### Features

- Support for HiRes audio (e.g. 96000:24:2) (Issue #13) 
  - Bitdepth must be one of 8, 16, 24 (=24 bit padded to 32) or 32 
- Auto start option for Android (Issue #49) 
- Creation mode for the fifo can be configured (Issue #52) 
  - "-s pipe:///tmp/snapfifo?mode=[read|create]" 

### Bugfixes

- Server was sometimes crashing during shutdown
- Exceptions were not properly logged (e.g. unsupported sample rates)
- Fixed default sound card detection on OpenWrt

_Johannes Pohl <snapcast@badaix.de>  Sat, 07 May 2016 00:13:37 +0200_

## Version 0.6.0

### Features

- Port to OpenWrt (Issue #18)

### Bugfixes

- Android client: fixed crash if more than two streams are active (Issue #47)

### General

- Support Tremor, an integer only Ogg-Vorbis implementation
- Endian-independent code (Issue #18)
- Cleaned up build process

_Johannes Pohl <snapcast@badaix.de>  Sun, 10 Apr 2016 00:02:00 +0200_

## Version 0.5.0

### Features

- Android client: Fast switching of clients between streams

### Bugfixes

- Server: Fixed reading of server.json config file

### General

- Source code cleanups

_Johannes Pohl <snapcast@badaix.de>  Fri, 25 Mar 2016 00:02:00 +0200_

## Version 0.4.1

### General

- Debian packages (.deb) are linked statically against libgcc and libstdc++ to improve compatibility

_Johannes Pohl <snapcast@badaix.de>  Sat, 12 Mar 2016 12:00:00 +0200_

## Version 0.5.0-beta-2

### Features

- Remote control API (JSON)
  - Added version information
  - Stream playing state (unknown, idle, playing, inactive) (Issue #34)
- Android client: manually configure snapserver host name
- Android client compatibility improved: armeabi and armeabi-v7 binaries
- Android client: configurable latency
- Improved compatibility to Mopidy (GStreamer) (Issue #23)

### Bugfixes

- Android client: fixed "hide offline" on start
- Store config in /var/lib/snapcast/ when running as daemon (Issue #33)

### General

- README.md: Added resampling command to the Mopidy section (Issue #32)

_Johannes Pohl <snapcast@badaix.de>  Wed, 09 Mar 2016 00:02:00 +0200_

## Version 0.5.0-beta-1

### Features

- Remote control API (JSON)
  - Get server status, get streams, get active clients, ...
  - Assign volume, assign stream, rename client, ...
- Android port of the Snapclient with a remote control app (requires API level 16, Android 4.1)
- Multiple streams ("zones") can be configured using `-s, --stream`  
  The stream is configured by an URI: path, name, codec, sample format, ...  
  E.g. `pipe:///tmp/snapfifo?name=Radio&sampleformat=48000:16:2&codec=flac`
  or `file:///home/user/some_pcm_file.wav?name=file`
- Added .default file for the service in `/etc/default/snapserver` and `/etc/default/snapclient`.  
  Default program options should be configured here (e.g. streams)

### Bugfixes

- Pipe reader recovers if the pipe has been reopened

### General

- SnapCast is renamed to Snapcast
  - SnapClient => Snapclient
  - SnapServer => Snapserver
- Changed default sample rate to 48kHz. The sample rate can be configured per stream `-s "pipe:///tmp/snapfifo?name=default&sampleformat=44100:16:2`. The default can be changed with `--sampleformat 44100:16:2`
- Snapcast protocol:  
  Less messaging: SampleFormat, Command, Ack, String, not yet final
- Removed dependency to boost

_Johannes Pohl <snapcast@badaix.de>  Tue, 09 Feb 2016 13:25:00 +0200_

## Version 0.4.0

### Features

- Debian packages (.deb) for amd64 and armhf
- Added man pages

### Bugfixes

- Snapserver and Snapclient are started as daemon on systemd systems (e.g. ARCH, Debian Jessie)

### General

- Snapserver is started with normal process priority (changed nice from -3 to 0)

_Johannes Pohl <snapcast@badaix.de>  Mon, 28 Dec 2015 12:00:00 +0200_

## Version 0.3.4

### Bugfixes

- Fix synchronization bug in FLAC decoder that could cause audible dropouts

_Johannes Pohl <snapcast@badaix.de>  Wed, 23 Dec 2015 12:00:00 +0200_

## Version 0.3.3

### Bugfixes

- Fix Segfault when ALSA device has no description

_Johannes Pohl <snapcast@badaix.de>  Sun, 15 Nov 2015 12:00:00 +0200_

## Version 0.3.2

### General

- Makefile uses CXX instead of CC to invoke the c++ compiler

### Bugfixes

- Time calculation for PCM chunk play-out was wrong on some gcc versions

_Johannes Pohl <snapcast@badaix.de>  Wed, 30 Sep 2015 12:00:00 +0200_

## Version 0.3.1

### General

- Improved stability over WiFi by avoiding simultaneous reads/writes on the socket connection

### Bugfixes

- Fixed a bug in avahi browser

_Johannes Pohl <snapcast@badaix.de>  Wed, 26 Aug 2015 12:00:00 +0200_

## Version 0.3.0

### Features

- Configurable codec options. Run `snapserver -c [flac|ogg|pcm]:?` to get supported options for the codec
- Configurable buffer size for the pipe reader (default 20ms, was 50ms before)
- Process priority can be changed as argument to the daemon option `-d<prio>`. Default priority is -3

### Bugfixes

- Fixed deadlock in logger
- Fixed occasional timeouts for client to server requests (e.g. time sync commands)
- Client didn't connect to a local server if the loopback device is the only device with an address

### General

- Code clean up
- Refactored encoding for lower latency

_Johannes Pohl <snapcast@badaix.de>  Sun, 16 Aug 2015 19:25:51 +0100

## Version 0.2.1

### Features

- Arch Linux compatibility

_Johannes Pohl <snapcast@badaix.de>  Fri, 24 Jul 2015 15:47:00 +0100
