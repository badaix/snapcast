# Setup of audio players/server

Snapcast can be used with a number of different audio players and servers, and so it can be integrated into your favorite audio-player solution and make it synced-multiroom capable.
The only requirement is that the player's audio can be redirected into the Snapserver's fifo `/tmp/snapfifo`. In the following configuration hints for [MPD](http://www.musicpd.org/) and [Mopidy](https://www.mopidy.com/) are given, which are the base of other audio player solutions, like [Volumio](https://volumio.org/) or [RuneAudio](http://www.runeaudio.com/) (both MPD) or [Pi MusicBox](http://www.pimusicbox.com/) (Mopidy).

The goal is to build the following chain:

```plain_text
audio player software -> snapfifo -> snapserver -> network -> snapclient -> alsa
```

**NOTE** With newer kernels using FIFO pipes in a world writeable sticky dir (e.g. `/tmp`) one might also have to turn off `fs.protected_fifos`, as default settings have changed recently: `sudo sysctl fs.protected_fifos=0`. 

See [stackexchange](https://unix.stackexchange.com/questions/503111/group-permissions-for-root-not-working-in-tmp) for more details. You need to run this after each reboot or add it to /etc/sysctl.conf or /etc/sysctl.d/50-default.conf depending on distribution.

## Streams

Snapserver can read audio from several sources, which are configured in the `snapserver.conf` file (default location is `/etc/snapserver.conf`); the config file can be changed with the `-c` parameter.  
Within the config file a list of pcm sources can be configured in the `[stream]` section:

```ini
[stream]
...
# stream URI of the PCM input stream, can be configured multiple times
# Format: TYPE://host/path?name=NAME[&codec=CODEC][&sampleformat=SAMPLEFORMAT]
source = pipe:///tmp/snapfifo?name=default
...
```

The sampleformat is a triple of `<samplerate>:<bit depth>:<channels>`, e.g. `44100:16:2`.  
The PCM samples (bit depth) must be encoded signed, little endian in 8, 16, 24, or 32 bit, where 24 is expected to be encoded in the lower three bytes of a 32 bit word.

## About the notation

In this document some expressions are in brackets:

* `<angle brackets>`: the whole expression must be replaced with your specific setting
* `[square brackets]`: the whole expression is optional and can be left out
  * `[key=value]`: if you leave this option out, `value` will be the default for `key`

For example:

```ini
source = spotify:///librespot?name=Spotify[&username=<my username>&password=<my password>][&devicename=Snapcast][&bitrate=320]
```

* `username` and `password` are both optional in this case. You need to specify neither or both of them.
* `bitrate` is optional. If not configured, `320` will be used.
* `devicename` is optional. If not configured, `Snapcast` will be used.

For instance, a valid usage would be:

```ini
source = spotify:///librespot?name=Spotify&bitrate=160
```

### MPD

To connect [MPD](http://www.musicpd.org/) to the Snapserver, edit `/etc/mpd.conf`, so that mpd will feed the audio into the snapserver's named pipe.

Disable alsa audio output by commenting out this section:

```plain_text
#audio_output {
#       type            "alsa"
#       name            "My ALSA Device"
#       device          "hw:0,0"        # optional
#       format          "48000:16:2"    # optional
#       mixer_device    "default"       # optional
#       mixer_control   "PCM"           # optional
#       mixer_index     "0"             # optional
#}
```

Add a new audio output of the type "fifo", which will let mpd play audio into the named pipe `/tmp/snapfifo`.
Make sure that the "format" setting is the same as the format setting of the Snapserver (default is "48000:16:2", which should make resampling unnecessary in most cases).

```plain_text
audio_output {
    type            "fifo"
    name            "my pipe"
    path            "/tmp/snapfifo"
    format          "48000:16:2"
    mixer_type      "software"
}
```

To test your mpd installation, you can add a radio station by

```sh
sudo su
echo "http://wdr-1live-live.icecast.wdr.de/wdr/1live/live/mp3/128/stream.mp3" > /var/lib/mpd/playlists/einslive.m3u
```

### Mopidy

[Mopidy](https://www.mopidy.com/) can stream the audio output into the Snapserver's fifo with a `filesink` as audio output in `mopidy.conf`:

```ini
[audio]
#output = autoaudiosink
output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! filesink location=/tmp/snapfifo
```

With newer kernels one might also have to change this sysctl-setting, as default settings have changed recently: `sudo sysctl fs.protected_fifos=0`

See [stackexchange](https://unix.stackexchange.com/questions/503111/group-permissions-for-root-not-working-in-tmp) for more details. You need to run this after each reboot or add it to /etc/sysctl.conf or /etc/sysctl.d/50-default.conf depending on distribution.

### FFmpeg

Pipe FFmpeg's audio output to the snapfifo:

```sh
ffmpeg -y -i http://wms-15.streamsrus.com:11630 -f u16le -acodec pcm_s16le -ac 2 -ar 48000 /tmp/snapfifo
```

### mpv

Pipe mpv's audio output to the snapfifo. For version < 0.21.0:

```sh
mpv http://wms-15.streamsrus.com:11630 --audio-display=no --audio-channels=stereo --audio-samplerate=48000 --audio-format=s16 --ao=pcm:file=/tmp/snapfifo
```

For version >= 0.21.0:

```sh
mpv http://wms-15.streamsrus.com:11630 --audio-display=no --audio-channels=stereo --audio-samplerate=48000 --audio-format=s16 --ao=pcm --ao-pcm-file=/tmp/snapfifo
```

### MPlayer

Use `-novideo` and `-ao` to pipe MPlayer's audio output to the snapfifo:

```sh
mplayer http://wms-15.streamsrus.com:11630 -novideo -channels 2 -srate 48000 -af format=s16le -ao pcm:file=/tmp/snapfifo
```

### Alsa

If the player cannot be configured to route the audio stream into the snapfifo, Alsa or PulseAudio can be redirected, resulting in this chain:

```plain_text
    audio player software -> Alsa -> Alsa file plugin -> snapfifo -> snapserver -> network -> snapclient -> Alsa
```

Edit or create your Alsa config `/etc/asound.conf` like this:

```plain_text
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

```plain_text
audio player software -> PulseAudio -> PulseAudio pipe sink -> snapfifo -> snapserver -> network -> snapclient -> Alsa
```

PulseAudio will create the pipe file for itself and will fail if it already exists; see the [Configuration section](https://github.com/badaix/snapcast#configuration) in the main README file on how to change the pipe creation mode to read-only.

Load the module `pipe-sink` like this:

```plain_text
pacmd load-module module-pipe-sink file=/tmp/snapfifo sink_name=Snapcast format=s16le rate=48000
pacmd update-sink-proplist Snapcast device.description=Snapcast
```

It might be necessary to set the PulseAudio latency environment variable to 60 msec: `PULSE_LATENCY_MSEC=60`

### AirPlay

Snapserver supports [shairport-sync](https://github.com/mikebrady/shairport-sync) with the `stdout` backend and metadata support.
These options are passed as parameters when Snapcast invokes Shairport-sync, so it _should_ work without modifying the shairport-sync.conf file.

Some distros have shairport-sync packages in their main package repositories, and you may find that they 'just work', as long as you **disable the service**, so that Snapcast has full control of the shairport-sync process. Here is an example for some debian-based distros, but the need for sudo, the name of the package manager, and the syntax to install and control services may vary in your case

```sh
sudo apt install -y shairport-sync
sudo systemctl disable --now shairport-sync
```

Here is an example configuration line for /etc/snapserver.conf but please see [the Snapcast Airplay configuration guide](configuration.md#airplay) for more details on the syntax and options.

```plaintext
source = airplay:///shairport-sync?name=Airplay
```

#### in case of issues

Although this _might_ be a quick and convenient way to set up an Airplay source stream, the Snapcast project has no control over how the package is compiled for different distros. Therefore, if you have issues getting the ready-compiled package to work, we recommend that you compile it yourself and re-test, before raising an issue against Snapcast.

 1. Install dependencies. For debian derivates: `apt-get install autoconf libpopt-dev libconfig-dev libssl-dev`  
 2. Build shairport-sync (version 3.3 or later) with `stdout` backend:
    * `autoreconf -i -f` 
    * `./configure --with-stdout --with-avahi --with-ssl=openssl --with-metadata`
 3. Copy the `shairport-sync` binary somewhere to your `PATH`, e.g. `/usr/local/bin/`
 4. Configure snapserver with `source = airplay:///shairport-sync?name=Airplay[&devicename=Snapcast][&port=PORT]`
    * `PORT` is 5000 for Airplay 1 and 7000 for Airplay 2 

### Spotify

Snapserver supports [librespot](https://github.com/librespot-org/librespot) with the `pipe` backend.

 1. Build and copy the `librespot` binary somewhere to your `PATH`, e.g. `/usr/local/bin/`
 2. Configure snapserver with `source = spotify:///librespot?name=Spotify[&username=<my username>&password=<my password>][&devicename=Snapcast][&bitrate=320][&onstart=<start command>][&onstop=<stop command>][&volume=<volume in percent>][&cache=<cache dir>][&disable_audio_cache=false][&killall=false]`
    * Valid bitrates are 96, 160, 320
    * `start command` and `stop command` are executed by Librespot at start/stop
        * For example: `onstart=/usr/bin/logger -t Snapcast Starting spotify...`
    * If `killall` is `true`, all running instances of Librespot will be killed. This MUST be disabled on all spotify streams by setting it to `false` if you want to use multiple spotify streams.
    * If `disable_audio_cache` is `false` (default), downloaded audio files are cached in `<cache dir>`. If set to `true` audio files will not be cached on disk.

### Process

Snapserver can start any process and read PCM data from the stdout of the process:

Configure snapserver with `source = process:///path/to/process?name=Process[&params=<--my list --of params>][&log_stderr=false]`

For example, you could install the minimalist **mpv** media player to pick up WebRadio from a given url ...

```ini
[stream]
# mpv < 0.21.0
#source = process:///usr/bin/mpv?name=Webradio&sampleformat=48000:16:2&params=http://129.122.92.10:88/broadwavehigh.mp3 --no-terminal --audio-display=no --audio-channels=stereo --audio-samplerate=48000 --audio-format=s16 --ao=pcm:file=/dev/stdout
# mpv >= 0.21.0
source = process:///usr/bin/mpv?name=Webradio&sampleformat=48000:16:2&params=http://129.122.92.10:88/broadwavehigh.mp3 --no-terminal --audio-display=no --audio-channels=stereo --audio-samplerate=48000 --audio-format=s16 --ao=pcm --ao-pcm-file=/dev/stdout

```

### Line-in

#### ALSA Method

Audio can be played directly through the line-in via ALSA. The following guide was written in regards to a raspberry pi using a HiFiBerry product but it should roughly apply for line-in with other hardware.

1. Get a list of recording devices from ALSA.

    ```shell
    arecord -l
    ```

2. You will receive output in the following format. Note the `<device_name>` of your input device for the next step.

    ```shell
    card <number>: <device_name> ....
    ```

3. Edit the file `/etc/snapserver.conf` and add the following line, substituting `<device_name>` for the value derived from the previous step. Pick whatever you'd like for `<stream_name>`.
    ```
    stream = alsa:///?name=<stream_name>&device=hw:<device_name>
    ```
4. Restart the snapserver service.
   ```
   sudo service snapserver restart
   ```
5. You are done. Enjoy your new snapserver with line in. However, if you'd like to run the client on the same machine as the server then continue with the remaining steps.
6. Get the sound devices as far as snapclient is concerned. You are looking for the device name. This is probably the same as the `aplay -l` device names so may be able to use that instead.
   ```
   snapclient -l
   ```
7. In the output you are looking for the `hw:CARD` line that corresponds with the output device you want to use. Note the `<device_name>` for the next step.
   ```
   <number> hw:CARD=<device_name>,DEV=<device_number>
   ```
8. Edit the file `/etc/default/snapclient` and modify the `SNAPCLIENT_OPTS`. 
   ```
   SNAPCLIENT_OPTS=" --host <hostname> -s <dervice_name> "
   ```
9. Restart the snapclient service.
   ```
   sudo service snapclient restart
   ```

#### Pipe Method

Audio captured from line-in can be redirected to the snapserver's pipe, e.g. by using:

##### cpiped

[cpipe](https://github.com/b-fitzpatrick/cpiped)

##### PulseAudio

`parec >/tmp/snapfifo` (defaults to 44.1kHz, 16bit, stereo)

#### VLC

Use `--aout afile` and `--audiofile-file` to pipe VLC's audio output to the snapfifo:

```sh
vlc --no-video --aout afile --audiofile-file /tmp/snapfifo
```
