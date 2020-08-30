# Configuration

## Sources

source URI of the PCM input stream, can be configured multiple times
The following notation is used in this paragraph:

- `<angle brackets>`: the whole expression must be replaced with your specific setting
- `[square brackets]`: the whole expression is optional and can be left out
- `[key=value]`: if you leave this option out, "value" will be the default for "key"

Format:

```sh
TYPE://host/path?name=<name>[&codec=<codec>][&sampleformat=<sampleformat>][&chunk_ms=<chunk ms>]
```

parameters have the form `key=value`, they are concatenated with an `&` character
parameter `name` is mandatory for all sources, while `codec`, `sampleformat` and `chunk_ms` are optional
and will override the default `codec`, `sampleformat` or `chunk_ms` settings.
Non blocking sources support the `dryout_ms` parameter: when no new data is read from the source, send silence to the clients

Available types are:

### pipe

```sh
pipe:///<path/to/pipe>?name=<name>[&mode=create][&dryout_ms=2000], mode can be "create" or "read"
```

### librespot

```sh
librespot:///<path/to/librespot>?name=<name>[&dryout_ms=2000][&username=<my username>&password=<my password>][&devicename=Snapcast][&bitrate=320][&wd_timeout=7800][&volume=100][&onevent=""][&normalize=false][&autoplay=false]
```

note that you need to have the librespot binary on your machine
sampleformat will be set to `44100:16:2`

### file

```sh
file:///<path/to/PCM/file>?name=<name>
```

### process

```sh
process:///<path/to/process>?name=<name>[&dryout_ms=2000][&wd_timeout=0][&log_stderr=false][&params=<process arguments>]
```

### airplay

```sh
airplay:///<path/to/airplay>?name=<name>[&dryout_ms=2000][&port=5000]
```

note that you need to have the airplay binary on your machine
sampleformat will be set to `44100:16:2`

### tcp server

```sh
tcp://<listen IP, e.g. 127.0.0.1>:<port>?name=<name>[&mode=server]
```

default for `port` (if omitted) is 4953, default for `mode` is `server`

mopidy.conf (running GStreamer in [client mode](https://www.freedesktop.org/software/gstreamer-sdk/data/docs/latest/gst-plugins-base-plugins-0.10/gst-plugins-base-plugins-tcpclientsink.html))

```sh
[audio]
output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! wavenc ! tcpclientsink
```

### tcp client

```sh
tcp://<server IP, e.g. 127.0.0.1>:<port>?name=<name>&mode=client
```

mopidy.conf (running GStreamer in [server mode](https://www.freedesktop.org/software/gstreamer-sdk/data/docs/latest/gst-plugins-base-plugins-0.10/gst-plugins-base-plugins-tcpserversink.html))

```sh
[audio]
output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! wavenc ! tcpserversink
```

### alsa

```sh
alsa://?name=<name>&device=<alsa device>
```

Snapcast v0.21 can capture audio from alsa, and to capture the output of a player, an alsa loopback device can be used:

1. setup the alsa loopback device by loading the kernel module:

    ```sh
    sudo modprobe snd-aloop
    ```

2. the loopback device should show up in `aplay -l`

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

    On my system it's card 0 with devices 0 and 1, each having 8 subdevices. You can address them with `hw:<card idx>,<device idx>,<subdevice num>`, i.e. `hw:0,0,0`.
    If a process plays audio using `hw:0,0,x`, then this audio is looped back to `hw:0,1,x`, so

3. Configure your player to use a loopback device.

    For mopidy (gstreamer) it should be something like this (not tested):

    ```sh
    output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! wavenc ! alsasink device=hw:0,0,0
    ```

    For mpd: in mpd.conf

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

4. Configure Snapserver to capture the loopback device:

    ```sh
    [stream]
    stream = alsa://?name=SomeName&sampleformat=48000:16:2&device=hw:0,1,0
    ```
