# PipeWire Stream Usage Guide for Snapcast

## Overview

This implementation adds native PipeWire support to Snapcast server, allowing direct audio capture from PipeWire without going through ALSA or JACK compatibility layers.

It is the solution to [issue 1371](https://github.com/badaix/snapcast/issues/1371).

## Building with PipeWire Support

1. Ensure PipeWire development packages are installed:

   ```bash
   # Fedora/RHEL
   sudo dnf install pipewire-devel
   
   # Ubuntu/Debian
   sudo apt install libpipewire-0.3-dev
   ```

2. Build Snapcast with PipeWire support:

   ```bash
   cmake .. -DBUILD_WITH_PIPEWIRE=ON
   make
   ```

## Usage

### Basic Usage

In your `snapserver.conf`, add a PipeWire stream:

```ini
[stream]
source = pipewire://?name=Snapcast&device=&auto_connect=true
```

### URI Parameters

- `name`: The name of the PipeWire stream (default: "Snapcast")
- `target`: Target device/node to connect to (default: empty, auto-selects)
- `capture_sink`: Capture from sink outputs instead of sources (default: false)
- `send_silence`: Send silent chunks when idle (default: false)
- `idle_threshold`: Duration of silence before switching to idle state in ms (default: 100)

### Examples

1. **Capture from default source:**

   ```ini
   source = pipewire://?name=<your_stream_name>
   ```

2. **Capture from specific application/device:**

   ```ini
   source = pipewire://?name=<your_stream_name>&target=Firefox
   ```

3. **Capture sink output (like pw-record with stream.capture.sink=true):**

   ```ini
   source = pipewire://?name=<your_stream_name>&capture_sink=true&target=alsa_output.platform-snd_aloop.0.analog-stereo
   ```

4. **Named stream with custom idle settings:**

   ```ini
   source = pipewire://?name=SnapcastCapture&idle_threshold=500&send_silence=true
   ```

## PipeWire Graph Management

You can use PipeWire tools to manage connections:

```bash
# List all nodes
pw-cli list-objects Node

# View the PipeWire graph
pw-dot
qpwgraph  # GUI tool

# Connect manually if auto_connect=false
pw-link "Snapcast:input_FL" "Firefox:output_FL"
pw-link "Snapcast:input_FR" "Firefox:output_FR"
```

Pulseaudio clients like the `pavucontrol` UI can also be used.

## Key Differences from ALSA Implementation

1. **Callback-based**: PipeWire uses callbacks instead of polling
2. **Graph-based**: Audio routing is handled through the PipeWire graph
3. **Lower latency**: Direct PipeWire integration can provide lower latency
4. **Better integration**: Works seamlessly with PipeWire's session management

## Troubleshooting

1. **No audio captured**: Check PipeWire connections with `pw-link -l`
2. **Permission issues**: Ensure your user is in the `audio` group
3. **High CPU usage**: Adjust buffer sizes in PipeWire configuration

## Replacing pw-record

If you're currently using pw-record to capture audio into a FIFO, like:

```bash
pw-record -P stream.capture.sink=true --target alsa_output.platform-snd_aloop.0.analog-stereo - >/tmp/snapfifo
```

You can replace it with a native PipeWire stream in snapserver:

```ini
source = pipewire://?capture_sink=true&target=alsa_output.platform-snd_aloop.0.analog-stereo
```

This eliminates the need for FIFOs and external processes, providing better performance and lower latency.

## Using with sound loopback devices

It is possible (but not needed) to use pipewire-stream with sound loopback.

Load the loopback module temporarily with:

```bash
sudo modprobe snd-aloop
```

Or permanently by creating a file `/etc/modules-load.d/snd_aloop.conf` like this:

```bash
$ cat /etc/modules-load.d/snd_aloop.conf 
snd_aloop
```

## Alternative: Use the `libpipewire-module-snapcast-discover` from PipeWire

An alternative to using the `pipewire-stream` source in Snapcast is to use the `libpipewire-module-snapcast-discover` module from PipeWire. This module allows PipeWire clients to automatically discover and connect to snapserver.

For details, see the [PipeWire documentation](https://docs.pipewire.org/page_module_snapcast_discover.html)
and [issue 1371](https://github.com/badaix/snapcast/issues/1371).

Using `libpipewire-module-snapcast-discover` allows for discover snapserver on the (sub) network.

Using pipewire-stream is more direct, avoids loopback networking, but is restricted to snapserver on the local machine. It is - of course - better integrated with PipeWire and may feel more naturally because of that.

## Acknowledgements

Research for this implementation was done with perplexity AI. Most of the inital code was written by Claude Code AI, including this documentation.

However, all tests, prompt directions, and the initial PR were done by [aanno](https://github.com/aanno).
