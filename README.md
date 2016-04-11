Snapcast
========

[![Join the chat at https://gitter.im/badaix/snapcast](https://badges.gitter.im/badaix/snapcast.svg)](https://gitter.im/badaix/snapcast?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

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
* [Android](doc/build.md#android-cross-compile)
* [OpenWrt](doc/build.md#openwrt-cross-compile)

###Install debian packages
Download the debian package for your CPU architecture from the [latest release page](https://github.com/badaix/snapcast/releases/latest), e.g. for Raspberry pi `snapclient_0.x.x_armhf.deb`
Install the package:

    $ sudo dpkg -i snapclient_0.x.x_armhf.deb

Install missing dependencies:

    $ sudo apt-get -f install

Configuration
-------------
After installation, Snapserver and Snapclient are started with the command line arguments that are configured in `/etc/default/snapserver` and `/etc/default/snapclient`.
Allowed options are listed in the man pages (`man snapserver`, `man snapclient`) or by invoking the snapserver or snapclient with the `-h` option.

Different streams can by configured with a list of `-s` options, e.g.:

    SNAPSERVER_OPTS="-d -s pipe:///tmp/snapfifo?name=Radio&sampleformat=48000:16:2&codec=flac -s file:///home/user/Musik/Some%20wave%20file.wav?name=File"

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


Setup of audio players/server
-----------------------------
Snapcast can be used with a number of different audio players and servers, and so it can be integrated into your favorite audio-player solution and make it synced-multiroom capable.
The only requirement is that the player's audio can be redirected into the Snapserver's fifo `/tmp/snapfifo`. In the following configuration hints for [MPD](http://www.musicpd.org/) and [Mopidy](https://www.mopidy.com/) are given, which are base of other audio player solutions, like [Volumio](https://volumio.org/) or [RuneAudio](http://www.runeaudio.com/) (both MPD) or [Pi MusicBox](http://www.pimusicbox.com/) (Mopidy).

The goal is to build the following chain:

    audio player software -> snapfifo -> snapserver -> network -> snapclient -> alsa

###MPD setup
To connect [MPD](http://www.musicpd.org/) to the Snapserver, edit `/etc/mpd.conf`, so that mpd will feed the audio into the snapserver's named pipe

Disable alsa audio output by commenting out this section:

    #audio_output {
    #       type            "alsa"
    #       name            "My ALSA Device"
    #       device          "hw:0,0"        # optional
    #       format          "48000:16:2"    # optional
    #       mixer_device    "default"       # optional
    #       mixer_control   "PCM"           # optional
    #       mixer_index     "0"             # optional
    #}

Add a new audio output of the type "fifo", which will let mpd play audio into the named pipe `/tmp/snapfifo`.
Make sure that the "format" setting is the same as the format setting of the Snapserver (default is "48000:16:2", which should make resampling unnecessary in most cases)

    audio_output {
        type            "fifo"
        name            "my pipe"
        path            "/tmp/snapfifo"
        format          "48000:16:2"
        mixer_type      "software"
    }

To test your mpd installation, you can add a radio station by

    $ sudo su
    $ echo "http://1live.akacast.akamaistream.net/7/706/119434/v1/gnl.akacast.akamaistream.net/1live" > /var/lib/mpd/playlists/einslive.m3u

###Mopidy setup
[Mopidy](https://www.mopidy.com/) can stream the audio output into the Snapserver's fifo with a `filesink` as audio output in `mopidy.conf`:

    [audio]
    #output = autoaudiosink
    output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! wavenc ! filesink location=/tmp/snapfifo

###Alsa setup
If the player cannot be configured to route the audio stream into the snapfifo, Alsa or PulseAudio can be redirected, resulting into this chain:

    audio player software -> Alsa -> Alsa file plugin -> snapfifo -> snapserver -> network -> snapclient -> Alsa

Edit or create your Alsa config `/etc/asound.conf` like this:

```
pcm.!default {
	type plug
	slave.pcm rate48000Hz
}

pcm.rate48000Hz {
	type rate
	slave {
		pcm writeFile # Direct to the plugin which will write to a file
		format S16_LE
		rate 48000
	}
}

pcm.writeFile {
	type file
	slave.pcm null
	file "/tmp/snapfifo"
	format "raw"
}
```

###PulseAudio setup
Redirect the PulseAudio stream into the snapfifo:

    audio player software -> PulseAudio -> PulsaAudio pipe sink -> snapfifo -> snapserver -> network -> snapclient -> Alsa

Load the module `pipe-sink` like this:

    pacmd load-module module-pipe-sink file=/tmp/snapfifo

It might me neccessary to set the pulse audio latency environment variable to 60 msec: `PULSE_LATENCY_MSEC=60`

Roadmap
-------
Unordered list of features that should make it into the v1.0
- [X] **Remote control** JSON-RPC API to change client latency, volume, zone, ...
- [X] **Android client** JSON-RPC client and Snapclient
- [X] **Zones** Support multiple streams
- [X] **Debian packages** prebuild deb packages
- [X] **Endian** independent code
- [X] **OpenWrt** port Snapclient to OpenWrt
- [ ] **Hi-Res audio** support (like 192kHz 24bit)
- [ ] **Protocol specification** Snapcast binary streaming protocol, JSON-RPC protocol
- [ ] **CMake** or another build system
- [ ] **Ports** Snapclient for Windows, ...
