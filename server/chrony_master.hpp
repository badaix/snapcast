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

namespace snapserver {

/**
 * Manages a chrony master server for time synchronization
 * 
 * This class configures and manages a chrony server instance that acts as a 
 * master clock for Snapcast clients using direct chronyc commands.
 */
class ChronyMaster : public chrony::ChronyBase {
public:
    // Get the singleton instance
    static ChronyMaster& getInstance() {
        static ChronyMaster instance;
        return instance;
    }
    
    /**
     * Initialize the chrony master server
     * @param config_dir Directory to store configuration files
     * @param port Port to use for chrony server (default: 323)
     * @return true if initialization was successful, false otherwise
     */
    bool init(const std::string& config_dir, uint16_t port = 323);
    
    /**
     * Start the chrony master server
     * @return true if server started successfully, false otherwise
     */
    bool start();
    
    /**
     * Stop the chrony master server
     */
    void stop();
    
    /**
     * Check if the chrony master server is running
     * @return true if the server is running, false otherwise
     */
    bool isRunning() const;
    
    /**
     * Get the current server status
     * @return A string containing server status information
     */
    std::string getStatus() const override;
    
    /**
     * Get the server address for clients to connect to
     * @return Server address string
     */
    std::string getServerAddress() const;
    
    /**
     * Get the port the server is listening on
     * @return Server port
     */
    uint16_t getPort() const;
    
private:
    ChronyMaster() = default;
    ChronyMaster(ChronyMaster const&) = delete;
    void operator=(ChronyMaster const&) = delete;
    
    std::string server_address_;
    uint16_t port_{323};
    
    std::atomic<bool> running_{false};
};

} // namespace snapserver
