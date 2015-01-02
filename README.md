SnapCast
========

Synchronous audio player

Installation
------------

    $ sudo apt-get install libboost-dev libboost-system-dev libboost-program-options-dev libasound2-dev libvorbis-dev alsamixer

WiFi setup:
http://www.maketecheasier.com/setup-wifi-on-raspberry-pi/

MPD setup
---------

    disable "audio_output alsa"
    add
    audio_output {
        type            "fifo"
        name            "my pipe"
        path            "/tmp/snapfifo" 
        format          "48000:16:2"
        mixer_type      "software"
    } 

