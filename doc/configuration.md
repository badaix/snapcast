# Configuration

## Sources

Audio sources are added to the server as `source` in the `[stream]` section of the configuration file `/etc/snapserver.conf`. Every source must be fed with a fixed sample format, that can be configured per stream (e.g. `48000:16:2`).  

The following notation is used in this paragraph:

- `<angle brackets>`: the whole expression must be replaced with your specific setting
- `[square brackets]`: the whole expression is optional and can be left out
- `[key=value]`: if you leave this option out, "value" will be the default for "key"

The general format of an audio source is:

```sh
TYPE://host/path?name=<name>[&codec=<codec>][&sampleformat=<sampleformat>][&chunk_ms=<chunk ms>][&controlscript=<control script filename>[&controlscriptparams=<control script command line arguments>]]
```

Within the `[stream]` section there are some global parameters valid for all `source`s:

- `sampleformat`: Default sample format of the stream source, e.g. `48000:16:2`
- `codec`: The codec to use to save bandwith, one of:
  - `flac` [default]: lossless codec, mean codec latency ~26ms
  - `ogg`: lossy codec
  - `opus`: lossy low latency codec, only supports 48kHz, if your stream has a different sample rate, automatic resampling will be applied, introducing further latecy
  - `pcm`: lossless, uncompresssed. No latency.
- `chunk_ms`: Default source stream read chunk size [ms]. The server will continously read this number of milliseconds from the source into a buffer, before this buffer is passed to the encoder (the `codec` above)
- `buffer`: Buffer [ms]. The end-to-end latency, from capturing a sample on the server until the sample is played-out on the client
- `send_to_muted`: `true` or `false`: Send audio to clients that are muted

`source` parameters have the form `key=value`, they are concatenated with an `&` character.
Supported parameters for all source types:

- `name`: The mandatory source name
- `codec`: Override the global codec
- `sampleformat`: Override the global sample format
- `chunk_ms`: Override the global `chunk_ms`
- `controlscript`: Script to control the stream source and read and provide meta data, see [stream_plugin.md](json_rpc_api/stream_plugin.md)
- `controlscriptparams`: Control script command line arguments, must be url-encoded (use `%20` instead of a space " "), e.g. `--mopidy-host=192.168.42.23%20--debug`

Available audio source types are:

### pipe

Captures audio from a named pipe

```sh
pipe:///<path/to/pipe>?name=<name>[&mode=create]
```

`mode` can be `create` or `read`. Sometimes your audio source might insist in creating the pipe itself. So the pipe creation mode can by changed to "not create, but only read mode", using the `mode` option set to `read`

**NOTE** With newer kernels using FIFO pipes in a world writeable sticky dir (e.g. `/tmp`) one might also have to turn off `fs.protected_fifos`, as default settings have changed recently: `sudo sysctl fs.protected_fifos=0`. 

