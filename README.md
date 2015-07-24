SnapCast
========

**S**y**n**chronous **a**udio **p**layer

SnapCast is a multi-room client-server audio player, where all clients are time synchronized with the server to play perfectly synced audio. It's not a standalone player, but an extension that turns your existing audio player into a Sonos-like multi-room solution.  
The server's audio input is a named pipe `/tmp/snapfifo`. All data that is fed into this file, will be send to the connected clients. One of the most generic ways to use SnapCast is in conjunction with the music player daemon ([MPD](http://www.musicpd.org/)) or [Mopidy](https://www.mopidy.com/), which can be configured to use a named pipe as audio output.

How does is work
----------------
The SnapServer reads PCM chunks of 50ms duration from the pipe `/tmp/snapfifo`. The chunk is encoded and tagged with the local time
* PCM: lossless uncompressed
* FLAC: lossless compressed [default]
* Vorbis: lossy compression

The encoded chunk is sent via a TCP connection to the SnapClients.
Each client does continuos time synchronization with the server, so that the client is always aware of the local server time.  
Every received chunk is first decoded and added to the client's chunk-buffer. Knowing the server's time, the chunk is played out using alsa at the appropriate time. Time deviations are corrected by 
* skipping parts or whole chunks
* playing silence
* playing faster/slower

Typically the deviation is < 1ms.

Installation
------------
First install all packages needed to compile SnapCast

For Debian derivates (e.g. Raspbian, Debian, Ubuntu, Mint):

    $ sudo apt-get install git build-essential
    $ sudo apt-get install libboost-dev libboost-system-dev libboost-program-options-dev libasound2-dev libvorbis-dev libflac-dev alsa-utils libavahi-client-dev avahi-daemon
    
For Arch derivates:

    $ pacman -S git base-devel
    $ pacman -S boost boost-libs alsa-lib avahi libvorbis flac alsa-utils
    
Build SnapCast by cd'ing into the SnapCast src-root directory

    $ cd <MY_SNAPCAST_ROOT>
    $ make
    
Install SnapClient and/or SnapServer:

    $ sudo make installserver
    $ sudo make installclient

This will copy the client and/or server binary to `/usr/sbin` and update init.d/systemd to start the client/server as a daemon.


Test
----
You can test your installation by copying random data into the server's fifo file

    $ sudo cat /dev/urandom > /tmp/snapfifo

All connected clients should play random noise now. You might raise the client's volume with "alsamixer".  
When you are using a raspberry pi, you might have to change your audio output to the 3.5mm jack:

    #The last number is the audio output with 1 being the 3.5 jack, 2 being HDMI and 0 being auto.
    $ amixer cset numid=3 1

To setup WiFi on a raspberry pi, you can follow this guide:  
https://www.raspberrypi.org/documentation/configuration/wireless/wireless-cli.md


Setup of audio players/server
-----------------------------
SnapCast can be used with a number of different audio players and servers, and so it can be integrated into your favorite audio-player solution and make it synced-multiroom capable.  
The only requirement is that the player's audio can be redirected into the SnapServer's fifo `/tmp/snapfifo`. In the following configuration hints for [MPD](http://www.musicpd.org/) and [Mopidy](https://www.mopidy.com/) are given, which are base of other audio player solutions, like [Volumio](https://volumio.org/) or [RuneAudio](http://www.runeaudio.com/) (both MPD) or [Pi MusicBox](http://www.pimusicbox.com/) (Mopidy).

###MPD setup
To connect [MPD](http://www.musicpd.org/) to the SnapServer, edit `/etc/mpd.conf`, so that mpd will feed the audio into the snap-server's named pipe

Disable alsa audio output by commenting out this section:

    #audio_output {
    #       type            "alsa"
    #       name            "My ALSA Device"
    #       device          "hw:0,0"        # optional
    #       format          "44100:16:2"    # optional
    #       mixer_device    "default"       # optional
    #       mixer_control   "PCM"           # optional
    #       mixer_index     "0"             # optional
    #}

Add a new audio output of the type "fifo", which will let mpd play audio into the named pipe `/tmp/snapfifo`.
Make sure that the "format" setting is the same as the format setting of the SnapServer (default is "44100:16:2", which should make resampling unnecessary in most cases)

    audio_output {
        type            "fifo"
        name            "my pipe"
        path            "/tmp/snapfifo" 
        format          "44100:16:2"
        mixer_type      "software"
    } 

To test your mpd installation, you can add a radio station by

    $ sudo su
    $ echo "http://1live.akacast.akamaistream.net/7/706/119434/v1/gnl.akacast.akamaistream.net/1live" > /var/lib/mpd/playlists/einslive.m3u

###Mopidy setup
[Mopidy](https://www.mopidy.com/) can stream the audio output into the SnapServer's fifo with a `filesink` as audio output:

    [audio]
    #output = autoaudiosink
    output = audioresample ! audioconvert ! wavenc ! filesink location=/tmp/snapfifo

###PulseAudio setup
On the server you could create a sink to route sound of your applications to the FIFO:
```
pacmd load-module module-pipe-sink file=/tmp/snapfifo
```
    
Roadmap
-------
* Support multiple streams ("Zones")
* Remote control: change client latency, volume, zone
