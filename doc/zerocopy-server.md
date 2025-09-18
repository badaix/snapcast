# Snapcast Server Zerocopy Implementation

This document describes the MSG_ZEROCOPY implementation for Snapcast server networking, providing enhanced performance through kernel-level zero-copy transmission.

## Overview

The zerocopy implementation reduces CPU usage and memory bandwidth by allowing the kernel to send data directly from application buffers without additional copying. This is particularly beneficial for streaming large PCM audio chunks to multiple clients.

## Configuration

### Command Line Option

Enable zerocopy networking with the `-z` flag:

```bash
./bin/snapserver -z -c snapserver.conf
```

The `-z` flag is a simple boolean switch that enables zerocopy for all client connections.

### Configuration File

Zerocopy can also be configured in the config file (e.g., `snapserver.conf`):

```ini
[stream]
# Enable zerocopy networking for improved performance
zerocopy = true
```

**Note**: The command line `-z` flag overrides the config file setting when present.

### Usage Examples

```bash
# Enable via command line
./bin/snapserver -z

# Enable via config file
echo "zerocopy = true" >> /etc/snapserver.conf
./bin/snapserver

# Command line overrides config file
./bin/snapserver -z -c snapserver.conf  # zerocopy enabled
```

## Implementation Details

### Critical Architecture Change: Buffer Lifetime Management

A fundamental change was required for zerocopy: **changing the `sendAsync()` signature from**:

```cpp
// BEFORE: Reference to buffer (dangerous for zerocopy)
virtual void sendAsync(const shared_const_buffer& buffer, WriteHandler&& handler) = 0;

// AFTER: Shared pointer to buffer (safe for zerocopy)  
virtual void sendAsync(const std::shared_ptr<shared_const_buffer> buffer, WriteHandler&& handler) = 0;
```

#### Why This Change Was Essential

**The Zerocopy Buffer Lifetime Problem**:
1. **Zerocopy operations are asynchronous** - the kernel may send data long after `sendmsg()` returns
2. **Kernel needs stable buffers** - the buffer must remain valid until the kernel completes transmission
3. **Reference semantics are dangerous** - passing `const shared_const_buffer&` means the buffer could be destroyed while kernel is still using it
4. **Stack unwinding risk** - if the calling function returns, stack-allocated buffers become invalid

**The Solution - Shared Pointer Semantics**:
1. **Guaranteed Lifetime** - `std::shared_ptr<shared_const_buffer>` ensures buffer lives until last reference is released
2. **Reference Counting** - Buffer automatically freed only when both application and kernel are done with it
3. **Zerocopy Safety** - Kernel completion notifications can safely decrement reference count
4. **RAII Cleanup** - No manual memory management required

#### Implementation in sendNext()

The key change is in `StreamSession::sendNext()` at server/stream_session.cpp:55:

```cpp
void StreamSession::sendNext()
{
    auto& buffer = messages_.front();  // Reference to buffer in deque
    buffer.on_air = true;
    boost::asio::post(strand_, [this, self = shared_from_this(), buffer]()
    {
        // CRITICAL: Convert to shared_ptr for safe zerocopy buffer lifetime
        auto buffer_ptr = std::make_shared<shared_const_buffer>(buffer);
        
        // Pass shared_ptr to sendAsync - buffer now guaranteed to live 
        // until kernel completion notification
        sendAsync(buffer_ptr, [this, buffer_ptr](boost::system::error_code ec, std::size_t length)
        {
            // buffer_ptr keeps buffer alive during this callback
            auto write_handler = buffer_ptr->getWriteHandler();
            if (write_handler)
                write_handler(ec, length);
            // buffer_ptr goes out of scope here - may trigger cleanup
        });
        // buffer_ptr may still be held by zerocopy system
    });
}
```

#### Multi-Client Buffer Sharing

This change also enables **efficient buffer sharing across multiple clients**:

1. **Same Audio Chunk, Multiple Clients** - When the same PCM chunk needs to go to multiple clients
2. **Single Buffer Creation** - `shared_const_buffer` created once with pooled memory
3. **Reference Counting** - Each client session gets its own `std::shared_ptr` to the same buffer
4. **Automatic Cleanup** - Buffer freed only when all clients have completed transmission
5. **Memory Efficiency** - One buffer copy instead of N copies for N clients

#### Buffer Pool Integration

The `shared_const_buffer` class has been enhanced with buffer pool integration:

