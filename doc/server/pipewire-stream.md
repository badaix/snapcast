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
- `device`: Target device/node to connect to (default: empty, auto-selects)
- `auto_connect`: Whether to automatically connect to available sources (default: true)
- `send_silence`: Send silent chunks when idle (default: false)
- `idle_threshold`: Duration of silence before switching to idle state in ms (default: 100)

### Examples

1. **Capture from default source:**
   ```
   source = pipewire://
   ```

2. **Capture from specific application:**
   ```
   source = pipewire://?device=Firefox
   ```

3. **Named stream without auto-connect:**
   ```
   source = pipewire://?name=SnapcastCapture&auto_connect=false
   ```

4. **With custom idle settings:**
   ```
   source = pipewire://?idle_threshold=500&send_silence=true
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

## Stream Registration

To enable PipeWire streams, you need to modify the stream factory. In `streamreader/stream_manager.cpp` or similar:

```cpp
#ifdef HAS_PIPEWIRE
#include "pipewire_stream.hpp"
#endif

// In the createStream function:
else if (uri.scheme == "pipewire")
{
#ifdef HAS_PIPEWIRE
    return std::make_shared<PipeWireStream>(pcmListener, ioc, server_settings, uri);
#else
    throw SnapException("PipeWire support not compiled in");
#endif
}
```
