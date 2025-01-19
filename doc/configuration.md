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

Mopidy configuration would look like this (running GStreamer in [client mode](https://www.freedesktop.org/software/gstreamer-sdk/data/docs/latest/gst-plugins-base-plugins-0.10/gst-plugins-base-plugins-tcpclientsink.html))

```sh
[audio]
output = audioresample ! audioconvert ! audio/x-raw,rate=48000,channels=2,format=S16LE ! wavenc ! tcpclientsink host=127.0.0.1
```

### tcp client

Receives audio from a TCP socket (acting as client)

```sh
tcp://<server IP, e.g. 127.0.0.1>:<port>?name=<name>&mode=client
```

Mopidy configuration would look like this (running GStreamer in [server mode](https://www.freedesktop.org/software/gstreamer-sdk/data/docs/latest/gst-plugins-base-plugins-0.10/gst-plugins-base-plugins-tcpserversink.html)):

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

### jack

Reads audio from a Jack server. Snapcast must be built with the cmake flag `BUILD_WITH_JACK=ON` (= default) to enable Jack support.

```sh
jack:///?name=<name>[sampleformat=48000:16:2][autoconnect=][autoconnect_skip=0][&send_silence=false][&idle_threshold=100]
```

#### Available parameters

- `server_name`: The Jack server name to connect to, leave empty for "default"
- `autoconnect`: Regular expression to match Jack ports to auto-connect to stream inputs
- `autoconnect_skip`: Skip this number of matches from the regular expression
- `idle_threshold`: switch stream state from playing to idle after receiving `idle_threshold` milliseconds of silence
- `silence_threshold_percent`: percent (float) of the max amplitude to be considered as silence
- `send_silence`: forward silence to clients when stream state is `idle`

#### Description of Jack stream

Each `jack` stream creates a separate connection to a jack server and registers
as many input ports as there are audio channels in this stream (according to
`sampleformat`).

You can use `autoconnect` to automatically connect this streams input ports
with Jack output ports. The parameter takes a regular expression and matches
against the whole Jack port name (including client name). For example, if you
have a Jack client named "system" with four output ports ("playback_1",
"playback_2", ...) and you want each output as a separate SnapCast stream, you
could either autoconnect to the exact ports, or you use an `autoconnect` search
term that returns all ports and use `autoconnect_skip` to pick the right one:

```ini
jack:///?name=Channel1&sampleformat=48000:16:1&autoconnect=system:playback_
jack:///?name=Channel2&sampleformat=48000:16:1&autoconnect=system:playback_&autoconnect_skip=1
jack:///?name=Channel3&sampleformat=48000:16:1&autoconnect=system:playback_&autoconnect_skip=2
jack:///?name=Channel4&sampleformat=48000:16:1&autoconnect=system:playback_&autoconnect_skip=3
```

#### Limitations

- Currently all `jack` streams need to match the sample rate of the Jack server.

- The `chunk_ms` parameter is ignored for jack streams. The Jack buffer size
  (Frames/Period) is used instead.

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

## HTTP

Snapserver supports RPC via HTTP(S) and WS(S) as well as audio streaming over WS(S). To enable HTTP and WS, the parameter `enabled` must be set to `true` (default) in the `[http]` section.

### HTTPS

For HTTPS/WSS, the paramter `ssl_enabled` must be set to `true` (default: `false`) and the `certificate` and `certificate_key` paramters in the `[ssl]` section must point to a certificate file and key file in PEM format.

Some hints on how to create a certificate and a private key are given for instance here:

- [Create Root CA (Done once)](https://gist.github.com/fntlnz/cf14feb5a46b2eda428e000157447309)
- [Create Your Own SSL Certificate Authority for Local HTTPS Development](https://deliciousbrains.com/ssl-certificate-authority-for-local-https-development/)

A certificate, signed by a CA, and a certificate key can be created like this:

Create an X509 V3 certificate extension config file `snapserver.ext`, which is used to define the Subject Alternative Name (SAN) for the certificate.  
Set the `DNS.x` values to your snapserver's IPs and domain names:

```ini
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names

[alt_names]
DNS.1 = <IP or domain name>
DNS.2 = <another IP or domain name>
...
DNS.x = <another IP or domain name>
```

Use OpenSSL to create the CA certificate `snapcastCA.crt`, the CA key `snapcastCA.key`, the snapserver certificate `snapserver.crt`, signed by the `snapcastCA` and the snapserver key `snapserver.key`:

```shell
# Create a private CA key file "snapcastCA.key"
openssl genrsa -out snapcastCA.key 4096
# Generate a root certificate "snapcastCA.crt"
openssl req -x509 -new -nodes -key snapcastCA.key -sha256 -days 3650 -out snapcastCA.crt -subj "/C=DE/ST=NRW/O=Badaix"
# Create a certificate key file "snapserver.key" for snapserver
openssl genrsa -out snapserver.key 2048
# Create a certificate signing request (CSR) "snapserver.csr"
openssl req -new -sha256 -key snapserver.key -subj "/C=DE/ST=NRW/O=Badaix/CN=Snapserver" -addext "subjectAltName=DNS:laptop.local,DNS:127.0.0.1" -out snapserver.csr
# Create a certificate file "snapserver.crt", signed by the root CA
openssl x509 -req -in snapserver.csr -CA snapcastCA.crt -CAkey snapcastCA.key -CAcreateserial -out snapserver.crt -days 3650 -sha256 -extfile snapserver.ext
```

Copy `snapserver.crt` and `snapserver.key` into the `/etc/snapserver/certs/` directory and configure `[ssl]` in `snapserver.conf` with:

```ini
[ssl]
...
# Certificate file in PEM format
certificate = snapserver.crt

# Private key file in PEM format
certificate_key = snapserver.key
```

Install the CA certificate `snapcastCA.crt` on you client's OS or browser.
