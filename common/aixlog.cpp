/***
      __   __  _  _  __     __    ___
     / _\ (  )( \/ )(  )   /  \  / __)
    /    \ )(  )  ( / (_/\(  O )( (_ \
    \_/\_/(__)(_/\_)\____/ \__/  \___/
    version 1.5.1
    https://github.com/badaix/aixlog

    This file is part of aixlog
    Copyright (C) 2017-2025 Johannes Pohl

    This software may be modified and distributed under the terms
    of the MIT license.  See the LICENSE file for details.
***/

#include "aixlog.hpp"
#include <unordered_map>

namespace AixLog
{

/// Cache for should_log results to avoid repeated computations
class ShouldLogCache
{
public:
    /// @brief Key for the cache
    struct CacheKey
    {
        int severity;
        std::string tag;
        
        bool operator==(const CacheKey& other) const
        {
            return severity == other.severity && tag == other.tag;
        }
    };
    
    /// @brief Hash function for CacheKey
    struct CacheKeyHash
    {
        std::size_t operator()(const CacheKey& key) const
        {
            return std::hash<int>()(key.severity) ^ 
                   (std::hash<std::string>()(key.tag) << 1);
        }
    };
    
    /// @brief  Try to get from cache
    bool getCached(SEVERITY severity, const char* tag, bool& result)
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        CacheKey key{static_cast<int>(severity), tag ? std::string(tag) : std::string()};
        auto it = cache_.find(key);
        if (it != cache_.end())
        {
            result = it->second;
            cache_hits_++;
            return true;
        }
        cache_misses_++;
        return false;
    }
    
    /// @brief  Store in cache
    void putCache(SEVERITY severity, const char* tag, bool result)
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        CacheKey key{static_cast<int>(severity), tag ? std::string(tag) : std::string()};
        cache_[key] = result;
        
        // Limit cache size to avoid memory growth
        if (cache_.size() > MAX_CACHE_SIZE)
        {
            // Simple eviction: clear half the cache
            auto it = cache_.begin();
            std::advance(it, cache_.size() / 2);
            cache_.erase(cache_.begin(), it);
        }
    }
    
    /// @brief Clear the cache
    void clearCache()
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_.clear();
        cache_hits_ = 0;
        cache_misses_ = 0;
    }
    
    /// Debug stats
    void getStats(size_t& hits, size_t& misses, size_t& size)
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        hits = cache_hits_;
        misses = cache_misses_;
        size = cache_.size();
    }
    
private:
    static constexpr size_t MAX_CACHE_SIZE = 1000;
    std::unordered_map<CacheKey, bool, CacheKeyHash> cache_;
    std::mutex cache_mutex_;
    size_t cache_hits_{0};
    size_t cache_misses_{0};
};

/// Global cache instance
static ShouldLogCache& getShouldLogCache()
{
    static ShouldLogCache instance;
    return instance;
}

/// Cached version of should_log
bool Log::should_log_cached(SEVERITY severity, const char* tag)
{
    auto& cache = getShouldLogCache();
    bool result;
    
    // Try cache first
    if (cache.getCached(severity, tag, result))
    {
        return result;
    }
    
    // Cache miss - compute result
    Log& log = instance();
    std::lock_guard<std::recursive_mutex> lock(log.mutex_);
    
    if (log.log_sinks_.empty())
    {
        result = true; // If no sinks configured, default to logging
    }
    else
    {
        // Convert old SEVERITY enum to new Severity enum
        auto new_severity = static_cast<Severity>(severity);
        
        Metadata temp_metadata;
        temp_metadata.severity = new_severity;
        temp_metadata.tag = tag;
        
        result = false;
        for (const auto& sink : log.log_sinks_)
        {
            if (sink->filter.match(temp_metadata))
            {
                result = true;
                break;
            }
        }
    }
    
    // Cache the result
    cache.putCache(severity, tag, result);
    
    return result;
}

/// Overload for new Severity enum class
bool Log::should_log_cached(Severity severity, const char* tag)
{
    return should_log_cached(static_cast<SEVERITY>(severity), tag);
}

/// Overload for new Severity enum class with std::string tag
bool Log::should_log_cached(Severity severity, const std::string& tag)
{
    return should_log_cached(static_cast<SEVERITY>(severity), tag.c_str());
}

/// Clear cache when log configuration changes
void Log::clearShouldLogCache()
{
    getShouldLogCache().clearCache();
}

/// Get cache statistics (for debugging)
void Log::getShouldLogCacheStats(size_t& hits, size_t& misses, size_t& size)
{
    getShouldLogCache().getStats(hits, misses, size);
}

} // namespace AixLog
