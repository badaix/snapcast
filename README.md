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

