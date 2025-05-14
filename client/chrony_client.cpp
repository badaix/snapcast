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

#include "chrony_client.hpp"
#include "common/aixlog.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <signal.h>

namespace fs = std::filesystem;

static constexpr auto LOG_TAG = "ChronyClient";

namespace snapclient {

ChronyClient::~ChronyClient() {
    disconnect();
}

void ChronyClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return;
    }
    
    LOG(INFO, LOG_TAG) << "Client disconnecting but keeping chrony server connection active";
    
    // Never remove the server from chrony sources
    // Just reset our internal connection state
    connected_ = false;
    
    LOG(INFO, LOG_TAG) << "Client disconnected but chrony still running and connected to server";
}

bool ChronyClient::init(const std::string& config_dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        LOG(ERROR, LOG_TAG) << "Cannot initialize chrony client while already connected";
        return false;
    }
    
    // Skip chrony setup if on_server flag is set
    if (shouldSkipSetup(true)) {
        LOG(NOTICE, LOG_TAG) << "Skipping chrony setup as client is running on the same machine as server";
        return true;
    }
    
    try {
        // Verify chrony is installed
        verifyChronoInstalled();
        
        // Store configuration directory
        config_dir_ = config_dir;
        
        LOG(INFO, LOG_TAG) << "Initialized chrony client";
        return true;
    }
    catch (const std::exception& e) {
        LOG(ERROR, LOG_TAG) << "Failed to initialize chrony client: " << e.what();
        return false;
    }
}

bool ChronyClient::connectToServer(const std::string& server_address, uint16_t port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        LOG(INFO, LOG_TAG) << "Already connected to a server";
        return true;
    }
    
    // Skip chrony setup if on_server flag is set
    if (shouldSkipSetup(true)) {
        LOG(NOTICE, LOG_TAG) << "Skipping chrony connection as client is running on the same machine as server";
        // Just mark as connected but don't actually configure chrony
        server_address_ = server_address;
        port_ = port;
        connected_ = true;
        return true;
    }
    
    // Store server information
    server_address_ = server_address;
    port_ = port;
    
    // Add retry logic for connection attempts
    bool success = false;
    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 500;
    
    for (int retry = 0; retry < MAX_RETRIES; ++retry) {
        if (retry > 0) {
            LOG(INFO, LOG_TAG) << "Retrying connection to chrony server (attempt " << retry + 1 << " of " << MAX_RETRIES << ")";
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
        }
        
        // Simple command to add server with appropriate options
        std::string cmd = "add server " + server_address + " prefer iburst";
        success = executeChronycCommand(cmd);
        
        if (success) {
            // Add a fallback pool for redundancy
            executeChronycCommand("add pool pool.ntp.org iburst");
            break;
        }
    }
    
    if (success) {
        // Mark as connected
        connected_ = true;
        LOG(INFO, LOG_TAG) << "Successfully connected to chrony server at " << server_address;
        return true;
    } else {
        LOG(ERROR, LOG_TAG) << "Failed to connect to chrony server at " << server_address << " after " << MAX_RETRIES << " attempts";
        return false;
    }
}

bool ChronyClient::shouldSkipSetup(bool on_server) const {
    // Skip chrony setup if on_server flag is set
    if (on_server) {
        return true;
    }
    
    // Check if snapserver is running on the same system using pgrep
    // This is more efficient than using grep with ps
    std::string ps_output = execCommand("pgrep snapserver");
    if (!ps_output.empty()) {
        // Found snapserver process running locally
        return true;
    }
    
    return false;
}

bool ChronyClient::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

// Implementation moved to the override method below

std::string ChronyClient::getServerAddress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return server_address_;
}

uint16_t ChronyClient::getServerPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return port_;
}

std::optional<time_sync::ChronyTrackingInfo> ChronyClient::getTrackingInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return std::nullopt;
    }
    
    if (shouldSkipSetup(false)) {
        // When running on the same machine as the server, we don't use chrony
        return std::nullopt;
    }
    
    try {
        // Get tracking information using chronyc tracking
        std::string tracking = execCommand("chronyc -c tracking 2>/dev/null");
        if (tracking.empty()) {
            return std::nullopt;
        }
        
        // Parse CSV output
        std::istringstream iss(tracking);
        std::string line;
        if (std::getline(iss, line)) {
            // Create and return tracking info
            time_sync::ChronyTrackingInfo info;
            // Store the line in ref_source since raw_data doesn't exist
            info.ref_source = line;
            return info;
        }
    }
    catch (const std::exception& e) {
        LOG(ERROR, LOG_TAG) << "Failed to get tracking info: " << e.what();
    }
    
    return std::nullopt;
}

