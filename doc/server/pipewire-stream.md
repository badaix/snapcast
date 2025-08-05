# PipeWire Stream Usage Guide for Snapcast

## Overview

This implementation adds native PipeWire support to Snapcast server, allowing direct audio capture from PipeWire without going through ALSA or JACK compatibility layers.

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
   ```
   source = pipewire://
   ```

2. **Capture from specific application/device:**
   ```
   source = pipewire://?target=Firefox
   ```

3. **Capture sink output (like pw-record with stream.capture.sink=true):**
   ```
   source = pipewire://?capture_sink=true&target=alsa_output.platform-snd_aloop.0.analog-stereo
   ```

4. **Named stream with custom idle settings:**
   ```
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
