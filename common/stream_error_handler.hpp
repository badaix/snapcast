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

#include "aixlog.hpp"
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <string>

namespace snapcast {

/**
 * Utility class for standardized stream error handling
 * Provides methods for handling resource errors and scheduling retries
 */
class StreamErrorHandler {
public:
    /**
     * Handle a resource access error with standardized logging and retry scheduling
     * @param resource_path Path to the resource that failed
     * @param log_tag Tag to use for logging
     * @param timer Timer to use for scheduling retry
     * @param retry_callback Callback to invoke for retry
     * @param retry_delay Delay before retry
     */
    template<typename Callback>
    static void handleResourceError(
        const std::string& resource_path,
        const std::string& log_tag,
        boost::asio::steady_timer& timer,
        Callback retry_callback,
        std::chrono::milliseconds retry_delay = std::chrono::milliseconds(500))
    {
        LOG(ERROR, log_tag) << "Error accessing resource: " << resource_path 
                           << ", retrying in " << retry_delay.count() << "ms\n";
                           
        scheduleRetry(timer, retry_callback, retry_delay);
    }
    
    /**
     * Schedule a retry operation
     * @param timer Timer to use for scheduling
     * @param retry_callback Callback to invoke for retry
     * @param retry_delay Delay before retry
     */
    template<typename Callback>
    static void scheduleRetry(
        boost::asio::steady_timer& timer,
        Callback retry_callback,
        std::chrono::milliseconds retry_delay = std::chrono::milliseconds(500))
    {
        timer.expires_after(retry_delay);
        timer.async_wait([retry_callback](const boost::system::error_code& ec) {
            if (!ec) {
                retry_callback();
            }
        });
    }
};

} // namespace snapcast
