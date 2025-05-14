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
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// local headers
#include "time_defs.hpp"
#include "message/message.hpp"

// Forward declaration
namespace msg {
    class Time;
}

namespace time_sync {

/**
 * Protocol version for time synchronization
 * Version 1: Legacy protocol (no version field)
 * Version 2: Adds time source capabilities and selection
 */
enum class ProtocolVersion : uint8_t
{
    V1 = 1, ///< Legacy protocol
    V2 = 2  ///< Enhanced protocol with time source selection
};

/**
 * Time synchronization source types
 * Note: Chrony is the only supported time source
 */
enum class TimeSyncSource : uint8_t
{
    NONE = 255,     ///< No specific time source
    CHRONY = 0,     ///< Chrony time source
    MONOTONIC = 3   ///< Monotonic clock (used only when server and client are on same machine)
};

/**
 * Time synchronization information structure
 */
struct TimeSyncInfo
{
    TimeSyncSource source = TimeSyncSource::NONE;  ///< The time source
    bool available = false;                        ///< Whether the time source is available
    float quality = 0.5f;                          ///< Quality metric (0.0-1.0, higher is better)
    float estimated_error_ms = 50.0f;              ///< Estimated error in milliseconds
    int64_t last_update = 0;                       ///< Last update timestamp (microseconds since epoch)
};

/**
 * Time synchronization mode
 */
enum class SyncMode
{
    fixed,          ///< Use chrony (the only supported source)
    disabled        ///< Disable time synchronization
};

/**
 * Convert a string mode to SyncMode enum
 * @param mode_str The mode string
 * @return The corresponding SyncMode enum value
 */
SyncMode stringToSyncMode(const std::string& mode_str);

/**
 * Convert a SyncMode enum to string
 * @param mode The SyncMode enum value
 * @return The corresponding string representation
 */
std::string syncModeToString(SyncMode mode);

/**
 * Check if chrony is available on the system
 * @return True if chrony is available
 */
bool isChronyAvailable();

/**
 * Get a string representation of a time source
 * @param source The time source
 * @return String representation
 */
std::string timeSourceToString(TimeSyncSource source);

/**
 * Convert an integer to a TimeSyncSource
 * @param source_int Integer representation of the time source
 * @return The corresponding TimeSyncSource enum value
 */
TimeSyncSource intToTimeSource(int source_int);

/**
 * Structure to hold chrony tracking information
 */
struct ChronyTrackingInfo {
    double ref_time = 0.0;          ///< Reference timestamp
    double system_time = 0.0;       ///< System time
    double last_offset = 0.0;       ///< Last offset (ms)
    double rms_offset = 0.0;        ///< RMS offset (ms)
    double freq_ppm = 0.0;          ///< Frequency (ppm)
    double residual_freq_ppm = 0.0; ///< Residual frequency (ppm)
    double skew_ppm = 0.0;          ///< Skew (ppm)
    double root_delay = 0.0;        ///< Root delay (ms)
    double root_dispersion = 0.0;   ///< Root dispersion (ms)
    int64_t update_interval = 0;    ///< Update interval (seconds)
    std::string ref_id;            ///< Reference ID
    std::string ref_source;         ///< Reference source
    std::string stratum;           ///< Stratum
    std::string state;             ///< Synchronization state
    
    /**
     * Parse tracking information from chronyc output
     * @param tracking_output Output from chronyc tracking command
     * @return ChronyTrackingInfo structure with parsed data
     */
    static ChronyTrackingInfo parse(const std::string& tracking_output) {
        ChronyTrackingInfo info;
        
        // Parse the tracking output line by line
        std::istringstream iss(tracking_output);
        std::string line;
        
        while (std::getline(iss, line)) {
            // Skip empty lines
            if (line.empty()) continue;
            
            // Parse key-value pairs
            size_t colon_pos = line.find(":");
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                // Fill in the appropriate field based on the key
                if (key == "Reference ID") info.ref_id = value;
                else if (key == "Stratum") info.stratum = value;
                else if (key == "Ref time (UTC)") {
                    // Parse timestamp if needed
                }
                else if (key == "System time") {
                    // Extract the offset value
                    size_t offset_pos = value.find("seconds");
                    if (offset_pos != std::string::npos) {
                        std::string offset_str = value.substr(0, offset_pos);
                        try {
                            info.system_time = std::stod(offset_str);
                        } catch (...) {}
                    }
                }
                else if (key == "Last offset") {
                    size_t unit_pos = value.find("seconds");
                    if (unit_pos != std::string::npos) {
                        std::string offset_str = value.substr(0, unit_pos);
                        try {
                            info.last_offset = std::stod(offset_str) * 1000.0; // Convert to ms
                        } catch (...) {}
                    }
                }
                else if (key == "RMS offset") {
                    size_t unit_pos = value.find("seconds");
                    if (unit_pos != std::string::npos) {
                        std::string offset_str = value.substr(0, unit_pos);
                        try {
                            info.rms_offset = std::stod(offset_str) * 1000.0; // Convert to ms
                        } catch (...) {}
                    }
                }
                else if (key == "Frequency") {
                    size_t unit_pos = value.find("ppm");
                    if (unit_pos != std::string::npos) {
                        std::string freq_str = value.substr(0, unit_pos);
                        try {
                            info.freq_ppm = std::stod(freq_str);
                        } catch (...) {}
                    }
                }
                else if (key == "Residual freq") {
                    size_t unit_pos = value.find("ppm");
                    if (unit_pos != std::string::npos) {
                        std::string freq_str = value.substr(0, unit_pos);
                        try {
                            info.residual_freq_ppm = std::stod(freq_str);
                        } catch (...) {}
                    }
                }
                else if (key == "Skew") {
                    size_t unit_pos = value.find("ppm");
                    if (unit_pos != std::string::npos) {
                        std::string skew_str = value.substr(0, unit_pos);
                        try {
                            info.skew_ppm = std::stod(skew_str);
                        } catch (...) {}
                    }
                }
                else if (key == "Root delay") {
                    size_t unit_pos = value.find("seconds");
                    if (unit_pos != std::string::npos) {
                        std::string delay_str = value.substr(0, unit_pos);
                        try {
                            info.root_delay = std::stod(delay_str) * 1000.0; // Convert to ms
                        } catch (...) {}
                    }
                }
                else if (key == "Root dispersion") {
                    size_t unit_pos = value.find("seconds");
                    if (unit_pos != std::string::npos) {
                        std::string disp_str = value.substr(0, unit_pos);
                        try {
                            info.root_dispersion = std::stod(disp_str) * 1000.0; // Convert to ms
                        } catch (...) {}
                    }
                }
                else if (key == "Update interval") {
                    size_t unit_pos = value.find("seconds");
                    if (unit_pos != std::string::npos) {
                        std::string interval_str = value.substr(0, unit_pos);
                        try {
                            info.update_interval = std::stoll(interval_str);
                        } catch (...) {}
                    }
                }
                else if (key == "Leap status") info.state = value;
                else if (key == "Reference source") info.ref_source = value;
            }
        }
        
