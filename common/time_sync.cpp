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

#include "time_sync.hpp"
#include "aixlog.hpp"
#include "time_defs.hpp"
#include "message/time.hpp"

// Standard headers
#include <string>
#include <map>
#include <chrono>
#include <optional>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

static constexpr auto LOG_TAG = "TimeSync";
static constexpr auto CHRONY_LOG_TAG = "ChronyTracker";

// Helper function to safely execute a command and capture its output
static std::string execCommand(const std::string& cmd) {
    std::string result;
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        LOG(WARNING, LOG_TAG) << "Failed to execute command: " << cmd << "\n";
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

namespace snapcast {

/**
 * Simple container for chrony tracking information
 */
struct ChronyTrackingInfo {
    // Raw output from chronyc
    std::string raw_output;
    
    // Estimated quality (0.0-1.0)
    double quality{0.8};
    
    // Estimated error in milliseconds
    double estimated_error_ms{1.0};
    
    // Create from chronyc output
    static ChronyTrackingInfo fromChronyc() {
        ChronyTrackingInfo info;
        
        // Always use CSV format for easier parsing
        std::string cmd = "chronyc -c tracking 2>/dev/null";
        info.raw_output = execCommand(cmd);
        
        if (!info.raw_output.empty()) {
            // Parse the CSV output
            // Format: Reference ID,IP,Stratum,Ref time (UTC),System time,Last offset,RMS offset,Frequency,Residual freq,Skew,Root delay,Root dispersion,Update interval,Leap status
            std::stringstream ss(info.raw_output);
            std::string field;
            std::vector<std::string> fields;
            
            // Split by commas
            while (std::getline(ss, field, ',')) {
                fields.push_back(field);
            }
            
            // Parse relevant fields if we have enough
            if (fields.size() >= 10) {
                try {
                    // RMS offset (field 6) is in seconds, convert to ms
                    double rms_offset = std::stod(fields[6]) * 1000.0;
                    
                    // Root delay (field 10) is in seconds, convert to ms
                    double root_delay = std::stod(fields[10]) * 1000.0;
                    
                    // Root dispersion (field 11) is in seconds, convert to ms
                    double root_dispersion = std::stod(fields[11]) * 1000.0;
                    
                    // Use these values to calculate quality and error
                    // Quality is inversely proportional to RMS offset
                    info.quality = std::min(1.0, std::max(0.0, 1.0 - (rms_offset / 10.0)));
                    
                    // Error estimate is based on root dispersion and delay
                    info.estimated_error_ms = root_dispersion + (root_delay / 2.0);
                } catch (const std::exception& e) {
                    LOG(WARNING, CHRONY_LOG_TAG) << "Error parsing chrony CSV output: " << e.what() << "\n";
                    info.quality = 0.5; // Fallback to moderate quality
                    info.estimated_error_ms = 5.0; // Conservative estimate
                }
            } else {
                LOG(WARNING, CHRONY_LOG_TAG) << "Unexpected chrony CSV format\n";
                info.quality = 0.5; // Fallback to moderate quality
                info.estimated_error_ms = 5.0; // Conservative estimate
            }
        } else {
            // No output means chrony isn't running or has issues
            info.quality = 0.0;
            info.estimated_error_ms = 1000.0;
        }
        
        return info;
    }
    
    // Get a formatted string representation
    std::string toString() const {
        if (raw_output.empty()) {
            return "No chrony tracking information available";
        }
        return raw_output;
    }
};

/**
 * Simple utility for chrony tracking information
 * Provides direct access to chronyc output
 */
class ChronyTracker {
public:
    // Singleton accessor
    static ChronyTracker& getInstance() {
        static ChronyTracker instance;
        return instance;
    }
    
    // Get tracking information
    ChronyTrackingInfo getTrackingInfo() {
        return ChronyTrackingInfo::fromChronyc();
    }
    
    // Check if chrony is available
    bool isAvailable() {
        return !getTrackingInfo().raw_output.empty();
    }
    
    // Get raw tracking output
    std::string getRawTracking() {
        return getTrackingInfo().raw_output;
    }
};

} // namespace snapcast

namespace time_sync {

SyncMode stringToSyncMode(const std::string& mode_str)
{
    if (mode_str == "fixed") return SyncMode::fixed;
    if (mode_str == "disabled") return SyncMode::disabled;
    
    // Default to fixed if not recognized
    LOG(WARNING, LOG_TAG) << "Unknown sync mode: " << mode_str << ", defaulting to fixed (chrony)\n";
    return SyncMode::fixed;
}

std::string syncModeToString(SyncMode mode)
{
    switch (mode)
    {
        case SyncMode::fixed:
            return "fixed";
        case SyncMode::disabled:
            return "disabled";
        default:
            return "unknown";
    }
}

bool isChronyAvailable()
{
    // Check if chronyd is running
    std::string result = execCommand("ps -ef | grep chrony[d] 2>/dev/null");
    if (!result.empty())
    {
        LOG(DEBUG, LOG_TAG) << "Chrony daemon detected running\n";
        return true;
    }
    
    // Check if chronyc is available
    result = execCommand("which chronyc 2>/dev/null");
    if (!result.empty())
    {
        LOG(DEBUG, LOG_TAG) << "Chronyc binary detected\n";
        return true;
    }
    
    return false;
}

std::string timeSourceToString(TimeSyncSource source)
{
    switch (source)
    {
        case TimeSyncSource::CHRONY:
            return "Chrony";
        case TimeSyncSource::MONOTONIC:
            return "Monotonic";
        case TimeSyncSource::NONE:
            return "None";
        default:
            return "Unknown";
    }
}

TimeSyncSource intToTimeSource(int source_int)
{
    switch (source_int)
    {
        case 0:
            return TimeSyncSource::CHRONY;
        case 3:
            return TimeSyncSource::MONOTONIC;
        case 255:
            return TimeSyncSource::NONE;
        default:
            LOG(WARNING, LOG_TAG) << "Unknown time source: " << source_int << ", using CHRONY\n";
            return TimeSyncSource::CHRONY; // Default to CHRONY instead of NONE
    }
}

/**
 * Query a specific time source and return its time value
 * @param source The time source to query
 * @return TimeValue containing the time information
 * @throws std::runtime_error if the time source is not available
 */
static TimeValue queryTimeSource(TimeSyncSource source) {
    std::string raw;
    chronos::time_point_clk now;
    
    if (source == TimeSyncSource::MONOTONIC) {
        // Use monotonic clock
        now = chronos::clk::now();
        raw = "monotonic";
    } else if (source == TimeSyncSource::CHRONY) {
        // Use chrony time
        now = chronos::clk::now(); // Use monotonic clock as base
        raw = snapcast::ChronyTracker::getInstance().getRawTracking();
        if (raw.empty()) {
            throw std::runtime_error("Chrony not available");
        }
    } else {
        throw std::runtime_error("Unknown time source");
    }

    return TimeValue{source, now, raw};
}

TimeValue getTime(TimeSyncSource specific, const std::vector<TimeSyncSource>& preferred) {
    // If a specific time source is requested, try to use it
    if (specific != TimeSyncSource::NONE) {
        try {
            return queryTimeSource(specific);
        } catch (const std::exception& e) {
            LOG(ERROR, LOG_TAG) << "Error querying time source " << timeSourceToString(specific) << ": " << e.what() << "\n";
            throw;
        }
    }

    // Try chrony first, then monotonic
    try {
        LOG(DEBUG, LOG_TAG) << "Using chrony time source\n";
        return queryTimeSource(TimeSyncSource::CHRONY);
    } catch (const std::exception& e) {
        LOG(WARNING, LOG_TAG) << "Failed to get time from chrony: " << e.what() << "\n";
    }

    // Fall back to monotonic clock
    LOG(WARNING, LOG_TAG) << "Falling back to monotonic clock\n";
    try {
        return queryTimeSource(TimeSyncSource::MONOTONIC);
    } catch (const std::exception& e) {
        LOG(ERROR, LOG_TAG) << "Critical error: Failed to get monotonic time: " << e.what() << "\n";
        throw std::runtime_error("No valid time sources available.");
    }
}

TimeSyncInfo getDefaultQualityMetrics(TimeSyncSource source)
{
    TimeSyncInfo info;
    info.source = source;
    
    // Set default quality metrics based on source type
    switch (source) {
        case TimeSyncSource::CHRONY:
            {
                // Use ChronyTracker to get quality metrics
                auto tracking_info = snapcast::ChronyTracker::getInstance().getTrackingInfo();
                info.quality = static_cast<float>(tracking_info.quality);
                info.estimated_error_ms = static_cast<float>(tracking_info.estimated_error_ms);
                LOG(DEBUG, LOG_TAG) << "Using chrony quality metrics: quality=" << info.quality 
                                   << ", error=" << info.estimated_error_ms << "ms\n";
            }
            break;
        case TimeSyncSource::MONOTONIC:
            info.quality = 0.5f;
            info.estimated_error_ms = 50.0f;
            break;
        default:
            info.quality = 0.0f;
            info.estimated_error_ms = 1000.0f;
            break;
    }
    
    return info;
}

ProtocolVersion ensureValidProtocolVersion(uint8_t version)
{
    // If no version is specified (version = 0), assume V1
    if (version == 0 || version == static_cast<uint8_t>(ProtocolVersion::V1)) {
        return ProtocolVersion::V1;
    } else if (version >= static_cast<uint8_t>(ProtocolVersion::V2)) {
        return ProtocolVersion::V2;
    } else {
        // Default to V1 for any unknown version
        LOG(WARNING, LOG_TAG) << "Unknown protocol version: " << static_cast<int>(version) 
                             << ", defaulting to V1\n";
        return ProtocolVersion::V1;
    }
}

std::map<TimeSyncSource, TimeSyncInfo> getAllTimeSourcesInfo()
{
    std::map<TimeSyncSource, TimeSyncInfo> time_sources;
    
    // Only check CHRONY and MONOTONIC sources
    for (auto source : {TimeSyncSource::CHRONY, TimeSyncSource::MONOTONIC}) {
        // Get default quality metrics
        TimeSyncInfo info = getDefaultQualityMetrics(source);
        
        // Set availability
        if (source == TimeSyncSource::CHRONY) {
            info.available = isChronyAvailable();
        } else { // MONOTONIC
            info.available = true; // Monotonic clock is always available
        }
        
        // Store in map
        time_sources[source] = info;
    }
    
    return time_sources;
}

TimeStatus getTimeStatus(double diff_ms)
{
    TimeStatus status;
    
    // Get all available time sources
    status.available_sources = getAllTimeSourcesInfo();
    
    // Get current time from best available source
    status.current_time = getTime();
    status.active_source = status.current_time.source;
    
    // Set active source info
    auto it = status.available_sources.find(status.active_source);
    if (it != status.available_sources.end()) {
        status.active_source_info = it->second;
    } else {
        // Fallback if source not in available_sources
        status.active_source_info = getDefaultQualityMetrics(status.active_source);
    }
    
    // Set protocol version (default to V1)
    status.protocol_version = ProtocolVersion::V1;
    
    // Set time difference (for client)
    status.diff_ms = diff_ms;
    
    return status;
}

void logTimeStatus(const TimeStatus& status, const std::string& log_tag)
{
    // Log active time source and its quality
    LOG(NOTICE, log_tag) << "Time synchronization initialized using " 
                        << timeSourceToString(status.active_source) 
                        << " (quality: " << status.active_source_info.quality 
                        << ", estimated error: " << status.active_source_info.estimated_error_ms << "ms)";
    
    // For chrony, directly show the raw chronyc output
    if (status.active_source == TimeSyncSource::CHRONY) {
        // Get raw output from chronyc
        std::string chrony_output = snapcast::ChronyTracker::getInstance().getRawTracking();
        if (!chrony_output.empty()) {
            LOG(INFO, log_tag) << "Chrony tracking information:\n" << chrony_output;
        }
    } else {
        // For other sources, log standard information
        LOG(INFO, log_tag) << "Current time source: " 
                          << timeSourceToString(status.current_time.source) 
                          << ", quality: " << status.current_time.quality 
                          << ", error: " << status.current_time.estimated_error_ms << "ms";
    }
    
    // Log protocol version as a separate log entry
    if (status.protocol_version > ProtocolVersion::V1) {
        LOG(INFO, log_tag) << "Time sync protocol version: V" 
                          << static_cast<int>(status.protocol_version);
        
        // Log time difference if available
        if (status.diff_ms != 0) {
            LOG(INFO, log_tag) << "Time difference to server: " 
                              << status.diff_ms << "ms";
        }
    } else {
        LOG(INFO, log_tag) << "Using legacy time sync protocol V1";
    }
}

TimeStatus initAndLogTimeSync(const std::string& log_tag, 
                                double diff_ms, 
                                ProtocolVersion protocol_version,
                                TimeSyncSource /* preferred_source */)
{
    // Get all available time sources
    auto time_sources = getAllTimeSourcesInfo();
    
    // With chrony-based synchronization, prioritize chrony when available
    TimeSyncSource selected_source;
    
    // Check if chrony is available
    auto chrony_it = time_sources.find(TimeSyncSource::CHRONY);
    if (chrony_it != time_sources.end() && chrony_it->second.available) {
        selected_source = TimeSyncSource::CHRONY;
        LOG(INFO, log_tag) << "Using chrony for time synchronization";
    } 
    // If server and client are on same machine, use monotonic clock
    else if (time_sources.find(TimeSyncSource::MONOTONIC) != time_sources.end() && 
             time_sources[TimeSyncSource::MONOTONIC].available) {
        selected_source = TimeSyncSource::MONOTONIC;
        LOG(INFO, log_tag) << "Server and client on same machine, using local clock";
    }
    // Otherwise use chrony
    else {
        selected_source = TimeSyncSource::CHRONY;
        LOG(INFO, log_tag) << "Using chrony for time synchronization";
    }
    
    // Get the source info
    TimeSyncInfo sourceInfo;
    auto it = time_sources.find(selected_source);
    if (it != time_sources.end()) {
        sourceInfo = it->second;
    } else {
        sourceInfo = getDefaultQualityMetrics(selected_source);
    }
    
    // Get current time from the selected source
    TimeValue current_time = getTime(selected_source);
    
    // Create time status manually instead of calling getTimeStatus()
    // to avoid race condition with a second call to getAllTimeSourcesInfo()
    TimeStatus status;
    status.available_sources = time_sources;
    status.current_time = current_time;
    status.active_source = selected_source;
    status.active_source_info = sourceInfo;
    status.protocol_version = protocol_version;
    status.diff_ms = diff_ms;
    
    // Log the time status
    logTimeStatus(status, log_tag);
    
    return status;
}

void populateTimeMessage(msg::Time* timeMsg, 
                         TimeSyncSource source,
                         ProtocolVersion version)
{
    if (!timeMsg) {
        LOG(ERROR, LOG_TAG) << "Cannot populate null time message\n";
        return;
    }
    
    // Set protocol version
    timeMsg->version = static_cast<uint8_t>(version);
    
    // Initialize latency to zero
    timeMsg->latency.sec = 0;
    timeMsg->latency.usec = 0;
    
    // If a specific time source is requested, try to use it
    if (source != TimeSyncSource::NONE) {
        bool source_available = (source == TimeSyncSource::MONOTONIC) || 
                              (source == TimeSyncSource::CHRONY && isChronyAvailable());
        if (source_available) {
            try {
                auto time_value = queryTimeSource(source);
                timeMsg->source = static_cast<uint8_t>(source);
                timeMsg->quality = time_value.quality;
                timeMsg->error_ms = time_value.estimated_error_ms;
                // Set timestamp in the BaseMessage sent field
                timeMsg->sent.sec = static_cast<int32_t>(time_value.timestamp.time_since_epoch().count() / 1000000);
                timeMsg->sent.usec = static_cast<int32_t>(time_value.timestamp.time_since_epoch().count() % 1000000);
            } catch (const std::exception& e) {
                LOG(WARNING, LOG_TAG) << "Failed to get time from " << timeSourceToString(source) 
                                     << ", falling back to best available: " << e.what() << "\n";
                // Fall back to best available
                source = TimeSyncSource::NONE;
            }
        } else {
            LOG(WARNING, LOG_TAG) << "Requested time source " << timeSourceToString(source) 
                                 << " not available, falling back to best available\n";
            // Fall back to best available
            source = TimeSyncSource::NONE;
        }
    }
    
    // If no specific source or the specific source failed, use best available
    if (source == TimeSyncSource::NONE) {
        try {
            auto time_value = getTime();
            timeMsg->source = static_cast<uint8_t>(time_value.source);
            timeMsg->quality = time_value.quality;
            timeMsg->error_ms = time_value.estimated_error_ms;
            // Set timestamp in the BaseMessage sent field
            timeMsg->sent.sec = static_cast<int32_t>(time_value.timestamp.time_since_epoch().count() / 1000000);
            timeMsg->sent.usec = static_cast<int32_t>(time_value.timestamp.time_since_epoch().count() % 1000000);
        } catch (const std::exception& e) {
            LOG(ERROR, LOG_TAG) << "Failed to get time: " << e.what() << "\n";
            // Use system time as last resort
            struct timeval tv;
            chronos::systemtimeofday(&tv);
            timeMsg->source = static_cast<uint8_t>(TimeSyncSource::CHRONY);
            timeMsg->quality = 0.4f;
            timeMsg->error_ms = 100.0f;
            // Set timestamp in the BaseMessage sent field
            timeMsg->sent.sec = tv.tv_sec;
            timeMsg->sent.usec = tv.tv_usec;
        }
    }
}

TimeStatus processTimeResponse(const msg::Time* response, double /* diff_ms */)
{
    if (!response) {
        throw std::invalid_argument("Time response message is null");
    }
    
    // Create time status
    TimeStatus status;
    
    // Set protocol version
    status.protocol_version = ensureValidProtocolVersion(response->version);
    
    // Set active time source from response
    status.active_source = intToTimeSource(response->source);
    
    // Create TimeValue from response
    chronos::time_point_clk timestamp;
    try {
        // Convert the response timestamp to chronos time_point_clk
        auto duration = chronos::usec(response->sent.sec * 1000000LL + response->sent.usec);
        timestamp = chronos::time_point_clk(duration);
    } catch (const std::exception& e) {
        LOG(WARNING, LOG_TAG) << "Failed to convert timestamp: " << e.what() << "\n";
        // Use current time as fallback
        timestamp = chronos::clk::now();
    }
    
    status.current_time = TimeValue{
        status.active_source,
        timestamp,
        "from_server"
    };
    status.current_time.quality = response->quality;
    status.current_time.estimated_error_ms = response->error_ms;
    
    // Get available sources
    status.available_sources = getAllTimeSourcesInfo();
    
    // With chrony-based synchronization, we don't need time differences
    // But we keep this parameter for API compatibility
    status.diff_ms = 0.0;
    
    return status;
}

} // namespace time_sync
