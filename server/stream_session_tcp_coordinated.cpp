/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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

// prototype/interface header file
#include "stream_session_tcp_coordinated.hpp"

// local headers
#include "common/aixlog.hpp"

// standard headers
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <cstring>
#include <pthread.h>

// MSG_ZEROCOPY definition for compatibility with older headers
#ifndef MSG_ZEROCOPY
#define MSG_ZEROCOPY 0x4000000
#endif

static constexpr auto LOG_TAG = "StreamSessionTcpCoordinated";
static constexpr auto LOG_TAG_COMPLETION = "ZeroCopyCompletion";
static constexpr auto LOG_TAG_STATS = "ZeroCopyStats";

// Removed global buffer registry - now using per-socket shared_ptr tracking

StreamSessionTcpCoordinated::StreamSessionTcpCoordinated(StreamMessageReceiver* receiver, const ServerSettings& server_settings, tcp::socket&& socket)
    : StreamSessionTcp(receiver, server_settings, std::move(socket))
{
    native_socket_ = socket_.native_handle();
    LOG(DEBUG, LOG_TAG) << "Native socket handle: " << native_socket_ << "\n";
    zerocopy_available_ = initializeZeroCopy();
    
    if (zerocopy_available_)
    {
        LOG(INFO, LOG_TAG) << "Coordinated ZeroCopy enabled for session " << getIP() << "\n";
    }
    else
    {
        LOG(INFO, LOG_TAG) << "ZeroCopy not available for session " << getIP() << ", using regular TCP\n";
    }
}

StreamSessionTcpCoordinated::~StreamSessionTcpCoordinated()
{
    stop();
    
    // Log final statistics
    if (zerocopy_available_)
    {
        auto stats = getZeroCopyStats();
        LOG(INFO, LOG_TAG) << "Session " << getIP() << " final stats - ZC: " << stats.zerocopy_successful 
                           << "/" << stats.zerocopy_attempts << " (" << stats.zerocopy_percentage() << "%), "
                           << "Regular: " << stats.regular_sends << ", Fallbacks: " << stats.coordination_fallbacks << "\n";
    }
}

void StreamSessionTcpCoordinated::start()
{
    StreamSessionTcp::start();
    
    if (zerocopy_available_)
    {
        startErrorQueueMonitoring();
    }
}

void StreamSessionTcpCoordinated::stop()
{
    if (zerocopy_available_)
    {
        stopErrorQueueMonitoring();
        
        // Clear any pending sends
        std::lock_guard<std::mutex> lock(pending_sends_mutex_);
        while (!pending_sends_.empty())
        {
            auto& pending = pending_sends_.front();
            if (pending.handler)
            {
                pending.handler(boost::asio::error::operation_aborted, 0);
            }
            pending_sends_.pop();
        }
    }
    
    StreamSessionTcp::stop();


}

bool StreamSessionTcpCoordinated::initializeZeroCopy()
{
    // LOG(DEBUG, LOG_TAG) << "initializeZeroCopy called with native_socket_: " << native_socket_ << "\n";
    if (native_socket_ < 0)
    {
        LOG(WARNING, LOG_TAG) << "Invalid native socket handle: " << native_socket_ << "\n";
        return false;
    }
    
    // Enable zerocopy on socket
    int enable = 1;
    if (setsockopt(native_socket_, SOL_SOCKET, SO_ZEROCOPY, &enable, sizeof(enable)) < 0)
    {
        LOG(WARNING, LOG_TAG) << "Failed to enable SO_ZEROCOPY: " << strerror(errno) << "\n";
        return false;
    }
    
    // Verify zerocopy is enabled
    int enabled = 0;
    socklen_t len = sizeof(enabled);
    if (getsockopt(native_socket_, SOL_SOCKET, SO_ZEROCOPY, &enabled, &len) < 0 || !enabled)
    {
        LOG(WARNING, LOG_TAG) << "SO_ZEROCOPY verification failed\n";
        return false;
    }
    
    LOG(DEBUG, LOG_TAG) << "Coordinated ZeroCopy successfully enabled on socket\n";
    return true;
}

bool StreamSessionTcpCoordinated::tryReserveZeroCopy()
{
    // Atomically check if idle and reserve for zerocopy (race-condition safe)
    uint32_t expected = 0;
    while (!pending_async_operations_.compare_exchange_weak(expected, 1))
    {
        if (expected != 0)
        {
            // Another operation is in progress; cannot do zerocopy now
            return false;
        }
        // If spurious failure, 'expected' is reloaded with current value, retry
    }
    return true; // Successfully reserved zerocopy
}

void StreamSessionTcpCoordinated::releaseZeroCopy()
{
    // Release zerocopy reservation
    pending_async_operations_--;
}

