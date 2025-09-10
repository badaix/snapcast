# PipeWire Player Usage Guide for Snapcast Client

## Overview

This implementation adds native PipeWire support to Snapcast client, allowing direct audio playback through PipeWire without going through ALSA or JACK compatibility layers.

The PipeWire player provides the same functionality as the PulseAudio player, including automatic silence detection and device resource management.

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

Use PipeWire as the audio player:

```bash
snapclient --player pipewire
```

### Player Parameters

- `buffer_time`: Audio buffer time in milliseconds (default: 100ms, minimum: 10ms)

### Examples

1. **Use default PipeWire sink:**

   ```bash
   snapclient --player pipewire
   ```

2. **Specify custom buffer time:**

   ```bash
   snapclient --player pipewire:buffer_time=50
   ```

3. **Target specific output device:**

   ```bash
   snapclient --player pipewire --soundcard alsa_output.pci-0000_00_1b.0.analog-stereo
   ```

4. **Combined parameters:**

   ```bash
   snapclient --player pipewire:buffer_time=200 --soundcard my_audio_device
   ```

## Device Selection and Management

The PipeWire player automatically lists available audio devices at startup:

```log
[Debug] (PipeWirePlayer) Found audio sink: alsa_output.pci-0000_00_1b.0.analog-stereo (Built-in Audio Analog Stereo)
[Debug] (PipeWirePlayer) Found audio sink: alsa_output.usb-Generic_USB_Audio-00.analog-stereo (USB Audio Device)
```

If no `soundcard` is specified, PipeWire automatically selects the default sink.

## PipeWire Graph Management

You can manage audio routing using PipeWire tools:

```bash
# List all nodes
pw-cli list-objects Node

# View the PipeWire graph
pw-dot
qpwgraph  # GUI tool

# List active connections
pw-link -l
```

PulseAudio clients like `pavucontrol` can also be used to manage PipeWire audio routing.

## Resource Management

The PipeWire player includes intelligent resource management:

- **Silence Detection**: Automatically disconnects from the audio device after 5 seconds of silence
- **Fast Reconnection**: Immediately reconnects when audio data becomes available

This behavior matches the PulseAudio player exactly and helps prevent audio device conflicts.

## Key Features

1. **Low Latency**: Direct PipeWire integration provides minimal audio latency
2. **Automatic Device Management**: Smart connection/disconnection based on audio activity  
3. **Volume Control**: Full hardware volume control support
4. **Device Discovery**: Automatic enumeration of available audio outputs
5. **Resource Efficient**: Releases audio device when not in use

## Troubleshooting

1. **No audio output**:
   - Check available devices: `snapclient -l`
   - Verify PipeWire is running: `systemctl --user status pipewire`

2. **Permission issues**:
   - Ensure your user has audio access
   - Check PipeWire session: `pw-cli info 0`

3. **Device not found**:
   - List available targets: `pw-cli list-objects Node | grep -A5 -B5 "audio.sink"`
   - Use exact node name from PipeWire

4. **High latency**:
   - Reduce buffer_time: `--player pipewire:buffer_time=50`
   - Check PipeWire quantum settings

## Replacing PulseAudio Usage

If you're currently using the PulseAudio player:

```bash
# Old PulseAudio usage
snapclient --player pulse

# New PipeWire equivalent  
snapclient --player pipewire
```

The PipeWire player provides identical functionality with potentially lower latency and better integration with modern Linux audio systems.

## Acknowledgements

This implementation was developed and tested as part of adding comprehensive PipeWire support to Snapcast. The implementation closely follows PipeWire best practices and official examples for robust audio playback.

Research for this implementation was done with perplexity AI. Most of the inital code was written by Claude Code AI, including this documentation.

However, all tests, prompt directions, and the initial PR were done by [aanno](https://github.com/aanno).

## References

- [pipewire examples](https://docs.pipewire.org/examples.html)
- [pw-cat (pw-play) documentation](https://docs.pipewire.org/page_man_pw-cat_1.html)
  - [pw-cat source code](https://github.com/PipeWire/pipewire/blob/master/src/tools/pw-cat.c)
