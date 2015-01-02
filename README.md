SnapCast
========

Synchronous audio player

Installation
------------
These installation instructions are valid for Debian derivates (e.g. Raspbian).
First install all packages needed to compile snapcast:

    $ sudo apt-get install libboost-dev libboost-system-dev libboost-program-options-dev libasound2-dev libvorbis-dev alsamixer

Build snapcast by cd'ing into the snapcast src-root directory

    $ cd <MY_SNAPCAST_ROOT>
    $ make all
    
Install snapclient and/or snapserver. The client installation will ask you for the server's hostname or ip address

    $ make installserver
    $ make installclient

Test
----
You can test your installation by copying random data into the server's fifo file

    $ cat /dev/urandom > /tmp/snapfifo

All connected clients should play random noise now


WiFi setup:
http://www.maketecheasier.com/setup-wifi-on-raspberry-pi/

MPD setup
---------
Edit /etc/mpd.conf, so that mpd will feed the audio into the snap-server's named pipe

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

Add a new audio output of the type "fifo", which will let mpd play audio into the named pipe "/tmp/snapfifo"

    audio_output {
        type            "fifo"
        name            "my pipe"
        path            "/tmp/snapfifo" 
        format          "48000:16:2"
        mixer_type      "software"
    } 