        return info;
    }
};

/**
 * Structure to hold time information from a specific source
 */
struct TimeValue {
    TimeSyncSource source;              ///< The time source used
    chronos::time_point_clk timestamp; ///< The timestamp using the chronos clock
    std::string raw;                   ///< Raw output from the time source
    float quality = 0.5f;              ///< Quality metric (0.0-1.0, higher is better)
    float estimated_error_ms = 50.0f;  ///< Estimated error in milliseconds
};

/**
 * Get default quality metrics for a time source
 * @param source The time source
 * @return TimeSyncInfo with default quality metrics for the source
 */
TimeSyncInfo getDefaultQualityMetrics(TimeSyncSource source);

/**
 * Ensure a valid protocol version
 * @param version The protocol version to check
 * @return A valid protocol version (defaults to V1 if invalid)
 */
ProtocolVersion ensureValidProtocolVersion(uint8_t version);

/**
 * Create a complete time source info map with availability and quality metrics
 * @return Map of all time sources with their availability and quality metrics
 */
std::map<TimeSyncSource, TimeSyncInfo> getAllTimeSourcesInfo();

/**
 * Structure containing comprehensive time status information
 */
struct TimeStatus
{
    TimeSyncSource active_source;       // Currently active time source
    TimeSyncInfo active_source_info;    // Info about the active source
    TimeValue current_time;             // Current time value
    std::map<TimeSyncSource, TimeSyncInfo> available_sources; // All available sources
    ProtocolVersion protocol_version;   // Protocol version in use
    // Legacy field kept for API compatibility - not used with chrony-based synchronization
    double diff_ms{0};                  // Time difference in ms (for client)
};

/**
 * Get comprehensive time status information
 * @param diff_ms Optional time difference to server in microseconds (for client)
 * @return TimeStatus object with all time-related information
 */
TimeStatus getTimeStatus(double diff_ms = 0);

/**
 * Generate log messages for time status
 * @param status The TimeStatus object containing time information
 * @param log_tag The tag to use for logging
 */
void logTimeStatus(const TimeStatus& status, const std::string& log_tag);

/**
 * Initialize and log time synchronization status in a standardized way
 * This function should be used by both client and server for consistent logging
 * @param log_tag The tag to use for logging
 * @param diff_ms Optional time difference to server in ms (for client only)
 * @param protocol_version Optional protocol version (defaults to V2)
 * @param preferred_source Optional preferred time source
 * @return The initialized TimeStatus object
 */
TimeStatus initAndLogTimeSync(const std::string& log_tag, 
                              double diff_ms = 0, 
                              ProtocolVersion protocol_version = ProtocolVersion::V2,
                              TimeSyncSource preferred_source = TimeSyncSource::NONE);

/**
 * Get current time from the best available or specified time source
 * @param specific Specific time source to use (NONE for auto-selection)
 * @param preferred Ordered list of preferred time sources to try
 * @return TimeValue containing the time information
 * @throws std::runtime_error if no valid time source is available
 */
TimeValue getTime(
    TimeSyncSource specific = TimeSyncSource::NONE,
    const std::vector<TimeSyncSource>& preferred = {
        TimeSyncSource::CHRONY,
        TimeSyncSource::MONOTONIC
    });

/**
 * Populate a time message with standardized information
 * @param timeMsg Pointer to the time message to populate
 * @param source Time source to use (NONE for auto-selection)
 * @param version Protocol version to use
 */
void populateTimeMessage(msg::Time* timeMsg, 
                         TimeSyncSource source = TimeSyncSource::NONE,
                         ProtocolVersion version = ProtocolVersion::V2);

/**
 * Process a time response message and create a TimeStatus object
 * @param response Pointer to the time response message
 * @param diff_ms Time difference to server in milliseconds
 * @return TimeStatus object with processed information
 * @throws std::invalid_argument if response is null
 */
TimeStatus processTimeResponse(const msg::Time* response, double diff_ms = 0);

} // namespace time_sync
