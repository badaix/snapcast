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

#include "common/chrony_base.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace snapclient {

/**
 * Manages chrony client configuration and connection to Snapcast server's chrony master
 * 
 * This class configures the local chrony client to synchronize with the Snapcast server's
 * chrony master, ensuring precise time synchronization for audio playback.
 * 
 * When the --on-server flag is set, chrony setup is skipped entirely as the client
 * will use the local monotonic clock for time synchronization.
 */
class ChronyClient : public chrony::ChronyBase {
public:
    // Get the singleton instance
    static ChronyClient& getInstance() {
        static ChronyClient instance;
        return instance;
    }
    
    /**
     * Initialize the chrony client
     * @param config_dir Directory to store configuration files
     * @return true if initialization was successful, false otherwise
     */
    bool init(const std::string& config_dir);
    
    /**
     * Connect to the Snapcast server's chrony master
     * @param server_address Server hostname or IP address
     * @param port Server port (default: 323)
     * @return true if connection was successful, false otherwise
     */
    bool connectToServer(const std::string& server_address, uint16_t port = 323);
    
    /**
     * Check if connected to the Snapcast server's chrony master
     * @return True if connected
     */
    bool isConnected() const;
    
    /**
     * Check if chrony setup should be skipped
     * @param on_server Flag indicating if client is running on the same machine as server
     * @return True if chrony setup should be skipped
     */
    bool shouldSkipSetup(bool on_server) const;
    
    /**
     * Get chrony tracking information
     * @return Tracking information structure or empty if not available
     */
    std::optional<time_sync::ChronyTrackingInfo> getTrackingInfo() const override;
    
    /**
     * Get the current status of the chrony client
     * @return Status string with connection information
     */
    std::string getStatus() const override;
    
    /**
     * Get the server address we're connected to
     * @return Server address string
     */
    std::string getServerAddress() const;
    
    /**
     * Get the port we're connected to
     * @return Server port
     */
    uint16_t getServerPort() const;
    
    /**
     * Destructor
     */
    ~ChronyClient();
    
    /**
     * Disconnect from the chrony server
     */
    void disconnect();
    
private:
    ChronyClient() = default;
    ChronyClient(ChronyClient const&) = delete;
    void operator=(ChronyClient const&) = delete;
    
    // Configure chrony client with server settings
    bool configureClient(const std::string& server_address, uint16_t port);
    
    // Start chrony client
    bool startClient();
    
    // Stop chrony client
    void stopClient();
    
    // No monitoring thread - assume chrony works if configured properly
    
    std::string config_dir_;
    std::string server_address_;
    std::string resolved_address_; // Stores the resolved IP address from hostname
    uint16_t port_{323};
    
    std::atomic<bool> connected_{false};
    mutable std::mutex mutex_;
    
    // Process ID of the chrony client
    pid_t chrony_pid_{-1};
};

} // namespace snapclient
