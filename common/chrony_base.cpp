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

#include "chrony_base.hpp"
#include <array>
#include <sstream>
#include <system_error>
#include <unistd.h>

namespace chrony {

std::string ChronyBase::execCommand(const std::string& cmd) const {
    std::string result;
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        LOG(WARNING, LOG_TAG) << "Failed to execute command: " << cmd;
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    // Trim trailing newlines for cleaner output
    if (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.erase(result.find_last_not_of("\n\r") + 1);
    }
    
    return result;
}

bool ChronyBase::isChronyInstalled() const {
    std::string result = execCommand("which chronyd 2>/dev/null");
    return !result.empty();
}

void ChronyBase::verifyChronoInstalled() {
    if (!isChronyInstalled()) {
        throw std::runtime_error("Chrony is not installed. It is required for time synchronization.");
    }
    LOG(INFO, LOG_TAG) << "Chrony is installed\n";
}

bool ChronyBase::executeChronycCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Construct the full command with the -a flag
    std::string full_command = "chronyc -a '" + command + "'";
    
    // Add retry logic
    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 500;
    
    for (int retry = 0; retry < MAX_RETRIES; ++retry) {
        if (retry > 0) {
            LOG(INFO, LOG_TAG) << "Retrying chronyc command (attempt " << retry + 1 << " of " << MAX_RETRIES << ")";
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
        }
        
        LOG(INFO, LOG_TAG) << "Executing chronyc command: " << command;
        
        // Execute the command
        std::string result = execCommand(full_command);
        
        // Check if command succeeded (should see 200 OK response)
        if (result.find("200 OK") != std::string::npos) {
            LOG(INFO, LOG_TAG) << "Chronyc command succeeded";
            return true;
        }
        
        // Special case: some commands don't return 200 OK but still succeed
        if (command == "burst" || command.find("local") == 0) {
            // For these commands, absence of an error message indicates success
            if (result.find("error") == std::string::npos && 
                result.find("Error") == std::string::npos) {
                LOG(INFO, LOG_TAG) << "Chronyc command likely succeeded (no error message)";
                return true;
            }
        }
        
        LOG(WARNING, LOG_TAG) << "Chronyc command failed (attempt " << retry + 1 << " of " << MAX_RETRIES << ")";
    }
    
    LOG(ERROR, LOG_TAG) << "Chronyc command failed after " << MAX_RETRIES << " attempts: " << command;
    return false;
}

bool ChronyBase::isSynchronized() const {
    try {
        // Use chronyc tracking to check synchronization status
        std::string result = execCommand("chronyc -c tracking 2>/dev/null");
        if (result.empty()) {
            return false;
        }
        
        // Parse CSV output
        // Format: <Ref ID>,<IP>,<Stratum>,<Ref time>,<System time>,<Last offset>,<RMS offset>,<Frequency>,<Residual freq>,<Skew>,<Root delay>,<Root dispersion>,<Update interval>,<Leap status>
        std::istringstream iss(result);
        std::string line;
        if (std::getline(iss, line)) {
            // Check if we have at least 3 fields (Ref ID, IP, Stratum)
            size_t comma1 = line.find(',');
            if (comma1 != std::string::npos) {
                size_t comma2 = line.find(',', comma1 + 1);
                if (comma2 != std::string::npos) {
                    // Extract stratum
                    std::string stratum_str = line.substr(comma1 + 1, comma2 - comma1 - 1);
                    try {
                        int stratum = std::stoi(stratum_str);
                        // Stratum 0-15 indicates synchronized, 16 is unsynchronized
                        return stratum >= 0 && stratum < 16;
                    } catch (...) {
                        return false;
                    }
                }
            }
        }
        return false;
    } catch (...) {
        return false;
    }
}

void ChronyBase::checkSynchronization() {
    if (!isSynchronized()) {
        throw std::runtime_error("Chrony is not properly synchronized. Accurate time synchronization is required.");
    }
    LOG(DEBUG, LOG_TAG) << "Chrony synchronization verified\n";
}

std::optional<time_sync::ChronyTrackingInfo> ChronyBase::getTrackingInfo() const {
    std::string tracking = execCommand("chronyc tracking 2>/dev/null");
    if (tracking.empty()) {
        return std::nullopt;
    }
    
    return time_sync::ChronyTrackingInfo::parse(tracking);
}

std::string ChronyBase::getStatus() const {
    std::string status = execCommand("chronyc -c tracking 2>/dev/null");
    if (status.empty()) {
        return "Chrony is not running or not responding";
    }
    
    // Get more detailed status
    std::string sources = execCommand("chronyc -c sources 2>/dev/null");
    std::string sourcestats = execCommand("chronyc -c sourcestats 2>/dev/null");
    
    std::ostringstream oss;
    oss << "Chrony tracking:\n" << status << "\n";
    
    if (!sources.empty()) {
        oss << "Chrony sources:\n" << sources << "\n";
    }
    
    if (!sourcestats.empty()) {
        oss << "Chrony source stats:\n" << sourcestats << "\n";
    }
    
    return oss.str();
}

} // namespace chrony
