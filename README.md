SnapCast
========

Synchronous audio player

Snapcast is a multi room client-server audio player, where all clients are time synchronized with the server to play perfectly synced audio.
The server's audio input is a named pipe (/tmp/snapfifo). All data that is fed into this file, will be send to the connected clients. One of the most generic ways to use snapcast is in conjunction with the music player daemon (mpd), which can by condfigured to use a named pipe as audio output.


Installation
------------
These installation instructions are valid for Debian derivates (e.g. Raspbian).
First install all packages needed to compile snapcast:

    $ sudo apt-get install libboost-dev libboost-system-dev libboost-program-options-dev libasound2-dev libvorbis-dev alsamixer

Build snapcast by cd'ing into the snapcast src-root directory

    $ cd <MY_SNAPCAST_ROOT>
    $ make all
    
Install snapclient and/or snapserver. The client installation will ask you for the server's hostname or ip address

    $ sudo make installserver
    $ sudo make installclient

This will copy the client and/or server binary to /usr/sbin and update init.d to start the client/server as a daemon.


Test
----
You can test your installation by copying random data into the server's fifo file

    $ cat /dev/urandom > /tmp/snapfifo

All connected clients should play random noise now. You might raise the client's volume with "alsamixer".


To setup WiFi on a raspberry pi, you can follow this guide:
http://www.maketecheasier.com/setup-wifi-on-raspberry-pi/


MPD setup
---------
To connect MPD to the snapserver, edit /etc/mpd.conf, so that mpd will feed the audio into the snap-server's named pipe

Disable alsa audio output by commenting the section:

    #audio_output {
    #       type            "alsa"
    #       name            "My ALSA Device"
    #       device          "hw:0,0"        # optional
    #       format          "44100:16:2"    # optional
    #       mixer_device    "default"       # optional
    #       mixer_control   "PCM"           # optional
    #       mixer_index     "0"             # optional
    #}

Add a new audio output of the type "fifo", which will let mpd play audio into the named pipe "/tmp/snapfifo".
Make sure that the "format" setting is the same as the format setting of the snapserver (default is "44100:16:2", which should make resampling unnecessary in most cases)

    audio_output {
        type            "fifo"
        name            "my pipe"
        path            "/tmp/snapfifo" 
        format          "44100:16:2"
        mixer_type      "software"
    } 

