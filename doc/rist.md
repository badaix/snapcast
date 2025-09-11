# RIST Implementation in Snapcast

## Overview

Snapcast implements RIST (Reliable Internet Stream Transport) support using a clean, bidirectional transport architecture that aligns with RIST's native design principles. This document describes our implementation approach, architectural decisions, and the journey that led to the current design.

RIST is an open protocol for low-latency, reliable streaming over unreliable IP networks, ideal for live audio/video contribution and broadcasting. It uses UDP for low-latency transport, with RTP for media synchronization, and adds reliability through ARQ (Automatic Repeat reQuest) and optional FEC (Forward Error Correction).

It is primarily designed for video, but Snapcast leverages it for audio streaming, benefiting from its robustness and low-latency capabilities. As RIST is bidirectional and multiplexed (with virtual ports), the original snapcast protocol could be used without modification.

## End-User Guide: Using RIST Transport

### Quick setup (Linux)

1. Checkout the github repository with submodules:
   ```bash
   git clone --recurse-submodules https://github.com/badaix/snapcast.git
    ```
2. Go to the librist folder and build the static library:
   ```bash
   cd librist
   mkdir build && cd build
   meson --default-library=static setup ..
   meson compile
   cp include/librist/*.h ../include/librist/
   ls librist.a
   ```
   See `doc/build-librist.md` for more details.
3. Go back to the snapcast root directory and build snapserver and snapclient.
   Be sure to include `-DBUILD_WITH_LIBRIST=ON`. For example:
   ```bash
   mkdir build && cd build
   cmake .. -D CMAKE_BUILD_TYPE=Debug -DBUILD_CLIENT=ON -DBUILD_SERVER=ON -DBUILD_WITH_PULSE=ON \
      -DBUILD_WITH_JACK=ON -DBUILD_WITH_PIPEWIRE=ON\
      -DBUILD_WITH_LIBRIST=ON -DCMAKE_INSTALL_FULL_SYSCONFDIR=/etc;
   ```
4. `snapserver` and `snapclient` binaries will be build with libRIST support
   and are static linked to the library.

### Quick Start

**1. Server Configuration** (`snapserver.conf`):
```ini
[rist]
enabled = true
bind_to_address = 192.168.1.100  # Your server IP
port = 1706                      # Base port (uses 1706 and 1708)
```

**2. Start Server**:
```bash
snapserver -c snapserver.conf
```

**3. Connect Client**:
```bash
snapclient --rist-latency 100 rist://192.168.1.100:1706
```

### URL Format and Parameters

#### Client Connection URL
```
rist://[server_address]:[port][?parameter=value&...]
```

**Examples**:
```bash
# Basic connection
snapclient rist://192.168.1.100:1706

# With RIST-specific latency compensation
snapclient --rist-latency 100 rist://192.168.1.100:1706

# With custom RIST recovery parameters
snapclient rist://server:1706?recovery_length_min=40&recovery_length_max=60

# Combined with audio player settings
snapclient --rist-latency 100 --player pipewire -s 'alsa_output.pci-0000_01_00.1.hdmi-stereo' rist://192.168.1.100:1706
```

#### Stream Source URL Parameters

For advanced users, stream sources can include RIST parameters:

```bash
# Example: PipeWire source with custom RIST recovery settings
snapserver -s 'pipewire://?name=pw-sink&recovery_length_min=40&recovery_length_max=60'
```

**Available RIST URL Parameters**:
- `recovery_length_min=N` - Minimum recovery window in ms (default: 20)
- `recovery_length_max=N` - Maximum recovery window in ms (default: 50)  
- Standard stream parameters (`name`, `codec`, `chunk_ms`, `sampleformat`)

### Command Line Options

#### snapclient RIST Options

```bash
snapclient [OPTIONS] rist://server:port
```

**RIST-Specific Options**:
- `--rist-latency N` - Add N milliseconds latency compensation for RIST transport (recommended: 100)

**Example**:
```bash
# Recommended RIST client command
snapclient --rist-latency 100 --player pipewire rist://192.168.1.100:1706
```

**Why `--rist-latency 100`?**
- RIST transport adds ~100ms network latency vs TCP
- Without compensation, audio chunks arrive "too old" and get dropped
- This flag makes chunks appear "younger" to Snapcast's timing system
- Essential for proper RIST audio playback

### Network Requirements

