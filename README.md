SnapCast
========

Synchronous audio player

Snapcast is a multi room client-server audio player, where all clients are time synchronized with the server to play perfectly synced audio.
The server's audio input is a named pipe `/tmp/snapfifo`. All data that is fed into this file, will be send to the connected clients. One of the most generic ways to use snapcast is in conjunction with the music player daemon (mpd), which can be configured to use a named pipe as audio output.

How does is work
----------------
The snapserver reads PCM chunks of 50ms duration from the pipe `/tmp/snapfifo`. The chunk is encoded and tagged with the local time
* PCM: lossless uncompressed
* FLAC: lossless compressed [default]
* Vorbis: lossy compression

The encoded chunk is sent via a TCP connection to the snapclients.
Each client does continuos time synchronization with the server, so that the client is always aware of the local server time.  
Every received chunk is first decoded and added to the client's chunk-buffer. Knowing the server's time, the chunk is played out using alsa at the appropriate time. Time deviations are corrected by 
* skipping parts or whole chunks
* playing silence
* playing faster/slower

Typically the deviation is < 1ms.

Installation
------------
These installation instructions are valid for Debian derivates (e.g. Raspbian, Debian, Ubuntu, Mint).
First install all packages needed to compile snapcast:

    $ sudo apt-get install libboost-dev libboost-system-dev libboost-program-options-dev libasound2-dev libvorbis-dev libflac-dev alsa-utils libavahi-client-dev avahi-daemon

Build snapcast by cd'ing into the snapcast src-root directory

    $ cd <MY_SNAPCAST_ROOT>
    $ make all
    
Install snapclient and/or snapserver. The client installation will ask you for the server's hostname or ip address

    $ sudo make installserver
    $ sudo make installclient

This will copy the client and/or server binary to `/usr/sbin` and update init.d to start the client/server as a daemon.


Test
----
You can test your installation by copying random data into the server's fifo file

    $ sudo cat /dev/urandom > /tmp/snapfifo

All connected clients should play random noise now. You might raise the client's volume with "alsamixer".


To setup WiFi on a raspberry pi, you can follow this guide:
http://www.maketecheasier.com/setup-wifi-on-raspberry-pi/


MPD setup
---------
To connect MPD to the snapserver, edit `/etc/mpd.conf`, so that mpd will feed the audio into the snap-server's named pipe

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
Make sure that the "format" setting is the same as the format setting of the snapserver (default is "44100:16:2", which should make resampling unnecessary in most cases)

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

PulseAudio setup
----------------
On the server you could create a sink to route sound of your applications to the FIFO:
```
pacmd load-module module-pipe-sink file=/tmp/snapfifo
```
    
Roadmap
-------
* Support multiple streams ("Zones")
* Remote control: change client latency, volume, zone
