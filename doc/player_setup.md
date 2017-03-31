Setup of audio players/server
-----------------------------
Snapcast can be used with a number of different audio players and servers, and so it can be integrated into your favorite audio-player solution and make it synced-multiroom capable.
The only requirement is that the player's audio can be redirected into the Snapserver's fifo `/tmp/snapfifo`. In the following configuration hints for [MPD](http://www.musicpd.org/) and [Mopidy](https://www.mopidy.com/) are given, which are base of other audio player solutions, like [Volumio](https://volumio.org/) or [RuneAudio](http://www.runeaudio.com/) (both MPD) or [Pi MusicBox](http://www.pimusicbox.com/) (Mopidy).

The goal is to build the following chain:

    audio player software -> snapfifo -> snapserver -> network -> snapclient -> alsa

#### About the notation
In this document some expressions are in brackets:
* `<angle brackets>`: the whole expression must be replaced with you specific setting
* `[square brackets]`: the whole expression is optional and can be left out
  * `[key=value]`: if you leave this option out, `value` will be the default for `key`

For example:
```
-s "spotify:///librespot?name=Spotify[&username=<my username>&password=<my password>][&devicename=Snapcast][&bitrate=320]"
```
* `username` and `password` are both optional in this case. You need to specify none of both of them
* `bitrate` is optional. If not configured, 320 will be used. Same for `devicename` 

A valid usage would be for instance:
```
-s "spotify:///librespot?name=Spotify&bitrate=160"
```

### MPD
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

### Mopidy
[Mopidy](https://www.mopidy.com/) can stream the audio output into the Snapserver's fifo with a `filesink` as audio output in `mopidy.conf`:

    [audio]
    #output = autoaudiosink
    output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! wavenc ! filesink location=/tmp/snapfifo

### FFmpeg
Pipe FFmpeg's audio output to the snapfifo:

    ffmpeg -y -i http://wms-15.streamsrus.com:11630 -f u16le -acodec pcm_s16le -ac 2 -ar 48000 /tmp/snapfifo

### mpv
Pipe mpv's audio output to the snapfifo:

    mpv http://wms-15.streamsrus.com:11630 --audio-display=no --audio-channels=stereo --audio-samplerate=48000 --audio-format=s16 --ao=pcm --ao-pcm-file=/tmp/snapfifo

### MPlayer
Use `-novideo` and `-ao` to pipe MPlayer's audio output to the snapfifo:

    mplayer http://wms-15.streamsrus.com:11630 -novideo -channels 2 -srate 48000 -af format=s16le -ao pcm:file=/tmp/snapfifo

### Alsa
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

### PulseAudio
Redirect the PulseAudio stream into the snapfifo:

    audio player software -> PulseAudio -> PulsaAudio pipe sink -> snapfifo -> snapserver -> network -> snapclient -> Alsa

PulseAudio will create the pipe file for itself and will fail if it already exsits, see the [Configuration section](https://github.com/badaix/snapcast#configuration) in the main readme file on how to change the pipe creation mode to read-only.

Load the module `pipe-sink` like this:

    pacmd load-module module-pipe-sink file=/tmp/snapfifo sink_name=Snapcast
    pacmd update-sink-proplist Snapcast device.description=Snapcast

It might be neccessary to set the pulse audio latency environment variable to 60 msec: `PULSE_LATENCY_MSEC=60`


### AirPlay
Snapserver supports [shairport-sync](https://github.com/mikebrady/shairport-sync) with `stdout` backend.
 1. Build shairport-sync with `stdout` backend: `./configure --with-stdout --with-avahi --with-ssl=openssl --with-metadata`
 2. Copy the `shairport-sync` binary somewhere to your `PATH`, e.g. `/usr/local/bin/`
 3. Configure snapserver with `-s "airplay:///shairport-sync?name=Airplay[&devicename=Snapcast][&port=5000]"`
 

### Spotify
Snapserver supports [librespot](https://github.com/plietar/librespot) with `pipe` backend.
 1. Build and copy the `librespot` binary somewhere to your `PATH`, e.g. `/usr/local/bin/`
 2. Configure snapserver with `-s "spotify:///librespot?name=Spotify[&username=<my username>&password=<my password>][&devicename=Snapcast][&bitrate=320][&onstart=/usr/bin/logger -t Snapcast Starting spotify...][&onstop=/usr/bin/logger -t Snapcast Stopping spotify...]"`
   * Valid bitrates are 96, 160, 320


### Process
Snapserver can start any process and read PCM data from the stdout of the process: 

Configure snapserver with `-s "process:///path/to/process?name=Process[&params=<--my list --of params>][&logStderr=false]"`


### Line-in
Audio captured from line-in can be redirected to the snapserver's pipe, e.g. by using:

#### cpiped
[cpipe](https://github.com/b-fitzpatrick/cpiped)

#### PulseAudio
`parec >/tmp/snapfifo` (defaults to 44.1kHz,16bit,stereo)
