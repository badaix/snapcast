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
#include "buffer_pool.hpp"

// local headers
#include "aixlog.hpp"

// standard headers
#include <algorithm>
#include <map>

static constexpr auto LOG_TAG = "BufferPool";

DynamicBufferPool::DynamicBufferPool(size_t initial_count, size_t default_buffer_size)
    : default_buffer_size_(std::max(default_buffer_size, MIN_BUFFER_SIZE))
    , initial_count_(initial_count)
    , last_cleanup_(std::chrono::steady_clock::now())
{
    // Lazy pre-allocation on first acquire
    LOG(DEBUG, LOG_TAG) << "Initialized buffer pool (lazy) with default size " << default_buffer_size_ << "\n";
}

DynamicBufferPool::BufferGuard DynamicBufferPool::acquire(size_t min_size)
{
    check_cleanup();
    
    size_t target_size = std::max({min_size, default_buffer_size_, MIN_BUFFER_SIZE});
    size_t size_bucket = get_size_bucket(target_size);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Lazy initial allocation
    if (available_buffers_.empty() && initial_count_ > 0)
    {
        for (size_t i = 0; i < initial_count_; ++i)
        {
            auto buffer = create_buffer(default_buffer_size_);
            available_buffers_[size_bucket].push_back(std::move(buffer));
        }
        initial_count_ = 0;  // Prevent repeated allocation
    }
    
    // Try to reuse an existing buffer from the same or larger size bucket
    auto it = available_buffers_.lower_bound(size_bucket);
    while (it != available_buffers_.end())
    {
        if (!it->second.empty())
        {
            auto buffer = std::move(it->second.back());
            it->second.pop_back();
            
            buffer->resize_if_needed(target_size);
            ++buffers_reused_;
            
            LOG(TRACE, LOG_TAG) << "Reused buffer from size bucket " << it->first 
                                << " for requested size " << target_size << "\n";
            
            return { *this, std::move(buffer) };
        }
        ++it;
    }
    
    // No suitable buffer found, create a new one
    auto buffer = create_buffer(target_size);
    ++buffers_created_;
    
    LOG(TRACE, LOG_TAG) << "Created new buffer of size " << target_size << "\n";
    
    return { *this, std::move(buffer) };
}

void DynamicBufferPool::release(std::unique_ptr<Buffer> buffer)
{
    if (!buffer)
        return;
        
    check_cleanup();
    
    size_t size_bucket = get_size_bucket(buffer->capacity);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if we have room in this size bucket
    if (available_buffers_[size_bucket].size() < MAX_POOL_SIZE)
    {
        buffer->last_used = std::chrono::steady_clock::now();
        available_buffers_[size_bucket].push_back(std::move(buffer));
        
        LOG(TRACE, LOG_TAG) << "Returned buffer to pool, size bucket " << size_bucket << "\n";
    }
    else
    {
        // Pool is full for this size, let buffer be destroyed
        --total_buffers_;
        bytes_allocated_ -= buffer->capacity;
        
        LOG(TRACE, LOG_TAG) << "Pool full for size bucket " << size_bucket 
                            << ", destroying buffer\n";
    }
}

std::unique_ptr<DynamicBufferPool::Buffer> DynamicBufferPool::create_buffer(size_t size)
{
    auto buffer = std::make_unique<Buffer>(size);
    ++total_buffers_;
    bytes_allocated_ += size;
    return buffer;
}

size_t DynamicBufferPool::get_size_bucket(size_t size)
{
    // Round up to next power of 2 for bucketing
    size_t bucket = MIN_BUFFER_SIZE;
    while (bucket < size)
        bucket *= GROWTH_FACTOR;
    return bucket;
}

DynamicBufferPool::Stats DynamicBufferPool::getStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.total_buffers = total_buffers_;
    stats.bytes_allocated = bytes_allocated_;
    stats.buffers_created = buffers_created_;
    stats.buffers_reused = buffers_reused_;
    stats.cleanup_operations = cleanup_operations_;
    
    // Count available buffers
    for (const auto& bucket : available_buffers_)
    {
        stats.available_buffers += bucket.second.size();
    }
    
    return stats;
}

void DynamicBufferPool::resetStats()
{
    std::lock_guard<std::mutex> lock(mutex_);
    buffers_created_ = 0;
    buffers_reused_ = 0;
    cleanup_operations_ = 0;
}

void DynamicBufferPool::cleanup(std::chrono::seconds max_idle_time)
{
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    last_cleanup_ = now;
    ++cleanup_operations_;
    
    size_t cleaned_count = 0;
    
    for (auto& bucket_pair : available_buffers_)
    {
        auto& deq = bucket_pair.second;
        for (auto it = deq.begin(); it != deq.end(); )
        {
            if (now - (*it)->last_used >= max_idle_time)
            {
                --total_buffers_;
                bytes_allocated_ -= (*it)->capacity;
                it = deq.erase(it);
                ++cleaned_count;
            }
            else
            {
                ++it;
            }
        }
    }
    
    if (cleaned_count > 0)
    {
        LOG(DEBUG, LOG_TAG) << "Cleaned up " << cleaned_count << " stale buffers\n";
    }
}

void DynamicBufferPool::check_cleanup()
{
    auto now = std::chrono::steady_clock::now();
    if (now - last_cleanup_ >= CLEANUP_INTERVAL)
    {
        cleanup(DEFAULT_MAX_IDLE);
    }
}

DynamicBufferPool& DynamicBufferPool::instance()
{
    static DynamicBufferPool instance_;
    return instance_;
}