See [stackexchange](https://unix.stackexchange.com/questions/503111/group-permissions-for-root-not-working-in-tmp) for more details. You need to run this after each reboot or add it to /etc/sysctl.conf or /etc/sysctl.d/50-default.conf depending on distribution.

### librespot

Launches librespot and reads audio from stdout

```sh
librespot:///<path/to/librespot>?name=<name>[&username=<my username>&password=<my password>][&devicename=Snapcast][&bitrate=320][&wd_timeout=7800][&volume=100][&onevent=""][&normalize=false][&autoplay=false][&cache=""][&disable_audio_cache=false][&killall=false][&params=extra-params]
```

Note that you need to have the librespot binary on your machine and the sampleformat will be set to `44100:16:2`

#### Available parameters

Parameters used to configure the librespot binary ([see librespot-org options](https://github.com/librespot-org/librespot/wiki/Options)):

- `username`: Username to sign in with
- `password`: Password
- `devicename`: Device name
- `bitrate`: Bitrate (96, 160 or 320). Defaults to 320
- `volume`: Initial volume in %, once connected [0-100]
- `onevent`: The path to a script that gets run when one of librespot's events is triggered
- `normalize`: Enables volume normalisation for librespot
- `autoplay`: Autoplay similar songs when your music ends
- `cache`: Path to a directory where files will be cached
- `disable_audio_cache`: Disable caching of the audio data
- `params`: Optional string appended to the librespot invocation. This allows for arbitrary flags to be passed to librespot, for instance `params=--device-type%20avr`. The value has to be properly URL-encoded.

Parameters introduced by Snapclient:

- `killall`: Kill all running librespot instances before launching librespot
- `wd_timeout`: Restart librespot if it doesn't create log messages for x seconds

### airplay

Launches [shairport-sync](https://github.com/mikebrady/shairport-sync) and reads audio from stdout

```sh
airplay:///<path/to/shairport-sync>?name=<name>[&devicename=Snapcast][&port=5000][&password=<my password>]
```

Note that you need to have the shairport-sync binary on your machine and the sampleformat will be set to `44100:16:2`

#### Available parameters

Parameters used to configure the shairport-sync binary:

- `devicename`: Advertised name
- `port`: RTSP listening port (5000 for Airplay 1, 7000 for Airplay 2)
- `password`: Password
- `params`: Optional string appended to the shairport-sync invocation. This allows for arbitrary flags to be passed to shairport-sync, for instance `params=--on-start=start.sh%20--on-stop=stop.sh`. The value has to be properly URL-encoded.

### file

Reads PCM audio from a file

```sh
file:///<path/to/PCM/file>?name=<name>
```

### process

Launches a process and reads audio from stdout

```sh
process:///<path/to/process>?name=<name>[&wd_timeout=0][&log_stderr=false][&params=<process arguments>]
```

#### Available parameters

- `wd_timeout`: kill and restart the process if there was no message logged for x seconds to stderr (0 = disabled)
- `log_stderr`: Forward stderr log messages to Snapclient logging
- `params`: Params to start the process with

### tcp server

Receives audio from a TCP socket (acting as server)

```sh
tcp://<listen IP, e.g. 127.0.0.1>:<port>?name=<name>[&mode=server]
```

default for `port` (if omitted) is 4953, default for `mode` is `server`

Mopdiy configuration would look like this (running GStreamer in [client mode](https://www.freedesktop.org/software/gstreamer-sdk/data/docs/latest/gst-plugins-base-plugins-0.10/gst-plugins-base-plugins-tcpclientsink.html))

```sh
[audio]
output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! wavenc ! tcpclientsink host=127.0.0.1
```

### tcp client

Receives audio from a TCP socket (acting as client)

```sh
tcp://<server IP, e.g. 127.0.0.1>:<port>?name=<name>&mode=client
```

Mopdiy configuration would look like this (running GStreamer in [server mode](https://www.freedesktop.org/software/gstreamer-sdk/data/docs/latest/gst-plugins-base-plugins-0.10/gst-plugins-base-plugins-tcpserversink.html)):

```sh
[audio]
output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! wavenc ! tcpserversink host=127.0.0.1
```

### alsa

Captures audio from an alsa device

```sh
alsa:///?name=<name>&device=<alsa device>[&send_silence=false][&idle_threshold=100][&silence_threshold_percent=0.0]
```

#### Available parameters

- `device`: alsa device name or identifier, e.g. `default` or `hw:0,0` or `hw:0,0,0`
- `idle_threshold`: switch stream state from playing to idle after receiving `idle_threshold` milliseconds of silence
- `silence_threshold_percent`: percent (float) of the max amplitude to be considered as silence
- `send_silence`: forward silence to clients when stream state is `idle`

The output of any audio player that uses alsa can be redirected to Snapcast by using an alsa loopback device:

1. Setup the alsa loopback device by loading the kernel module:

    ```sh
    sudo modprobe snd-aloop
    ```

    The loopback device can be created during boot by adding `snd-aloop` to `/etc/modules`

2. The loopback device should show up in `aplay -l`

    ```sh
    aplay -l
    **** List of PLAYBACK Hardware Devices ****
    card 0: Loopback [Loopback], device 0: Loopback PCM [Loopback PCM]
    Subdevices: 8/8
    Subdevice #0: subdevice #0
    Subdevice #1: subdevice #1
    Subdevice #2: subdevice #2
    Subdevice #3: subdevice #3
    Subdevice #4: subdevice #4
    Subdevice #5: subdevice #5
    Subdevice #6: subdevice #6
    Subdevice #7: subdevice #7
    card 0: Loopback [Loopback], device 1: Loopback PCM [Loopback PCM]
    Subdevices: 8/8
    Subdevice #0: subdevice #0
    Subdevice #1: subdevice #1
    Subdevice #2: subdevice #2
    Subdevice #3: subdevice #3
    Subdevice #4: subdevice #4
    Subdevice #5: subdevice #5
    Subdevice #6: subdevice #6
    Subdevice #7: subdevice #7
    card 1: Intel [HDA Intel], device 0: CX20561 Analog [CX20561 Analog]
    Subdevices: 1/1
    Subdevice #0: subdevice #0
    card 1: Intel [HDA Intel], device 1: CX20561 Digital [CX20561 Digital]
    Subdevices: 1/1
    Subdevice #0: subdevice #0
    card 2: CODEC [USB Audio CODEC], device 0: USB Audio [USB Audio]
    Subdevices: 0/1
    Subdevice #0: subdevice #0
    ```

    In this example the loopback device is card 0 with devices 0 and 1, each having 8 subdevices.  
    The devices are addressed with `hw:<card idx>,<device idx>,<subdevice num>`, e.g. `hw:0,0,0`.  
    If a process plays audio using `hw:0,0,x`, then the audio will be looped back to `hw:0,1,x`

3. Configure your player to use a loopback device

    For mopidy (gstreamer) in `mopidy.conf`:

    ```sh
    output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! alsasink device=hw:0,0,0
    ```

    For mpd: in `mpd.conf`

    ```sh
    audio_output_format            "48000:16:2"
    audio_output {
            type            "alsa"
            name            "My ALSA Device"
            device          "hw:0,0,0"      # optional
    #       auto_resample   "no"
    #       mixer_type      "hardware"      # optional
    #       mixer_device    "default"       # optional
    #       mixer_control   "PCM"           # optional
    #       mixer_index     "0"             # optional
    }
    ```

    For [librespot](https://github.com/librespot-org/librespot) (check previusly if your librespot binary is compiled with alsa backend with `./librespot --backend ?`):
    Warning, you need to set snapserver rate to 44100`source = alsa:///?name=Spotify&sampleformat=44100:16:2&device=hw:0,1,0`

    ```sh
    ./librespot -b 320 --disable-audio-cache --name Snapcast --initial-volume 100 --backend alsa --device hw:0,0,0
    ```

4. Configure Snapserver to capture the loopback device:

    ```sh
    [stream]
    source = alsa:///?name=SomeName&device=hw:0,1,0
    ```

### meta

Read and mix audio from other stream sources

```sh
meta:///<name of source#1>/<name of source#2>/.../<name of source#N>?name=<name>
```

Plays audio from the active source with the highest priority, with `source#1` having the highest priority and `source#N` the lowest.  
Use `codec=null` for stream sources that should only serve as input for meta streams

## Streaming clients

Streaming clients connect to the server and receive configuration and audio data. The client is fully controlled from the server so clients don't have to persist any state. The `[streaming_client]` section has just one option currently:

- `initial_volume`: 0-100 [percent]: The volume a streaming client gets assigned on very first connect (i.e. the client is not known to the server yet). Defaults to 100 if unset.