```cpp
struct Message
{
    DynamicBufferPool::BufferGuard buffer_guard; ///< pooled buffer for data
    size_t data_size;                             ///< actual size of data in buffer  
    // ... other fields
};
```

**Benefits**:
- **Reduced Allocations** - Reuses pooled memory instead of malloc/free per message
- **Size Bucketing** - Efficient memory management for different message sizes
- **RAII Cleanup** - BufferGuard automatically returns buffer to pool
- **Thread Safety** - Pool operations are thread-safe for multi-client scenarios

### Architecture: Coordinated Approach

Our implementation uses a **coordinated approach** that safely combines Boost.Asio async operations with direct MSG_ZEROCOPY syscalls. This design choice was made to:

1. **Preserve Boost.Asio Benefits**: Keep all existing async patterns, error handling, and event loop integration
2. **Avoid Complete Rewrite**: Minimize changes to existing codebase  
3. **Ensure Safety**: Prevent race conditions between async and direct socket operations
4. **Provide Graceful Fallback**: Automatic fallback when zerocopy is unavailable or inappropriate

### Core Components

**StreamSessionTcpCoordinated** (`server/stream_session_tcp_coordinated.hpp/cpp`)
- Enhanced TCP session that coordinates zerocopy with Boost.Asio async operations
- Uses direct `sendmsg()` with MSG_ZEROCOPY when socket is idle from async operations
- Falls back to regular `boost::asio::async_write()` when async operations are pending
- Provides comprehensive diagnostics and statistics

**Stream Server Integration** (`server/stream_server.cpp`)
- Creates coordinated sessions when zerocopy is enabled
- Provides periodic diagnostics reporting every 30 seconds
- Logs session creation: `"Creating zerocopy-enabled session"` vs `"Creating regular TCP session"`

### Technical Implementation

#### Coordination Mechanism
- **Atomic Counter**: `pending_async_operations_` tracks active Boost.Asio operations
- **Safe Zerocopy**: Only uses MSG_ZEROCOPY when counter is 0 (socket idle)  
- **Mutual Exclusion**: Regular async operations increment counter, blocking zerocopy
- **Race Prevention**: Atomic compare-and-swap prevents race conditions

#### Operation Flow
1. **Large Messages (≥1024 bytes)**: Try zerocopy first, fallback to async if socket busy
2. **Small Messages (<1024 bytes)**: Use regular async operations (more efficient)
3. **Busy Socket**: Queue operations and process when socket becomes idle
4. **Error Handling**: Monitor kernel error queue for zerocopy completion notifications

## Race Condition Avoidance

### The Challenge
Mixing Boost.Asio async operations with direct syscalls creates potential race conditions:
```cpp
// DANGEROUS: Check-then-act race condition
if (no_async_operations_pending) {  // ✓ Check passes
    // Another thread could start async_write here!
    sendmsg(socket, MSG_ZEROCOPY);  // ✗ Now mixing operations!
}
```

### Our Solution: Atomic Reservation

**1. Atomic Compare-and-Swap Pattern**
```cpp
bool StreamSessionTcpCoordinated::tryReserveZeroCopy()
{
    uint32_t expected = 0;
    while (!pending_async_operations_.compare_exchange_weak(expected, 1))
    {
        if (expected != 0)
            return false; // Another operation is in progress
        // Retry on spurious failures (expected reloaded with current value)
    }
    return true; // Successfully reserved zerocopy
}
```

**2. Coordination Points**
- **Regular Async Operations**: 
  - Increment counter before starting: `pending_async_operations_++`
  - Decrement in completion handler: `pending_async_operations_--`
- **Zerocopy Operations**:
  - Reserve atomically: `tryReserveZeroCopy()` (sets to 1)  
  - Release when done: `releaseZeroCopy()` (sets to 0)

**3. Race Prevention Guarantees**
- **Atomic Operations**: All counter access uses atomic operations
- **No Check-Then-Act**: The compare-and-swap is atomic, no gap for races
- **Spurious Failure Handling**: Retry loop handles `compare_exchange_weak` spurious failures
- **Mutual Exclusion**: Both systems use same counter, ensuring exclusive access

### Why Compare-and-Swap Works

The atomic `compare_exchange_weak()` operation:
1. **Atomically** checks if `pending_async_operations_ == 0` (idle)
2. **Atomically** sets it to `1` (reserved) if idle
3. **Returns false** if another operation started between check and set
4. **Handles spurious failures** by retrying in a loop

This eliminates the race window completely - there's no gap between checking and acting.

## Alternative Approaches Considered

