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

// local headers
#include "client_settings.hpp"
#include "common/message/message.hpp"
#include "common/time_defs.hpp"
#include "common/time_sync.hpp"
#include "common/chrony_base.hpp"

// standard headers
#include <atomic>
#include <chrono>

/// Provides time synchronization using chrony
/**
 * Uses chrony for precise time synchronization between server and clients.
 * Chrony is a hard dependency - the system will not function without it.
 * When chrony is properly configured, system time is already synchronized,
 * so we can use it directly without additional adjustments.
 */
class TimeProvider
{
public:
    static TimeProvider& getInstance()
    {
        static TimeProvider instance;
        return instance;
    }
    
    /**
     * Get protocol version
     * @return Current protocol version
     */
    time_sync::ProtocolVersion getProtocolVersion() const {
        return protocol_version_;
    }
    
    /**
     * Set protocol version
     * @param version Protocol version to set
     */
    void setProtocolVersion(time_sync::ProtocolVersion version) {
        protocol_version_ = version;
    }
    
    /// Configure the time provider with client settings
    void configure(const ClientSettings::TimeSync& settings);
    
    /// Get current time sync information
    time_sync::TimeSyncInfo getSyncInfo() const;
    
    /// Get current time - uses system time directly when chrony is available
    chronos::time_point_clk getCurrentTime();
    
    /**
     * Check if the client is running on the same machine as the server
     * @return true if client and server are on the same machine
     */
    bool isLocalServer() const {
        return local_server_.load();
    }
    
    /**
     * Set the local server flag directly
     * @param is_local true if client and server are on the same machine
     */
    void setLocalServer(bool is_local) {
        local_server_.store(is_local);
    }
    
    /// Verify chrony is available and properly configured
    /// @throws std::runtime_error if chrony is not available or not synchronized
    void verifyChrony();
    
    /// Check if chrony is properly synchronized
    /// @throws std::runtime_error if chrony is not synchronized
    void checkSynchronization();

    /// Convert a time point to a duration since epoch
    template <typename T>
    static T sinceEpoche(const chronos::time_point_clk& point)
    {
        return std::chrono::duration_cast<T>(point.time_since_epoch());
    }

    /// Convert a timeval struct to a time point
    static chronos::time_point_clk toTimePoint(const tv& timeval)
    {
        return chronos::time_point_clk(chronos::usec(timeval.usec) + chronos::sec(timeval.sec));
    }

    /// Get the current time
    inline static chronos::time_point_clk now()
    {
        return chronos::clk::now();
    }

    /// Get the current server time (same as now() when chrony is properly configured)
    inline static chronos::time_point_clk serverNow()
    {
        return chronos::clk::now();
    }

private:
    TimeProvider();
    TimeProvider(TimeProvider const&) = delete;
    void operator=(TimeProvider const&) = delete;
    
    // Configuration
    ClientSettings::TimeSync settings_;
    
    // Protocol version
    time_sync::ProtocolVersion protocol_version_{time_sync::ProtocolVersion::V2};
    
    // Is chrony available and properly configured
    std::atomic<bool> chrony_available_{false};
    
    // Is server on same machine (no need for chrony in this case)
    std::atomic<bool> local_server_{false};
};
