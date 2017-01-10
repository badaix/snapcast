Snapcast
========

![Snapcast](https://raw.githubusercontent.com/badaix/snapcast/master/doc/Snapcast_800.png)

**S**y**n**chronous **a**udio **p**layer

Snapcast is a multi-room client-server audio player, where all clients are time synchronized with the server to play perfectly synced audio. It's not a standalone player, but an extension that turns your existing audio player into a Sonos-like multi-room solution.
The server's audio input is a named pipe `/tmp/snapfifo`. All data that is fed into this file will be send to the connected clients. One of the most generic ways to use Snapcast is in conjunction with the music player daemon ([MPD](http://www.musicpd.org/)) or [Mopidy](https://www.mopidy.com/), which can be configured to use a named pipe as audio output.

How does is work
----------------
The Snapserver reads PCM chunks from the pipe `/tmp/snapfifo`. The chunk is encoded and tagged with the local time. Supported codecs are:
* **PCM** lossless uncompressed
* **FLAC** lossless compressed [default]
* **Vorbis** lossy compression

The encoded chunk is sent via a TCP connection to the Snapclients.
Each client does continuos time synchronization with the server, so that the client is always aware of the local server time.
Every received chunk is first decoded and added to the client's chunk-buffer. Knowing the server's time, the chunk is played out using ALSA at the appropriate time. Time deviations are corrected by
* skipping parts or whole chunks
* playing silence
* playing faster/slower

Typically the deviation is smaller than 1ms.

Installation
------------
You can either build and install snapcast from source, or on debian systems install a prebuild .deb package

###Installation from source
Please follow this [guide](doc/build.md) to build Snapcast for
* [Linux](doc/build.md#linux-native)
* [FreeBSD](doc/build.md#freebsd-native)
* [macOS](doc/build.md#macos-native)
* [Android](doc/build.md#android-cross-compile)
* [OpenWrt](doc/build.md#openwrt-cross-compile)

###Install debian packages
Download the debian package for your CPU architecture from the [latest release page](https://github.com/badaix/snapcast/releases/latest), e.g. for Raspberry pi `snapclient_0.x.x_armhf.deb`
Install the package:

    $ sudo dpkg -i snapclient_0.x.x_armhf.deb

Install missing dependencies:

    $ sudo apt-get -f install

On OpenWrt do:

    $ opkg install snapclient_0.x.x_ar71xx.ipk

Configuration
-------------
After installation, Snapserver and Snapclient are started with the command line arguments that are configured in `/etc/default/snapserver` and `/etc/default/snapclient`.
Allowed options are listed in the man pages (`man snapserver`, `man snapclient`) or by invoking the snapserver or snapclient with the `-h` option.

Different streams can by configured with a list of `-s` options, e.g.:

    SNAPSERVER_OPTS="-d -s pipe:///tmp/snapfifo?name=Radio&sampleformat=48000:16:2&codec=flac -s file:///home/user/Musik/Some%20wave%20file.wav?name=File"

The pipe stream (`-s pipe`) will per default create the pipe. Sometimes your audio source might insist in creating the pipe itself. So the pipe creation mode can by changed to "not create, but only read mode", using the `mode` option set to `create` or `read`:
    
    SNAPSERVER_OPTS="-d -s pipe:///tmp/snapfifo?name=Radio&mode=read"
    
Test
----
You can test your installation by copying random data into the server's fifo file

    $ sudo cat /dev/urandom > /tmp/snapfifo

All connected clients should play random noise now. You might raise the client's volume with "alsamixer".
It's also possible to let the server play a wave file. Simply configure a `file` stream in `/etc/default/snapserver`, and restart the server:

    SNAPSERVER_OPTS="-d -s file:///home/user/Musik/Some%20wave%20file.wav?name=test"

When you are using a Raspberry pi, you might have to change your audio output to the 3.5mm jack:

    #The last number is the audio output with 1 being the 3.5 jack, 2 being HDMI and 0 being auto.
    $ amixer cset numid=3 1

To setup WiFi on a raspberry pi, you can follow this guide:
https://www.raspberrypi.org/documentation/configuration/wireless/wireless-cli.md

Control
-------
Snapcast can be controlled using JSON-RPC:
* Set client's volume
* Mute clients
* Rename clients
* Assign a client to a stream
* ...

There is an Android client available in [Releases](https://github.com/badaix/snapcast/releases/latest) 

![Snapcast for Android](https://raw.githubusercontent.com/badaix/snapcast/master/doc/snapcast_android_scaled.png)

There is also an unofficial WebApp from @atoomic [atoomic/snapcast-volume-ui](https://github.com/atoomic/snapcast-volume-ui).
This app list all clients connected to a server and allow to control individualy the volume of each client.
Once installed, you can use any mobile device, laptop, desktop, or browser.

Setup of audio players/server
-----------------------------
Snapcast can be used with a number of different audio players and servers, and so it can be integrated into your favorite audio-player solution and make it synced-multiroom capable.
The only requirement is that the player's audio can be redirected into the Snapserver's fifo `/tmp/snapfifo`. In the following configuration hints for [MPD](http://www.musicpd.org/) and [Mopidy](https://www.mopidy.com/) are given, which are base of other audio player solutions, like [Volumio](https://volumio.org/) or [RuneAudio](http://www.runeaudio.com/) (both MPD) or [Pi MusicBox](http://www.pimusicbox.com/) (Mopidy).

The goal is to build the following chain:

    audio player software -> snapfifo -> snapserver -> network -> snapclient -> alsa

This [guide](doc/player_setup.md) shows how to configure different players/audio sources to redirect their audio signal into the Snapserver's fifo:
* [MPD](doc/player_setup.md#mpd)
* [Mopidy](doc/player_setup.md#mopidy)
* [FFmpeg](doc/player_setup.md#ffmpeg)
* [mpv](doc/player_setup.md#mpv)
* [MPlayer](doc/player_setup.md#mplayer)
* [Alsa](doc/player_setup.md#alsa)
* [PulseAudio](doc/player_setup.md#pulseaudio)
* [AirPlay](doc/player_setup.md#airplay)
* [Spotify](doc/player_setup.md#spotify)
* [Process](doc/player_setup.md#process)

Roadmap
-------
Unordered list of features that should make it into the v1.0
- [X] **Remote control** JSON-RPC API to change client latency, volume, zone, ...
- [X] **Android client** JSON-RPC client and Snapclient
- [X] **Streams** Support multiple streams
- [X] **Debian packages** prebuild deb packages
- [X] **Endian** independent code
- [X] **OpenWrt** port Snapclient to OpenWrt
- [X] **Hi-Res audio** support (like 192kHz 24bit)
- [ ] **Groups** support multiple Groups of clients ("Zones")
- [ ] **JSON-RPC** Possibility to add, remove, rename streams
- [ ] **Protocol specification** Snapcast binary streaming protocol, JSON-RPC protocol
- [ ] **Ports** Snapclient for Windows, Mac OS X, ...