void StreamSessionTcpCoordinated::sendAsync(const std::shared_ptr<shared_const_buffer> buffer, WriteHandler&& handler)
{
    size_t buffer_size = boost::asio::buffer_size(*buffer);
    
    // Decide whether to attempt zerocopy based on size and availability
    bool should_use_zerocopy = zerocopy_available_ && 
                              buffer_size >= ZEROCOPY_THRESHOLD &&
                              tryReserveZeroCopy();
    
    if (should_use_zerocopy)
    {
        // LOG(TRACE, LOG_TAG) << "Attempting coordinated zerocopy send for " << buffer_size << " bytes\n";
        sendZeroCopy(buffer, std::move(handler));
    }
    else
    {
        if (zerocopy_available_ && buffer_size >= ZEROCOPY_THRESHOLD)
        {
            coordination_fallbacks_++;
            LOG(DEBUG, LOG_TAG) << "Zerocopy fallback due to pending async ops for " << buffer_size << " bytes\n";
        }
        sendRegularCoordinated(buffer, std::move(handler));
    }
}

void StreamSessionTcpCoordinated::sendRegularCoordinated(const std::shared_ptr<shared_const_buffer> buffer, WriteHandler&& handler)
{
    // Track the async operation
    pending_async_operations_++;
    regular_sends_++;
    regular_bytes_ += boost::asio::buffer_size(*buffer);
    
    LOG(DEBUG, LOG_TAG_STATS) << "Regular send started, pending_async_operations now: " << pending_async_operations_.load() << "\n";
    
    // Use the parent class implementation with coordination tracking
    StreamSessionTcp::sendAsync(buffer, [this, handler = std::move(handler)](boost::system::error_code ec, std::size_t bytes_transferred) mutable
    {
        // Decrement pending operations counter
        pending_async_operations_--;
        // LOG(DEBUG, LOG_TAG_STATS) << "Regular send completed, pending_async_operations now: " << pending_async_operations_.load() << "\n";
        
        // Call the original handler
        if (handler)
            handler(ec, bytes_transferred);
        
        // Process any queued sends that might now be able to use zerocopy
        processPendingSends();
    });
}

void StreamSessionTcpCoordinated::sendZeroCopy(const std::shared_ptr<shared_const_buffer> buffer, WriteHandler&& handler)
{
    zerocopy_attempts_++;
    
    size_t buffer_size = boost::asio::buffer_size(*buffer);
    
    // Generate simple sequential buffer ID that matches kernel MSG_ZEROCOPY numbering
    uint32_t buffer_id = next_buffer_id_++;
    
    // Prepare message header for sendmsg using the original buffer directly
    struct msghdr msg = {};
    
    // Create iovec from the shared_const_buffer - no copying needed!
    // Get the first boost::asio::const_buffer from the shared_const_buffer
    auto const_buf = *buffer->begin();
    const auto* data = boost::asio::buffer_cast<const void*>(const_buf);
    struct iovec iov = {const_cast<void*>(data), buffer_size};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    
    // Send with MSG_ZEROCOPY
    // LOG(DEBUG, LOG_TAG) << "Attempting sendmsg with MSG_ZEROCOPY|MSG_DONTWAIT, buffer_size: " << buffer_size << "\n";
    ssize_t result = sendmsg(native_socket_, &msg, MSG_ZEROCOPY | MSG_DONTWAIT);
    // LOG(TRACE, LOG_TAG) << "sendmsg result: " << result << ", errno: " << (result < 0 ? strerror(errno) : "success") << "\n";
    
    if (result < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS)
        {
            LOG(WARNING, LOG_TAG) << "ZeroCopy send would block, falling back to regular send\n";
            releaseZeroCopy(); // Release reservation before fallback
            sendRegularCoordinated(buffer, std::move(handler));
            return;
        }
        else
        {
            LOG(ERROR, LOG_TAG) << "ZeroCopy sendmsg failed: " << strerror(errno) << "\n";
            releaseZeroCopy(); // Release reservation on error
            if (handler)
                handler(boost::system::error_code(errno, boost::system::system_category()), 0);
            return;
        }
    }
    
    if (static_cast<size_t>(result) != buffer_size)
    {
        LOG(WARNING, LOG_TAG) << "ZeroCopy partial send: " << result << "/" << buffer_size << " bytes\n";
        
        // Track the partial zerocopy send - this IS a successful zerocopy operation
        zerocopy_successful_++;
        zerocopy_bytes_ += result;
        outstanding_zerocopy_buffers_++;
        
        // Track the buffer for completion notification
        {
            std::lock_guard<std::mutex> lock(zerocopy_buffers_mutex_);
            pending_zerocopy_buffers_[buffer_id] = buffer;
        }
        
        // Send the unsent portion via regular send
        size_t remaining_bytes = buffer_size - result;
        // Create a sub-buffer for the remaining data
        auto const_buf = *buffer->begin();
        auto remaining_data = boost::asio::buffer_cast<const char*>(const_buf) + result;
        auto remaining_buffer = std::make_shared<std::vector<char>>(remaining_data, remaining_data + remaining_bytes);
        
        LOG(INFO, LOG_TAG) << "Sending remaining " << remaining_bytes << " bytes via regular send\n";
        releaseZeroCopy();
        
        // Send remaining data with shared_ptr to ensure buffer lifetime
        boost::asio::async_write(socket_, boost::asio::buffer(*remaining_buffer),
            [this, handler = std::move(handler), buffer_size, remaining_buffer](boost::system::error_code ec, std::size_t) mutable {
            if (handler) {
                handler(ec, ec ? 0 : buffer_size); // Report full size on success
            }
        });
        return;
    }
    
    // Success - zerocopy send completed immediately (synchronously from our perspective)
    zerocopy_successful_++;
    zerocopy_bytes_ += buffer_size;
    outstanding_zerocopy_buffers_++;
    
    // Track the buffer for completion notification - shared_ptr keeps it alive
    {
        std::lock_guard<std::mutex> lock(zerocopy_buffers_mutex_);
        pending_zerocopy_buffers_[buffer_id] = buffer;
    }
    
    // LOG(TRACE, LOG_TAG) << "ZeroCopy send successful: " << buffer_size << " bytes, ID: " << buffer_id << ", tracking for completion\n";
    
    // Release zerocopy reservation
    releaseZeroCopy();
    
    // Complete the handler immediately
    if (handler)
        handler(boost::system::error_code(), buffer_size);
}