std::string ChronyClient::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (shouldSkipSetup(false)) {
        return "Using local monotonic clock (server on same machine)";
    }
    
    if (!connected_) {
        return "Not connected to a chrony server";
    }
    
    try {
        std::stringstream status;
        status << "Connected to chrony server at " << server_address_;
        
        // Get additional status from base class if available
        std::string baseStatus = chrony::ChronyBase::getStatus();
        if (!baseStatus.empty()) {
            status << "\n" << baseStatus;
        }
        
        return status.str();
    }
    catch (const std::exception& e) {
        LOG(ERROR, LOG_TAG) << "Error getting status: " << e.what();
        return "Connected to chrony server at " + server_address_ + " (status unavailable)";
    }
}

bool ChronyClient::configureClient(const std::string& server_address, uint16_t /* port */) {
    // Configure chrony client with server and options
    LOG(INFO, LOG_TAG) << "Configuring chrony client with server: " << server_address;
    
    // Add server with appropriate options
    bool success = executeChronycCommand("add server " + server_address + " prefer iburst");
    if (!success) {
        LOG(ERROR, LOG_TAG) << "Failed to add server to chrony configuration";
        return false;
    }
    
    // Send burst command to speed up initial synchronization
    executeChronycCommand("burst");
    
    // Add fallback pool
    executeChronycCommand("add pool pool.ntp.org iburst");
    
    LOG(INFO, LOG_TAG) << "Successfully configured chrony client";
    return true;
}

// Using isSynchronized from ChronyBase

bool ChronyClient::startClient() {
    // Check if chronyd is already running
    std::string status = execCommand("chronyc -c tracking 2>/dev/null");
    if (!status.empty()) {
        LOG(INFO, LOG_TAG) << "Chronyd already running, configuring as client\n";
    } else {
        // Start chronyd if not running
        std::string cmd = "chronyd";
        LOG(INFO, LOG_TAG) << "Starting chronyd: " << cmd << "\n";
        
        // Start chronyd as a background process
        std::string result = execCommand(cmd + " & echo $!");
        if (result.empty()) {
            LOG(ERROR, LOG_TAG) << "Failed to start chronyd\n";
            return false;
        }
        
        // Extract PID
        try {
            chrony_pid_ = std::stoi(result);
            LOG(INFO, LOG_TAG) << "Started chronyd with PID " << chrony_pid_ << "\n";
        } catch (const std::exception& e) {
            LOG(ERROR, LOG_TAG) << "Failed to parse chronyd PID: " << e.what() << "\n";
        }
        // Wait for chronyd to start
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Configure client using chronyc -a commands
    if (!configureClient(server_address_, port_)) {
        LOG(ERROR, LOG_TAG) << "Failed to configure chrony client\n";
        return false;
    }
    
    // Verify server was added by checking sources
    bool server_verified = false;
    for (int retry = 0; retry < 5; retry++) {
        std::string sources = execCommand("chronyc -c sources 2>/dev/null");
        // Check for either the original server address or the resolved address
        if (sources.find(server_address_) != std::string::npos || 
            (!resolved_address_.empty() && sources.find(resolved_address_) != std::string::npos)) {
            server_verified = true;
            LOG(INFO, LOG_TAG) << "Server verified in sources list\n";
            break;
        }
        
        LOG(INFO, LOG_TAG) << "Waiting for server to appear in sources list (attempt " << retry+1 << "/5)\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    if (!server_verified) {
        LOG(WARNING, LOG_TAG) << "Server not found in sources list, but continuing anyway\n";
    }
    
    LOG(INFO, LOG_TAG) << "Chrony client started successfully\n";
    return true;
}

void ChronyClient::stopClient() {
    // Never stop chrony or remove servers
    LOG(INFO, LOG_TAG) << "Client stopping but keeping chrony running and connected to server\n";
    
    // Just reset our internal state
    chrony_pid_ = -1;
    
    LOG(INFO, LOG_TAG) << "Client stopped but chrony still running and connected to server\n";
}

// No monitor thread implementation - assuming chrony works if configured properly

} // namespace snapclient