**Firewall Configuration**:
- Server needs **2 ports open**: base port (1706) and base+2 (1708)
- Client connects to both server ports automatically
- UDP protocol (RIST uses UDP with reliability layer)

**Port Usage**:
- **Port 1706**: Audio and control data (server → client)
- **Port 1708**: Backchannel data (client → server)

### Troubleshooting

#### Common Issues

1. **No Audio / Silence**:
   ```bash
   # Solution: Add RIST latency compensation
   snapclient --rist-latency 100 rist://server:1706
   ```

2. **Connection Timeout**:
   - Check firewall allows UDP ports 1706 and 1708
   - Verify server IP address and port
   - Ensure RIST is enabled in server config

3. **Audio Dropouts**:
   - Increase recovery parameters in stream URL
   - Check network stability and bandwidth
   - Monitor server logs for buffer underruns

#### Debugging Commands

**Check RIST Status**:
```bash
# Server logs
tail -f snapserver.log | grep -i rist

# Client logs with full debug
snapclient --rist-latency 100 --logsink stdout --logfilter *:debug rist://server:1706
```

**Test Connectivity**:
```bash
# Verify server is listening on RIST ports
ss -ulnp | grep -E '1706|1708'

# Test UDP connectivity to server
nc -u server 1706
```

### Performance Tuning

#### RIST Recovery Parameters

```bash
# Conservative (higher latency, more reliable)
snapclient rist://server:1706?recovery_length_min=50&recovery_length_max=100

# Aggressive (lower latency, less recovery)
snapclient rist://server:1706?recovery_length_min=20&recovery_length_max=40
```

#### Zero-Copy Optimization

Snapcast automatically uses zero-copy optimization for audio data:
- **Audio chunks** (>90% of data): Direct memory access, no copying
- **Control messages**: Safe memory copying for stability
- **Verification**: Look for 🚀 **ZERO-COPY** messages in debug logs

## Configuration

### Network Architecture

**RIST Port Pair System**: RIST requires **2 ports per endpoint** for bidirectional communication:

**Server (binds to both ports)**:
- **Port N (1706)**: Audio/Control sender - sends data TO clients
- **Port N+2 (1708)**: Backchannel receiver - receives data FROM clients

**Client (connects to both ports)**:
- **Port N (1706)**: Audio/Control receiver - receives data FROM server  
- **Port N+2 (1708)**: Backchannel sender - sends data TO server

**Same-host testing requires 2 ports total**: 1706 and 1708
- Server uses: 1706 (bind), 1708 (bind)
- Client uses: 1706 (connect), 1708 (connect)

### Server Configuration (`snapserver.conf`)

```ini
[rist]
enabled = true
bind_to_address = 192.168.10.139  # Server binds to this address
port = 1706                        # Base port (server uses 1706 and 1708)
```

### Client Configuration

**Critical**: RIST introduces ~100ms transport latency compared to TCP. Clients must compensate with the `--rist-latency` flag:

```bash
snapclient --rist-latency 100 rist://192.168.10.139:1706
```

**Latency Parameters**:
- `--latency X`: Reduces buffer tolerance by X ms (backward compatible, for TCP)
- `--rist-latency X`: Increases buffer tolerance by X ms (RIST-specific compensation)
- For RIST connections, use `--rist-latency 100` to compensate for transport delay

**Why this is needed**:
- RIST transport adds 60-120ms network latency vs TCP's near-zero latency
- Snapcast's Stream buffer expects chunks to arrive "just in time"
- `--rist-latency 100` makes chunks appear 100ms "younger" to prevent dropping
- Without this, all chunks are dropped as "too old" and only silence plays

**Client URL Format**: 
```bash
snapclient rist://server:1706 --rist-latency 100
```

Client automatically connects to both server ports (1706 and 1708) when RIST protocol is detected.

## RIST and libRIST

RIST (Reliable Internet Stream Transport) is an open protocol for low-latency, reliable (video) streaming over unreliable IP networks, developed by the Video Services Forum (VSF). It ensures packet recovery using ARQ (Automatic Repeat reQuest) and optional FEC (Forward Error Correction), ideal for live (video) contribution, broadcasting, and remote production. libRIST is its open-source C library implementation, enabling easy integration into applications like FFMPEG, VLC, and GStreamer for adding RIST support.