void StreamSessionTcpCoordinated::processPendingSends()
{
    // Avoid recursive processing
    if (processing_queue_.exchange(true))
        return;
    
    std::lock_guard<std::mutex> lock(pending_sends_mutex_);
    while (!pending_sends_.empty() && tryReserveZeroCopy())
    {
        auto pending = std::move(pending_sends_.front());
        pending_sends_.pop();
        
        // Process this send now that we can use zerocopy
        if (pending.use_zerocopy)
        {
            sendZeroCopy(pending.buffer, std::move(pending.handler));
        }
        else
        {
            sendRegularCoordinated(pending.buffer, std::move(pending.handler));
        }
    }
    
    processing_queue_.store(false);
}

void StreamSessionTcpCoordinated::startErrorQueueMonitoring()
{
    if (monitoring_active_.load())
        return;
    
    monitoring_active_.store(true);
    shutdown_requested_.store(false);
    
    LOG(DEBUG, LOG_TAG_COMPLETION) << "Starting error queue monitoring thread for session " << getIP() << "\n";
    
    // Launch dedicated thread for error queue monitoring
    error_queue_thread_ = std::make_unique<std::thread>(&StreamSessionTcpCoordinated::errorQueueMonitoringLoop, this);
}

void StreamSessionTcpCoordinated::stopErrorQueueMonitoring()
{
    if (!monitoring_active_.load())
        return;
        
    LOG(DEBUG, LOG_TAG_COMPLETION) << "Stopping error queue monitoring thread for session " << getIP() << "\n";
    
    shutdown_requested_.store(true);
    monitoring_active_.store(false);
    
    // Wait for thread to complete
    if (error_queue_thread_ && error_queue_thread_->joinable())
    {
        error_queue_thread_->join();
        error_queue_thread_.reset();
    }
}

