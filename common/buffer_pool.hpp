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

#pragma once

// standard headers
#include <atomic>
#include <memory>
#include <mutex>
#include <deque>
#include <vector>
#include <chrono>
#include <map>

/// Dynamic buffer pool for reducing memory allocations in message sending
/**
 * Thread-safe buffer pool that provides reusable buffers for message serialization.
 * - Dynamically grows when buffers are exhausted
 * - Buffers are returned after use for reuse
 * - Supports different buffer sizes through size buckets
 * - Thread-safe operations for multi-client scenarios
 * - Automatic buffer cleanup and pool shrinkage
 */
class DynamicBufferPool
{
public:
    /// Individual buffer wrapper with metadata
    struct Buffer
    {
        std::vector<char> data;
        size_t capacity;
        std::chrono::steady_clock::time_point last_used;
        
        explicit Buffer(size_t size) 
            : data(size)
            , capacity(size)
            , last_used(std::chrono::steady_clock::now())
        {
        }
        
        void resize_if_needed(size_t new_size)
        {
            if (new_size > capacity)
            {
                data.resize(new_size);
                capacity = new_size;
            }
            else
            {
                data.resize(new_size);
            }
            last_used = std::chrono::steady_clock::now();
        }
    };
    
    /// RAII wrapper for automatic buffer return
    class BufferGuard
    {
    public:
        BufferGuard(DynamicBufferPool& pool, std::unique_ptr<Buffer> buffer)
            : pool_(pool), buffer_(std::move(buffer))
        {
        }
        
        ~BufferGuard()
        {
            if (buffer_)
                pool_.release(std::move(buffer_));
        }
        
        // No copying, only moving
        BufferGuard(const BufferGuard&) = delete;
        BufferGuard& operator=(const BufferGuard&) = delete;
        
        BufferGuard(BufferGuard&& other) noexcept
            : pool_(other.pool_), buffer_(std::move(other.buffer_))
        {
        }
        
        BufferGuard& operator=(BufferGuard&& other) noexcept
        {
            if (this != &other)
            {
                if (buffer_)
                    pool_.release(std::move(buffer_));
                buffer_ = std::move(other.buffer_);
            }
            return *this;
        }
        
        std::vector<char>& get() { return buffer_->data; }
        const std::vector<char>& get() const { return buffer_->data; }
        
        void resize(size_t size) { buffer_->resize_if_needed(size); }
        
    private:
        DynamicBufferPool& pool_;
        std::unique_ptr<Buffer> buffer_;
    };

    /// c'tor
    explicit DynamicBufferPool(size_t initial_count = 16, size_t default_buffer_size = 4096);
    
    /// d'tor
    ~DynamicBufferPool() = default;
    
    /// Acquire a buffer of at least the specified size
    BufferGuard acquire(size_t min_size = 0);
    
    /// Get pool statistics
    struct Stats
    {
        size_t total_buffers{0};
        size_t available_buffers{0};
        size_t bytes_allocated{0};
        size_t buffers_created{0};
        size_t buffers_reused{0};
        size_t cleanup_operations{0};
    };
    
    Stats getStats() const;
    void resetStats();
    
    /// Force cleanup of old unused buffers
    void cleanup(std::chrono::seconds max_idle_time = std::chrono::seconds(300));
    
    /// Get singleton instance
    static DynamicBufferPool& instance();

private:
    /// Release a buffer back to the pool
    void release(std::unique_ptr<Buffer> buffer);
    
    /// Create new buffer
    std::unique_ptr<Buffer> create_buffer(size_t size);
    
    /// Size bucket determination
    static size_t get_size_bucket(size_t size);
    
    /// Check and perform cleanup if needed
    void check_cleanup();
    
    // Configuration
    static constexpr size_t MAX_POOL_SIZE = 128;      // Maximum buffers per size bucket
    static constexpr size_t GROWTH_FACTOR = 2;       // Buffer size growth factor
    static constexpr size_t MIN_BUFFER_SIZE = 1024;  // Minimum buffer size
    static constexpr auto CLEANUP_INTERVAL = std::chrono::seconds(30);
    static constexpr auto DEFAULT_MAX_IDLE = std::chrono::seconds(300);
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Storage - map from size bucket to available buffers
    std::map<size_t, std::deque<std::unique_ptr<Buffer>>> available_buffers_;
    
    // Statistics
    size_t total_buffers_{0};
    size_t bytes_allocated_{0};
    size_t buffers_created_{0};
    size_t buffers_reused_{0};
    size_t cleanup_operations_{0};
    
    // Pool configuration
    size_t default_buffer_size_;
    size_t initial_count_;
    std::chrono::steady_clock::time_point last_cleanup_;
};