Layers: Built on RTP (Real-time Transport Protocol) over UDP (User Datagram Protocol). UDP provides connectionless, low-overhead transmission for real-time efficiency but lacks reliability; RTP adds sequencing, timestamps, and payload typing for media synchronization. RIST enhances these with error correction, optional encryption (PSK/DTLS), and features like multipath bonding, contributing to robustness, low latency (programmable), and security. No RTC (assuming WebRTC) layer; RIST is distinct but compatible with RTP-based systems.

Compared to SRT (Secure Reliable Transport): Both use UDP for low-latency (video), ARQ/FEC, encryption, and open-source libs. SRT employs UDT/UDP, focuses on simplicity, uni-directional streams; RIST uses RTP/UDP, adds multicast, bi-directional traffic, DTLS, IPv6, ST2110 support, and advanced multipath (single buffer, lower latency vs. SRT's 1+1). RIST pros: more features, standards-based (VSF); cons: complexity. SRT pros: easier for basic use; cons: fewer advanced options.

## Architecture

### Design Philosophy

Our RIST implementation follows a simple PoC in C 'testrist' - a direct virtual port multiplexing approach that embraces RIST's message-oriented nature rather than forcing it into TCP's connection-oriented model.

### Core Components

#### 1. RistTransport (`common/rist_transport.hpp`)

**Location**: `common/` - shared between server and client  
**Purpose**: Bidirectional RIST transport using virtual port multiplexing

```cpp
class RistTransport {
public:
    enum class Mode { SERVER, CLIENT };
    
    // Virtual ports following testrist model
    static constexpr uint16_t VPORT_AUDIO = 1000;       // Audio data
    static constexpr uint16_t VPORT_CONTROL = 2000;     // Control messages  
    static constexpr uint16_t VPORT_BACKCHANNEL = 3000; // Backchannel data
};
```

**Key Features**:
- Mode-based configuration (SERVER binds, CLIENT connects)
- Virtual port multiplexing for clean message separation
- Callback interface (`RistTransportReceiver`) decoupled from session management
- Shared implementation reduces code duplication

#### 2. Server Integration (`server/stream_server.cpp`)

**Integration Strategy**: Parallel transport system - RIST runs alongside TCP/WebSocket without interference.

```cpp
class StreamServer : public StreamMessageReceiver, public RistTransportReceiver {
    std::unique_ptr<RistTransport> rist_transport_;
};
```

**Message Flow**:
1. **Client Hello** (vport 3000) → Server
2. **ServerSettings** (vport 2000) → Client  
3. **CodecHeader** (vport 1000) → Client
4. **Audio Chunks** (vport 1000) → Client (continuous)

#### 3. Port Assignment

Following testrist pattern for compatibility:
- **Server**: Binds to port 1706 (sender) and 1708 (receiver)
- **Client**: Connects to server port 1706 (receives) and 1708 (sends)
- **Virtual Ports**: 1000 (audio), 2000 (control), 3000 (backchannel)

## Why This Approach?

### The Problem with Session-Based RIST

Our initial implementation attempted to force RIST into Snapcast's existing session-per-client model, which led to fundamental issues:

1. **Empty `clientId` Problem**: RIST sessions were created before client connection (server startup), unlike TCP sessions created after connection
2. **Session Lifecycle Mismatch**: RIST's connectionless nature conflicted with session-based state management  
3. **Complex Message Routing**: Trying to make RIST behave like TCP connections created unnecessary complexity

### The testrist Insight

The breakthrough came from analyzing our working `testrist` implementation (120 lines vs 800+ in the complex approach):

- **Simple and Direct**: No session management complexity
- **Follows RIST Design**: Embraces message-oriented, virtual port multiplexing
- **Proven Pattern**: Server binds, client connects, virtual ports handle routing

### Benefits of Current Approach

#### 1. **Clean Architecture**
- **Zero Impact on TCP/WebSocket**: Completely separate transport path preserves existing functionality
- **Shared Code**: Common implementation between server and client reduces duplication
- **Clear Separation**: Virtual ports provide clean boundaries between message types

#### 2. **RIST-Native Design**
- **Message-Oriented**: Works with RIST's natural message flow instead of fighting it
- **Virtual Port Multiplexing**: Uses RIST's built-in routing capabilities
- **Direct Communication**: No artificial session abstraction layer

#### 3. **Maintainability**
- **Simpler Codebase**: Fewer abstractions means easier debugging and modification
- **Protocol Alignment**: Code structure matches RIST protocol design
- **Future-Proof**: Easy to extend with additional RIST features

## Implementation Details

### Server Side

```cpp
// Initialization
rist_transport_ = std::make_unique<RistTransport>(RistTransport::Mode::SERVER, this);
rist_transport_->configureServer(address, port);
rist_transport_->start();

// Audio streaming (parallel to TCP sessions)
if (rist_transport_ && isDefaultStream) {
    rist_transport_->sendAudioChunk(chunk);
}

// Client connection handling
void StreamServer::onRistClientConnected(const std::string& clientId) {
    // Send ServerSettings on vport 2000
    // Send CodecHeader on vport 1000  
}
```

### Client Side

**Architecture**: The client uses `ClientConnectionRistBidirectional` which implements the `RistTransportReceiver` interface, following the same direct communication pattern as the server.

```cpp
class ClientConnectionRistBidirectional : public ClientConnection, public RistTransportReceiver {
    std::unique_ptr<RistTransport> rist_transport_;
};
```

**Key Implementation Details**:

1. **Connection Establishment**:
   ```cpp
   // Client connects to server's RIST ports
   rist_transport_ = std::make_unique<RistTransport>(RistTransport::Mode::CLIENT, this);
   rist_transport_->configureClient(server_address, server_port);  // connects to 1706/1708
   rist_transport_->start();
   ```

2. **Message Sending** (Fixed Implementation):
   ```cpp
   void write(boost::asio::streambuf& buffer, WriteHandler&& write_handler) {
       // Send raw serialized data directly (no re-serialization)
       const auto* data_ptr = boost::asio::buffer_cast<const char*>(buffer.data());
       size_t data_size = buffer.size();
       
       // Use sendRawData to avoid JSON parse errors
       bool success = rist_transport_->sendRawData(RistTransport::VPORT_BACKCHANNEL, data_ptr, data_size);
   }
   ```

3. **Message Receiving**:
   ```cpp
   void onRistMessageReceived(const msg::BaseMessage& baseMessage, 
                             const std::string& payload, uint16_t vport) {
       // Handle ServerSettings, CodecHeader, Time responses
       auto message = msg::factory::createMessage(baseMessage, const_cast<char*>(payload.data()));
       messageReceived(std::move(message), handler); // Use base ClientConnection correlation
   }
   ```

**Critical Fix - sendRawData() Method**:
The major breakthrough was implementing `RistTransport::sendRawData()` to send pre-serialized message data directly, avoiding the JSON parse errors that occurred when trying to deserialize and re-serialize messages in the client's `write()` method.

**Protocol Flow**:
1. **Hello Request**: Client sends Hello via vport 3000 (backchannel)
2. **ServerSettings Response**: Server responds via vport 2000 (control)  
3. **CodecHeader**: Server sends via vport 1000 (audio)
4. **Audio Streaming**: Continuous PcmChunks via vport 1000

### Message Protocol

1. **Hello Handshake**:
   ```
   Client → Server (vport 3000): Hello{clientId}
   Server → Client (vport 2000): ServerSettings  
   Server → Client (vport 1000): CodecHeader
   ```

2. **Audio Streaming**:
   ```
   Server → Client (vport 1000): PcmChunk (continuous)
   ```

3. **Control Messages**:
   ```
   Client ↔ Server (vport 3000): Time, Status, etc.
   ```

## Alternatives Considered

### 1. Session-Based Approach (Initial Implementation)

**What we tried**:
- `StreamSessionRistBidirectional` inheriting from `StreamSession`
- Session-per-client model matching TCP implementation
- Complex session lifecycle management

**Why it failed**:
- RIST sessions created before client connection → empty `clientId`
- Session pointer lifecycle issues preventing Hello response
- Buffer format mismatches between protocols
- Fighting RIST's connectionless nature

### 2. Hybrid Session Approach

**What we tried**:
- Keep session interface but simplify RIST handling
- Session created on-demand during Hello processing

**Why we moved away**:
- Still required complex session management
- Didn't align with RIST's natural message flow
- Added unnecessary abstraction layer

### 3. Direct Port Communication (Current)

**Why it works**:
- Embraces RIST's message-oriented design
- Simple, direct communication pattern
- No session management complexity
- Matches proven testrist pattern

## Debugging and Monitoring

### Key Log Messages

**Server Startup**:
```
[Info] (RistTransport) Starting RIST transport in SERVER mode
[Info] (RistTransport) Virtual ports: 1000 (audio), 2000 (control), 3000 (backchannel)
```

**Client Connection**:
```
[Info] (RistTransport) RIST Hello received from client: {clientId}
[Info] (StreamServer) Sent ServerSettings to RIST client: {clientId}  
[Info] (StreamServer) Sent CodecHeader ({size} bytes) to RIST client: {clientId}
```

**Audio Streaming**:
```
[Debug] (RistTransport) Sent message type ServerSettings (83 bytes) on vport 2000
[Debug] (RistTransport) Sent message type CodecHeader (1374 bytes) on vport 1000
```

### Common Issues

1. **Empty CodecHeader**: Ensure audio stream is active before client connects
2. **Connection Timeouts**: Check firewall settings for ports 1706/1708
3. **Audio Gaps**: Monitor RIST buffer settings and network conditions

## Technical Implementation

### Zero-Copy Audio Optimization 🚀

**Performance Critical**: Snapcast implements zero-copy optimization for audio data, eliminating memory copying for >90% of transmitted data.

**How It Works**:
1. **Large Audio Chunks** (>100 bytes): Direct memory pointer access, no copying
2. **Small Control Messages** (<100 bytes): Safe memory copying for stability
3. **Automatic Detection**: System automatically chooses optimal path based on message size

**Performance Benefits**:
- **Memory Bandwidth**: Massive savings for audio streaming workloads
- **CPU Efficiency**: Reduces processing overhead for audio data
- **Latency**: Eliminates copy-related delays in audio path
- **Stability**: Control messages maintain memory safety through copying

**Verification**: Enable debug logging to see zero-copy in action:
```bash
snapclient --rist-latency 100 --logfilter *:debug rist://server:1706 | grep -E "🚀|📋"
```

Look for:
- 🚀 **ZERO-COPY**: Audio chunk processing (no memory copy)
- 📋 **COPY**: Control message processing (safe copying)

### Stream Parameter Propagation

**Dynamic Configuration**: RIST parameters can be specified in stream URLs and are automatically propagated to clients:

```cpp
// Server extracts parameters from stream URL
std::pair<uint32_t, uint32_t> StreamServer::getRistParameters() const {
    if (active_pcm_stream_) {
        const auto& uri = active_pcm_stream_->getUri();
        std::string min_str = uri.getQuery("recovery_length_min");
        std::string max_str = uri.getQuery("recovery_length_max");
        // ... parameter extraction and validation
    }
    return {settings_.rist.recovery_length_min, settings_.rist.recovery_length_max};
}
```

**Parameter Flow**:
1. Stream URL: `pipewire://?recovery_length_min=40&recovery_length_max=60`
2. Server extracts and validates parameters
3. Parameters sent to clients via ServerSettings messages
4. Clients update RIST transport with received parameters

## Performance Considerations

### Buffer Configuration

Optimized parameters for low-latency audio streaming:
```cpp
config->recovery_length_min = recovery_length_min;     // 20ms (down from 200ms)
config->recovery_length_max = recovery_length_max;     // 50ms (down from 200ms) 
config->recovery_rtt_min = recovery_rtt_min;           // 5ms
config->recovery_rtt_max = recovery_rtt_max;           // 50ms (down from 500ms)
config->recovery_reorder_buffer = recovery_reorder_buffer; // 15ms
config->min_retries = min_retries;                     // 3 (down from 6)
config->max_retries = max_retries;                     // 10 (down from 20)
```

These constants are defined in `common/rist_transport.hpp` and significantly reduce RIST's buffer latency while maintaining reliability for LAN/WAN audio streaming.

### Network Efficiency

- **Virtual Port Multiplexing**: Reduces connection overhead
- **Direct Message Routing**: Minimal processing overhead
- **Zero-Copy Audio**: Eliminates memory copying for audio streams
- **Optimized Logging**: Reduced spam from high-frequency audio chunks

## Request/Response Correlation Fix ✅

### Problem: Client Timeout/Reconnection Cycles

**Symptom**: RIST clients experienced recurring 10-second timeout/reconnection cycles, even though audio streaming worked correctly.

**Root Cause**: The RIST client's `sendRequest` implementation was not assigning proper message IDs, causing request/response correlation to fail.

### Technical Details

#### The Issue
```cpp
// RIST client sendRequest (before fix)
void ClientConnectionRistBidirectional::sendRequest(const msg::message_ptr& message, ...)
{
    // ❌ Using original message ID (often 0 for Hello messages)
    auto request = std::make_shared<PendingRequest>(strand_, message->id, handler);
    // Client sends Hello with id=0 via VPORT_BACKCHANNEL
}
```

**Correlation Failure Chain**:
1. Client sends Hello request with `id=0` via VPORT_BACKCHANNEL
2. Server receives Hello and sends ServerSettings response with `refersTo=0` via VPORT_CONTROL
3. Client receives ServerSettings but correlation check fails: `if (message->refersTo != 0)`
4. Client times out waiting for Hello response → disconnects → reconnects

#### The Solution
```cpp
// RIST client sendRequest (after fix)
void ClientConnectionRistBidirectional::sendRequest(const msg::message_ptr& message, ...)
{
    // ✅ Assign proper request ID (copied from base ClientConnection::sendRequest)
    static constexpr uint16_t max_req_id = 10000;
    if (++reqId_ >= max_req_id)
        reqId_ = 1;
    message->id = reqId_;
    
    auto request = std::make_shared<PendingRequest>(strand_, message->id, handler);
    // Client now sends Hello with proper non-zero ID (1, 2, 3...)
}
```

#### Why Virtual sendRequest Was Necessary

The fix also required making the base `ClientConnection::sendRequest()` method virtual:

```cpp
// client_connection.hpp
virtual void sendRequest(const msg::message_ptr& message, const chronos::usec& timeout, 
                        const MessageHandler<msg::BaseMessage>& handler);

// client_connection_rist_bidirectional.hpp  
void sendRequest(const msg::message_ptr& message, const chronos::usec& timeout,
                const MessageHandler<msg::BaseMessage>& handler) override;
```

Without the virtual override, RIST requests would attempt to use TCP transport instead of routing through the RIST backchannel.

### Files Modified

1. **`client/client_connection.hpp`**: Made `sendRequest` virtual (line 131)
2. **`client/client_connection_rist_bidirectional.hpp`**: Added override and correlation members
3. **`client/client_connection_rist_bidirectional.cpp`**: Implemented proper ID assignment and correlation logic
4. **`server/stream_server.cpp`**: Uncommented Time response code (lines 446-450)

### Result

- **✅ Stable connections**: No more 10-second reconnection cycles
- **✅ Perfect Time sync**: Regular Time responses every ~1 second via backchannel
- **✅ Proper correlation**: Hello/Time requests get non-zero IDs and correlate correctly
- **✅ Production ready**: RIST bidirectional communication working reliably

### Key Insights

1. **Message ID assignment is critical** for request/response correlation across all transports
2. **Virtual method dispatch** is necessary for protocol-specific routing in polymorphic designs
3. **RIST virtual ports are unidirectional**: 
   - VPORT_BACKCHANNEL (3000): Client → Server
   - VPORT_CONTROL (2000): Server → Client  
   - VPORT_AUDIO (1000): Server → Client
4. **Systematic debugging** through logs revealed the exact correlation failure point

The lesson: Always ensure message ID management is consistent across all transport implementations, especially when implementing protocol-specific overrides.

## Future Enhancements

### Potential Improvements

1. **Multi-Stream Support**: Extend virtual port assignments for multiple audio streams
2. **Advanced RIST Features**: Implement encryption, authentication, and advanced recovery
3. **Dynamic Configuration**: Runtime RIST parameter adjustment
4. **Monitoring Integration**: RIST-specific metrics and health monitoring

## Conclusion

The current RIST implementation represents a significant architectural improvement over initial attempts. By embracing RIST's native design principles and following the proven testrist pattern, we've created a clean, maintainable, and extensible foundation for reliable internet streaming in Snapcast.

The key lesson learned: **don't fight the protocol's nature** - embrace RIST's strengths rather than trying to make it behave like TCP. This alignment between code structure and protocol design results in simpler, more reliable software.

## Acknowledgements

Research for this implementation was done with perplexity AI and grok AI. Most of the inital code was written by Claude Code AI, including this documentation.

However, all tests, prompt directions, and the initial PR were done by [aanno](https://github.com/aanno).