void StreamSessionTcpCoordinated::errorQueueMonitoringLoop()
{
    // Set thread name for debugging (Linux-specific)
    #ifdef __linux__
    pthread_setname_np(pthread_self(), "zcopy-compl");
    #endif
    
    // LOG(DEBUG, LOG_TAG_COMPLETION) << "Error queue monitoring thread started for session " << getIP() << "\n";
    
    while (monitoring_active_.load() && !shutdown_requested_.load())
    {
        processErrorQueue();
        
        // Sleep for 100ms between checks
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // LOG(DEBUG, LOG_TAG_COMPLETION) << "Error queue monitoring thread stopped for session " << getIP() << "\n";
}

void StreamSessionTcpCoordinated::processErrorQueue()
{
    // static int call_count = 0;
    static int debug_call_count = 0;
    char control_buf[512];
    struct msghdr msg = {};
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);
    
    while (true)
    {
        /*
        if (++call_count % 100 == 1) { // Log every 100th call to avoid spam
            LOG(TRACE, LOG_TAG) << "Processing error queue (call #" << call_count << ")\n";
        }*/
        if (++debug_call_count % 1000 == 1) { // Log every 1000th call for proof it's running
            LOG(DEBUG, LOG_TAG_COMPLETION) << "processErrorQueue() is active (call #" << debug_call_count << "), pending buffers: " << pending_zerocopy_buffers_.size() << "\n";
        }

        ssize_t ret = recvmsg(native_socket_, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
        if (ret < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                LOG(WARNING, LOG_TAG) << "Error queue recv failed: " << strerror(errno) << "\n";
            }
            break; // No more messages or error
        }
        
        // LOG(TRACE, LOG_TAG) << "Received error queue message, size: " << ret << "\n";
        
        // Process control messages
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
        {
            // LOG(TRACE, LOG_TAG) << "Control message: level=" << cmsg->cmsg_level << ", type=" << cmsg->cmsg_type << "\n";
            if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR)
            {
                struct sock_extended_err* ee = reinterpret_cast<struct sock_extended_err*>(CMSG_DATA(cmsg));
                if (ee->ee_errno == 0 && ee->ee_origin == SO_EE_ORIGIN_ZEROCOPY)
                {
                    // Zerocopy completion notification
                    uint32_t lo = ee->ee_info;
                    uint32_t hi = ee->ee_data;
                    
                    uint32_t buffers_in_range = hi - lo + 1;
                    // LOG(TRACE, LOG_TAG_COMPLETION) << "ZeroCopy completion notification: range [" << lo << "-" << hi << "] (" << buffers_in_range << " buffers), tracking " << pending_zerocopy_buffers_.size() << " buffers\n";
                    completion_notifications_received_++;
                    buffers_completed_via_notifications_ += buffers_in_range;
                    // LOG(TRACE, LOG_TAG_STATS) << "Added " << buffers_in_range << " completed buffers, total now: " << buffers_completed_via_notifications_.load() << "\n";
                    
                    // Release completed buffers - much simpler with shared_ptr!
                    {
                        std::lock_guard<std::mutex> lock(zerocopy_buffers_mutex_);
                        for (uint32_t buffer_id = lo; buffer_id <= hi; ++buffer_id) {
                            auto it = pending_zerocopy_buffers_.find(buffer_id);
                            if (it != pending_zerocopy_buffers_.end()) {
                                // LOG(TRACE, LOG_TAG) << "Completed zerocopy buffer ID " << buffer_id << ", shared_ptr will handle cleanup\n";
                                
                                // Remove from tracking - shared_ptr automatically handles cleanup
                                pending_zerocopy_buffers_.erase(it);
                                outstanding_zerocopy_buffers_--;
                            } else {
                                LOG(WARNING, LOG_TAG) << "Completion notification for unknown buffer ID " << buffer_id << "\n";
                            }
                        }
                    }
                }
            }
        }
    }
}

StreamSessionTcpCoordinated::ZeroCopyStats StreamSessionTcpCoordinated::getZeroCopyStats() const
{
    ZeroCopyStats stats;
    stats.zerocopy_attempts = zerocopy_attempts_.load();
    stats.zerocopy_successful = zerocopy_successful_.load();
    stats.zerocopy_bytes = zerocopy_bytes_.load();
    stats.regular_sends = regular_sends_.load();
    stats.regular_bytes = regular_bytes_.load();
    stats.coordination_fallbacks = coordination_fallbacks_.load();
    stats.pending_async_operations = pending_async_operations_.load();
    stats.outstanding_zerocopy_buffers = outstanding_zerocopy_buffers_.load();
    stats.completion_notifications_received = completion_notifications_received_.load();
    stats.completion_notifications_missing = completion_notifications_missing_.load();
    stats.buffers_completed_via_notifications = buffers_completed_via_notifications_.load();
    
    // Debug logging for problematic counters
    LOG(DEBUG, LOG_TAG_STATS) << "Stats debug - buffers_completed=" << stats.buffers_completed_via_notifications
                              << ", pending_async=" << stats.pending_async_operations  << "\n";
    
    return stats;
}

void StreamSessionTcpCoordinated::resetZeroCopyStats()
{
    zerocopy_attempts_.store(0);
    zerocopy_successful_.store(0);
    zerocopy_bytes_.store(0);
    regular_sends_.store(0);
    regular_bytes_.store(0);
    coordination_fallbacks_.store(0);
    completion_notifications_received_.store(0);
    completion_notifications_missing_.store(0);
    buffers_completed_via_notifications_.store(0);
    // Note: outstanding_zerocopy_buffers, and pending_async_operations are not reset as they represent current state
}

// cleanupStaleBuffers() removed - shared_ptr handles cleanup automatically
