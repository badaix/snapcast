# Snapcast changelog

## Version 0.28.0-beta.1

### Features

- Server: Use filename as title for FileStreams

### Bugfixes

- Server: Fix noise caused by reading half samples (Issue #1170)
- Server: Check open syscall error in PipeStream::do_connect (PR #1150)
- Server: Tweak Airplay support (#1102)
- Server: Lower log severity for shairport-sync (Issue #891)
- Server: Emits notifications for streams with codec=null (Issue #1205)
- Improve connect handling in meta_mopidy.py
- Fix cppcheck issues

### General

- CI: Build debian bookworm and bullseye packages for amd64, arm and arm64
- CI: Add cppcheck static analysis
- Update documentation (PR #1130, PR #1067)
- Delete deprecated Makefiles, CMake is the only supported buildsystem
- Snapweb: Update to v0.7.0

_Johannes Pohl <snapcast@badaix.de>  Sun, 24 Mar 2024 00:13:37 +0200_

## Version 0.27.0

### Features

- Server: Configurable default volume for new clients (Issue #910, PR #1024)
- Server: New control script for mopidy: meta_mopidy.py
- Server: New control script for librespot java: meta_librespot-java.py
- Server: Add "mute" stream property

### Bugfixes

- Server: Fix random crash with meta stream (Issue #966)
- Server: Fix compatibility with librespot 0.5-dev (Issue #1063, #1074, #1042)
- Server: Not terminate on malformed json messages (Issue #1049)
- Server: Fix random segfault (Issue #1047)
- Server: Fix growing delay on input stream (Issue #1014)
- Server: Fix segfault with Librespot on Alpine (Issue #1026)
- Server: meta_mpd.py is installed with 755 (Issue #1021, #970)
- Server: Add list of dependecies to meta_mpd.py (Issue #971)
- Server: meta_mpd.py stopped working (Issue #997)
- Client: Not terminate if codec is "null" (Issue #1076)
- Fix test failures (Issue #961)

### General

- Fix compilation on macOS with Xcode 13.4.1 (Issue #1028)
- Fix compilation with boost 1.81.0 (Issue #1082)
- Remove debhelper files from project (moved to SnapOS project)
- SnapOs uses containers to build deb packages (solves Issue #968)
- Snapweb: Update to v0.5.0

_Johannes Pohl <snapcast@badaix.de>  Sun, 05 Feb 2023 00:13:37 +0200_

## Version 0.26.0

### Features

- Client: Disconnect from pulse when no audio is available (Issue #927, PR #931)
- Server: New Metadata API for audio streams (Issue #803, #953)
- Server: New Control API for audio streams (Issue #954)
- Server: Lowered minimum buffer from 400ms to 20ms (Issue #329)

### Bugfixes

- Server: systemd.unit starts server after network-online (Issue #950)

### General

- Update documentation (Issue #804, PR #945, PR #951)
- Snapweb: Update to v0.4.0

_Johannes Pohl <snapcast@badaix.de>  Wed, 22 Dec 2021 00:13:37 +0200_

## Version 0.25.0

### Features

- Client: Editable PulseAudio properties, e.g. media.role=music (Issue #829)
- Server: Configurable amplitute for silence detection in alsa stream (Issue #846)

### Bugfixes

- Client: Fix crash on Windows when system volume changes (PR #838)
- Client: Removed a log message to stdout for file player backend (Issue #681)
- Server: Fix percent-decoding for stream URIs (Issue #850)
- Server: Fix double quotes in Airplay device names (Issue #851)
- Server: Fix crash when feeding a 16 bit signal into 24 bit flac (Issue #866)  

### General

- Server: less verbose librespot logging (Issue #874)
- Snapweb: Update to v0.3.0
- Add unit tests to the project

_Johannes Pohl <snapcast@badaix.de>  Sat, 15 May 2021 00:13:37 +0200_

## Version 0.24.0

### Features

- Client: Configurable server for Pulseaudio backend (Issue #779)

### Bugfixes

- Client: Fix dropouts in alsa player backend (Issue #774)
- Client: Fix alsa player volume resetting to max (Issue #735)  
- Client: Fix noise while muted for Pulseaudio (Issue #785)
- Client: Android support for opus readded (Issue #789)
- Client: Fix host id for MacBook Pro (later 2016) (Issue #807)

### General

- Snapweb: Update to v0.2.0
- Remove submodules with external libs from the git repository
- Write version and revision (git sha) to log
- Add documentation to the default files (Issue #791)

_Johannes Pohl <snapcast@badaix.de>  Sat, 27 Feb 2021 00:13:37 +0200_

## Version 0.23.0

### Features

- Client: Add PulseAudio player backend (Issue #722)
- Client: Configurable buffer time for alsa and pulse players
- Server: If docroot is not configured, a default page is served (Issue #711)
- Server: airplay source supports "password" parameter (Issue #754)

### Bugfixes

- Server: airplay source deletes Shairport's meta pipe on exit (Issue #672)
- Server: alsa source will not send silece when going idle (Issue #729)
- Server: pipe source will not send silence after 3h idle (Issue #741)
- Server: Fix build error on FreeBSD (Issue #752)
- Client: "make install" will set correct user/group for snapclient (Issue #728)
- Client: Fix printing UTF-8 device names on Windows (Issue #732)
- Client: Fix stuttering on alsa player backend (Issue #722, #727)
- Client: Terminate if host is not configured and mDNS is unavailable

### General

- Server: Change librespot parameter "killall" default to false (Issue #746, #724)
- Client: Android uses performance mode "none" to allow effects (Issue #766)
- Snapweb: Update to v0.1.0
- Build: Update CMakeLists.txt to build Snapclient on Android

_Johannes Pohl <snapcast@badaix.de>  Sun, 10 Jan 2021 00:13:37 +0200_

## Version 0.22.0

### Features

- Server: Add Meta stream source (Issue #402, #569, #666)
- Client: Add file audio backend (Issue #681)

### Bugfixes

- Add missing define for alsa stream to makefile (Issue #692)
- Fix playback when plugging the headset on Android (Issue #699)
- Server discards old chunks if not consumed (Issue #708)

### General

- Less verbose logging during pipe reconnects (Issue #696)
- Add null encoder for streams used only as input for meta streams
- Snapweb: Change latency range to [-10s, 10s] (Issue #695)
- Update Snapweb, including PR #11, #12, #13, Issues #16, #17

_Johannes Pohl <snapcast@badaix.de>  Thu, 15 Oct 2020 00:13:37 +0200_

## Version 0.21.0

### Features

- Server: Support for WebSocket streaming clients
- Server: Install Snapweb web client (Issue #579)
- Server: Resample input to 48000:16:2 when using opus codec
- Server: Add Alsa stream source

### Bugfixes

- make install will setup the snapserver home dir (Issue #643)
- Client retries to open a blocked alsa device (Issue #652)

### General

- debian package generation switched from make to CMake buildsystem
- Reintroduce MACOS define, hopefully not breaking anything on macOS
- Snapcast uses GitHub actions for automated CI/CD
- CMake installs man files (Issue #507)
- Update documentation (Issue #615, #617)

_Johannes Pohl <snapcast@badaix.de>  Sun, 13 Sep 2020 00:13:37 +0200_

## Version 0.20.0

### Features

- Client: Windows support (Issue #24)
- Client: add hardware mixer (Issue #318)
- Client: add "script" and "none" mixer (Issue #302)
- Client: add sharingmode for audio device (if supported)
- Logging: configurable sink and filters (Issue #30, #561, #122, #559)
- Librespot: add option "disable-audio-cache=[false|true]"

### Bugfixes

- Fix build failure on FreeBSD (Issue #565)
- Fix calling lsb_release multiple times (Issue #470)
- Client: high CPU load and crash during playback (Issue #609, #628)
- Client: improved handling of USB audio disconnects (Issue #64)
- Client: latency is forgotten (Issue #476, #588, Snapdroid #11)
- Client: fix segfault on mac when playback is paused (Issue #560)
- Client: fix bonjour on mac reports empty IP (Issue #632)
- Client: fix buzzing tone on Android (Issue #23, #24)
- Server: fix crash if client disconnects during connect (Issue #639)
- Server: fix reading metadata from shairport-sync (Issue #624)
- Server: fix crash on FreeBSD if settings.json is empty (Issue #620)
- Server: fix warning about unknown command line options (Issue #635)
- Readme: openWrt documentation (Issue #633)
- Fix setting the daemon's process priority (PR #448)

### General

- Client: use less threads and thus less ressources
- Update links to xiph externals (Issue #637, PR #616)

_Johannes Pohl <snapcast@badaix.de>  Sat, 13 Jun 2020 00:13:37 +0200_

## Version 0.19.0

### Features

- Option to not kill all running librespot instances (PR #539)
- Add Airplay cover art to metadata (PR #543)
- Anroid: add support for Oboe audio backend
- Server: configurable PID file (Issue #554)
- Server: configurable persistant storage directory (Issue #554)
- Server: config file options can be overwritten on command line
- Client: PCM stream can be resampled using the "--sampleformat" option
- Librespot: add option "autoplay=[false|true]"

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

_Johannes Pohl <snapcast@badaix.de>  Sat, 14 Mar 2020 00:13:37 +0200_

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
- debian scripts: change usernames to \_snapclient and \_snapserver

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

_Johannes Pohl <snapcast@badaix.de>  Sun, 16 Aug 2015 19:25:51 +0100_

## Version 0.2.1

### Features

- Arch Linux compatibility

_Johannes Pohl <snapcast@badaix.de>  Fri, 24 Jul 2015 15:47:00 +0100_