### 1. Pure Boost.Asio Zerocopy
- **Pros**: Clean integration, no race conditions
- **Cons**: Boost.Asio doesn't natively support MSG_ZEROCOPY, would require extensive framework modifications

### 2. Separate Zerocopy Socket Class  
- **Pros**: Complete control over zerocopy operations
- **Cons**: Duplicate async infrastructure, more complex error handling, larger codebase changes

### 3. Strand-Based Serialization
- **Pros**: Boost.Asio strand ensures serialized access
- **Cons**: All operations must go through strand, potential performance impact

### 4. Mutex-Based Locking
- **Pros**: Simple exclusive access
- **Cons**: Blocking operations, potential latency impact, doesn't align with async patterns

**Our Choice**: The coordinated approach provides the best balance of safety, performance, and integration simplicity.

## Comprehensive Diagnostics System

### Real-time Statistics Tracking

The implementation includes comprehensive statistics with atomic counters for thread safety:

- **Zerocopy Operations**: Count and bytes sent via MSG_ZEROCOPY
- **Regular Operations**: Count and bytes sent via standard async_write  
- **Coordination Fallbacks**: When zerocopy is skipped due to pending async operations
- **Outstanding Operations**: Number of zerocopy operations pending kernel completion
- **Success Rate**: Percentage of operations using zerocopy vs regular sends

### ZeroCopyStats Structure

Each coordinated session tracks detailed statistics with automatic reset after each report:

```cpp
struct ZeroCopyStats {
    uint64_t zerocopy_attempts{0};      // Total zerocopy send attempts
    uint64_t zerocopy_successful{0};    // Successful zerocopy sends  
    uint64_t zerocopy_bytes{0};         // Total bytes sent via zerocopy
    uint64_t regular_sends{0};          // Messages sent via async_write
    uint64_t regular_bytes{0};          // Total bytes sent via async_write
    uint64_t coordination_fallbacks{0}; // Fallbacks due to pending async ops
    uint64_t outstanding_operations{0}; // Currently outstanding zerocopy operations in kernel
    double zerocopy_percentage() const; // Success rate calculation
};
```

### Statistics Management

**Automatic Reset**: Statistics are reset after each periodic report to show current performance trends rather than lifetime accumulated values. This provides:
- **Current Performance**: Each 30-second report shows recent activity, not lifetime totals
- **Trend Analysis**: Easier to identify performance changes over time  
- **Memory Management Tracking**: Outstanding operations counter shows kernel buffer usage
- **Fresh Diagnostics**: Each report period starts with clean counters

**Buffer Lifecycle Tracking**: The `outstanding_operations` counter tracks zerocopy buffers:
- **Incremented**: When `sendmsg()` succeeds with MSG_ZEROCOPY
- **Decremented**: When kernel completion notifications are received via error queue
- **Purpose**: Monitor memory pressure and kernel buffer usage

### Periodic Reporting

Every 30 seconds, when clients are connected, the server logs zerocopy diagnostics then resets all counters:

```
=== Periodic ZeroCopy Status (every 30s) ===
Zerocopy Stats for session 192.168.1.100
	ZC Attempts: 1250, 
	ZC Successful: 1200, 
	ZC Bytes: 15728640, 
	Regular Sends: 45, 
	Regular Bytes: 2048, 
	Coordination Fallbacks: 5, 
	Outstanding Operations: 12,
	ZC Success Rate: 96.00%
```

**Note**: After this report, all counters except `outstanding_operations` are reset to zero. The next 30-second report will show fresh statistics from that point forward.

## Testing and Verification

### How to Test Zerocopy

1. **Start the server with zerocopy enabled**: 
   ```bash
   ./bin/snapserver -z -c snapserver.conf
   ```

2. **Connect clients**:
   ```bash
   scripts/run-snapclient.sh
   ```

3. **Monitor logs** for zerocopy diagnostics messages

### What the Logs Tell You

#### Connection Messages
- `"Creating zerocopy-enabled session for 192.168.1.100"` - Zerocopy is working
- `"Creating regular TCP session for 192.168.1.100"` - Zerocopy disabled or not working
- `"ZeroCopy setting: enabled"` - Shows zerocopy configuration status at startup

#### Performance Indicators

**High Performance (Working Well)**:
- High zerocopy success rate (>90%)
- Low coordination fallbacks
- Large byte ratios for zerocopy vs regular

**Potential Issues**:
- High coordination fallbacks - Socket frequently busy with async operations  
- Low success rate - MSG_ZEROCOPY not available or network issues
- Zero successful attempts - Kernel doesn't support zerocopy

