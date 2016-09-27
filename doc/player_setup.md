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

###MPlayer setup
Use `-novideo` and `-ao` to pipe MPlayer's audio output to the snapfifo:

    mplayer http://wms-15.streamsrus.com:11630 -novideo -channels 2 -srate 48000 -af format=s16le -ao pcm:file=/tmp/snapfifo

###Alsa setup
If the player cannot be configured to route the audio stream into the snapfifo, Alsa or PulseAudio can be redirected, resulting in this chain:

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

PulseAudio will create the pipe file for itself and will fail if it already exsits, see the [Configuration section](https://github.com/badaix/snapcast#configuration) in the main readme file on how to change the pipe creation mode to read-only.

Load the module `pipe-sink` like this:

    pacmd load-module module-pipe-sink file=/tmp/snapfifo sink_name=Snapcast
    pacmd update-sink-proplist Snapcast device.description=Snapcast

It might me neccessary to set the pulse audio latency environment variable to 60 msec: `PULSE_LATENCY_MSEC=60`
