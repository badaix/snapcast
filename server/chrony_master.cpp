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

#include "chrony_master.hpp"
#include "common/aixlog.hpp"
#include <sstream>

static constexpr auto LOG_TAG = "ChronyMaster";

namespace snapserver {

bool ChronyMaster::init(const std::string& /* config_dir */, uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        LOG(INFO, LOG_TAG) << "Chrony master already initialized";
        return true;
    }
    
    try {
        // Verify chrony is installed - this is a hard requirement
        verifyChronoInstalled();
        
        // Store configuration parameters
        port_ = port;
        
        // Get server address (hostname or IP)
        server_address_ = execCommand("hostname -f 2>/dev/null");
        if (server_address_.empty()) {
            server_address_ = execCommand("hostname 2>/dev/null");
        }
        if (server_address_.empty()) {
            server_address_ = "127.0.0.1";
        }
        
        // Trim newlines
        server_address_.erase(server_address_.find_last_not_of("\n\r") + 1);
        
        LOG(NOTICE, LOG_TAG) << "Chrony present, setting up " << server_address_ << " as chrony MASTER";
        return true;
    }
    catch (const std::exception& e) {
        LOG(ERROR, LOG_TAG) << "Failed to initialize chrony master: " << e.what();
        return false;
    }
}

bool ChronyMaster::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        LOG(INFO, LOG_TAG) << "Chrony master already running";
        return true;
    }
    
    try {
        // Check if chronyd is already running
        std::string status = execCommand("chronyc -c tracking 2>/dev/null");
        if (status.empty()) {
            // Start chronyd if not running
            LOG(INFO, LOG_TAG) << "Starting chronyd";
            std::string result = execCommand("chronyd & echo $!");
            if (result.empty()) {
                LOG(ERROR, LOG_TAG) << "Failed to start chronyd. Time synchronization cannot function.";
                return false;
            }
            
            // Wait for chronyd to start with retry logic
            const int MAX_RETRIES = 5;
            const int RETRY_DELAY_MS = 500;
            bool chronyd_started = false;
            
            for (int retry = 0; retry < MAX_RETRIES; ++retry) {
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                status = execCommand("chronyc -c tracking 2>/dev/null");
                if (!status.empty()) {
                    chronyd_started = true;
                    LOG(INFO, LOG_TAG) << "Chronyd started successfully after " << retry + 1 << " attempts";
                    break;
                }
            }
            
            if (!chronyd_started) {
                LOG(ERROR, LOG_TAG) << "Chronyd started but is not responding after " << MAX_RETRIES << " attempts";
                return false;
            }
        }
        
        // Determine network to allow based on server IP
        std::string network_allow = "0.0.0.0/0"; // Allow all by default
        
        // Configure chrony with retry logic
        const int MAX_CONFIG_RETRIES = 3;
        const int CONFIG_RETRY_DELAY_MS = 500;
        bool config_success = false;
        
        for (int retry = 0; retry < MAX_CONFIG_RETRIES; ++retry) {
            if (retry > 0) {
                LOG(INFO, LOG_TAG) << "Retrying chrony configuration (attempt " << retry + 1 << " of " << MAX_CONFIG_RETRIES << ")";
                std::this_thread::sleep_for(std::chrono::milliseconds(CONFIG_RETRY_DELAY_MS));
            }
            
            // Configure chrony as master with a single command
            std::string cmd = "local stratum 1 orphan";
            config_success = executeChronycCommand(cmd);
            
            if (config_success) {
                // Allow network connections
                executeChronycCommand("allow " + network_allow);
                executeChronycCommand("cmdallow " + network_allow);
                
                // Add fallback servers
                executeChronycCommand("add server time.cloudflare.com iburst");
                executeChronycCommand("add server time.google.com iburst");
                break;
            }
        }
        
        if (config_success) {
            // Mark as running
            running_ = true;
            LOG(NOTICE, LOG_TAG) << "Chrony master configured successfully on " << server_address_;
            return true;
        } else {
            LOG(ERROR, LOG_TAG) << "Failed to configure chrony as master after " << MAX_CONFIG_RETRIES << " attempts";
            return false;
        }
    }
    catch (const std::exception& e) {
        LOG(ERROR, LOG_TAG) << "Error starting chrony master: " << e.what();
        return false;
    }
}

void ChronyMaster::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_) {
        return;
    }
    
    // We don't actually need to stop chronyd - it can continue running
    // Just mark our service as stopped
    running_ = false;
    LOG(INFO, LOG_TAG) << "Chrony master stopped";
}

bool ChronyMaster::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

std::string ChronyMaster::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_) {
        return "Chrony master is not running";
    }
    
    try {
        // Get tracking information
        std::string tracking = execCommand("chronyc -n tracking 2>/dev/null");
        if (tracking.empty()) {
            return "Chrony master is running but tracking information is not available";
        }
        
        return "Chrony master is running on " + server_address_;
    }
    catch (const std::exception& e) {
        LOG(ERROR, LOG_TAG) << "Error getting status: " << e.what();
        return "Chrony master is running on " + server_address_ + " (status unavailable)";
    }
}

std::string ChronyMaster::getServerAddress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return server_address_;
}

uint16_t ChronyMaster::getPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return port_;
}

// No configuration file generation - using direct chronyc commands

// Using isChronyInstalled from ChronyBase

// No monitor thread implementation - assuming chrony works if configured properly

} // namespace snapserver