#### Example Good Performance
```
ZC Success Rate: 98.20%
ZC Successful: 1200
Coordination Fallbacks: 5
```

#### Example Coordination Issues  
```
ZC Success Rate: 45.00%  
ZC Successful: 450
Coordination Fallbacks: 550  # High fallback rate
```

## Troubleshooting

### Zerocopy Not Available
If zerocopy is not working:

1. **Check kernel version**: MSG_ZEROCOPY requires Linux kernel 4.14+
2. **Check socket buffer limits**: May need to increase `net.core.optmem_max`
3. **Check permissions**: Some containers may restrict socket options
4. **Review startup logs**: Look for `"ZeroCopy setting: disabled"` messages

### Performance Issues

1. **High coordination fallbacks**: 
   - High async operation frequency (normal for small messages)
   - Very active client connections
   - Consider this normal behavior - the system is working as designed

2. **Low success rate with low fallbacks**:
   - Kernel zerocopy issues (EAGAIN, insufficient buffers)
   - Network congestion  
   - Check system limits and kernel messages

### Configuration Verification

To verify your configuration:

```bash
# Check command line help
./bin/snapserver -h

# Check config file options  
./bin/snapserver -hh | grep -i zerocopy
```

## Performance Benefits

When working correctly, the coordinated zerocopy approach provides:

- **Reduced CPU usage**: Eliminates memory copying in kernel for large messages
- **Lower memory bandwidth**: Direct buffer transmission
- **Better scalability**: More efficient with multiple clients  
- **Maintained Async Benefits**: Keeps all Boost.Asio advantages
- **Safe Operation**: No race conditions or undefined behavior

The diagnostics system allows real-time monitoring of these benefits and helps identify any coordination or performance issues.

## Additional Optimizations

### Buffer Pool Memory Management

The implementation includes a `DynamicBufferPool` (`common/buffer_pool.hpp/cpp`) that significantly reduces memory allocation overhead:

**Features**:
- **Size Buckets**: Powers of 2 sizing (1024, 2048, 4096, etc.) for efficient reuse
- **RAII Management**: `BufferGuard` automatically returns buffers to pool
- **Thread Safety**: Atomic statistics and mutex protection for multi-client scenarios
- **Automatic Cleanup**: Idle buffers cleaned up after 5 minutes
- **Statistics Tracking**: Pool efficiency metrics logged every 30 seconds

**Integration**: 
- `shared_const_buffer` uses pooled memory via `DynamicBufferPool::BufferGuard`
- Audio message serialization reuses buffers instead of individual malloc/free
- Significant allocation reduction in multi-client streaming scenarios

**Statistics Logged**:
```
=== Buffer Pool Stats ===
	Total Buffers: 16
	Available Buffers: 16  
	Bytes Allocated: 65536
	Buffers Created: 0
	Buffers Reused: 6088
	Cleanup Operations: 0
```

### Optimized Logging System

The logging system (`common/aixlog.hpp/cpp`) has been enhanced with caching for better performance:

**Optimization**: 
- `should_log()` results cached with hash map: `(severity, tag) -> bool`
- `LOG()` macro uses `should_log_cached()` to avoid repeated filter evaluations
- Cache automatically invalidated when sink configuration changes
- LRU-style eviction prevents memory growth

**Performance Impact**:
- Eliminates repeated filter matching for frequent log calls
- Reduces string construction overhead for filtered-out messages
- Particularly beneficial for TRACE/DEBUG logging in production

**Cache Statistics** (accessible via `getShouldLogCacheStats()`):
- Cache hits/misses for performance monitoring
- Cache size for memory usage tracking
- Automatic cache clearing on configuration changes

## Integration Notes

- **Backward Compatible**: Automatically falls back to regular async operations when zerocopy unavailable
- **Thread Safe**: All statistics and coordination use atomic operations
- **Boost.Asio Compatible**: Full integration with existing async patterns
- **Configurable**: Enable via command line (`-z`) or config file (`zerocopy = true`)
- **Memory Optimized**: Buffer pool and log caching reduce allocation overhead
- **Comprehensive Diagnostics**: Real-time monitoring of zerocopy, buffer pool, and cache performance
- **Zero Code Changes**: Existing client code unchanged, transparent enhancement

## Acknowledgements

Research for this implementation was done with perplexity AI. Most of the inital code was written by Claude Code AI, including this documentation.

However, all tests, prompt directions, and the initial PR were done by [aanno](https://github.com/aanno).
