/***
    This file is part of snapcast
    Copyright (C) 2025  aanno <aannoaanno@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#pragma once

// local headers
#include "stream_session_tcp.hpp"

// 3rd party headers
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <linux/errqueue.h>

using boost::asio::ip::tcp;

/// Coordinated TCP session with zerocopy when async operations are idle
/**
 * This session extends the regular TCP session with coordinated zerocopy capability.
 * - Uses regular Boost.Asio async_write when there are pending operations
 * - Uses direct MSG_ZEROCOPY sendmsg() only when socket is idle from Boost.Asio perspective
 * - Maintains strict coordination to prevent mixing async and direct operations
 * - Provides graceful fallback when zerocopy is not available
 */
class StreamSessionTcpCoordinated : public StreamSessionTcp
{
public:
    StreamSessionTcpCoordinated(StreamMessageReceiver* receiver, const ServerSettings& server_settings, tcp::socket&& socket);
    ~StreamSessionTcpCoordinated() override;

    void start() override;
    void stop() override;

    /// Get zerocopy statistics for this session
    struct ZeroCopyStats
    {
        uint64_t zerocopy_attempts{0};      // Total zerocopy send attempts
        uint64_t zerocopy_successful{0};    // Successful zerocopy sends
        uint64_t zerocopy_bytes{0};         // Total bytes sent via zerocopy
        uint64_t regular_sends{0};          // Messages sent via regular async_write
        uint64_t regular_bytes{0};          // Total bytes sent via regular async_write
        uint64_t coordination_fallbacks{0}; // Fallbacks due to pending async ops
        uint64_t pending_async_operations{0}; // Currently pending async_write operations
        uint64_t outstanding_zerocopy_buffers{0}; // Buffers awaiting completion notifications
        uint64_t completion_notifications_received{0}; // Completion notifications received
        uint64_t completion_notifications_missing{0}; // Expected but missing notifications
        uint64_t buffers_completed_via_notifications{0}; // Total buffers completed via notifications
        uint64_t buffer_reuse_count{0}; // How many times buffers were reused
        double zerocopy_percentage() const 
        { 
            return (zerocopy_attempts + regular_sends) > 0 ? 
                   (double(zerocopy_successful) / double(zerocopy_attempts + regular_sends)) * 100.0 : 0.0; 
        }
        double completion_reliability() const 
        {
            return zerocopy_successful > 0 ? 
                   (double(buffers_completed_via_notifications) / double(zerocopy_successful)) * 100.0 : 0.0;
        }
    };
    
    ZeroCopyStats getZeroCopyStats() const;
    void resetZeroCopyStats();

protected:
    void sendAsync(const std::shared_ptr<shared_const_buffer> buffer, WriteHandler&& handler) override;

private:
    /// Initialize zerocopy capability
    bool initializeZeroCopy();
    
    /// Check if we can safely use zerocopy (no pending async operations)
    bool canUseZeroCopy() const;
    
    /// Send using zerocopy (only when socket is idle)
    void sendZeroCopy(const std::shared_ptr<shared_const_buffer> buffer, WriteHandler&& handler);
    
    /// Send using regular async_write (coordinated with async operations)
    void sendRegularCoordinated(const std::shared_ptr<shared_const_buffer> buffer, WriteHandler&& handler);
    
    /// Process pending send queue
    void processPendingSends();
    
    /// Try to reserve zerocopy access (thread-safe)
    bool tryReserveZeroCopy();
    
    /// Release zerocopy reservation
    void releaseZeroCopy();
    
    /// Error queue monitoring for zerocopy completions - thread-based
    void startErrorQueueMonitoring();
    void stopErrorQueueMonitoring();
    void errorQueueMonitoringLoop();
    void processErrorQueue();
    
    /// Simple buffer tracking for zerocopy completion
    /// Maps buffer_id to the shared_ptr that keeps the buffer alive
    /// When completion notification arrives, we remove the entry and let shared_ptr handle cleanup
    
    /// Pending send operation
    struct PendingSend
    {
        std::shared_ptr<shared_const_buffer> buffer;
        WriteHandler handler;
        size_t size;
        bool use_zerocopy;
    };
    
    // Configuration
    static constexpr size_t ZEROCOPY_THRESHOLD = 1024;  // Use zerocopy for messages >1KB
    
    // Zerocopy state
    bool zerocopy_available_{false};
    int native_socket_{-1};
    std::atomic<uint32_t> next_buffer_id_{0};
    
    // Coordination state
    std::atomic<uint32_t> pending_async_operations_{0};
    std::queue<PendingSend> pending_sends_;
    std::mutex pending_sends_mutex_;
    std::atomic<bool> processing_queue_{false};
    
    // Statistics (thread-safe)
    mutable std::atomic<uint64_t> zerocopy_attempts_{0};
    mutable std::atomic<uint64_t> zerocopy_successful_{0};
    mutable std::atomic<uint64_t> zerocopy_bytes_{0};
    mutable std::atomic<uint64_t> regular_sends_{0};
    mutable std::atomic<uint64_t> regular_bytes_{0};
    mutable std::atomic<uint64_t> coordination_fallbacks_{0};
    mutable std::atomic<uint64_t> outstanding_zerocopy_buffers_{0};
    mutable std::atomic<uint64_t> completion_notifications_received_{0};
    mutable std::atomic<uint64_t> completion_notifications_missing_{0};
    mutable std::atomic<uint64_t> buffers_completed_via_notifications_{0};
    
    // Error queue monitoring - dedicated thread approach
    std::unique_ptr<std::thread> error_queue_thread_;
    std::atomic<bool> monitoring_active_{false};
    std::atomic<bool> shutdown_requested_{false};
    
    // Buffer tracking - maps buffer_id to shared_ptr for completion handling
    std::unordered_map<uint32_t, std::shared_ptr<shared_const_buffer>> pending_zerocopy_buffers_;
    std::mutex zerocopy_buffers_mutex_;
};
