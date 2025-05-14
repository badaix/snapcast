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

#include "time_sync.hpp"
#include "aixlog.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <optional>
#include <stdexcept>

namespace chrony {

static constexpr auto LOG_TAG = "ChronyBase";

/**
 * Base class for chrony functionality shared between client and server
 * 
 * This class provides common functionality for chrony operations,
 * including command execution, installation checking, and synchronization
 * verification.
 */
class ChronyBase {
public:
    virtual ~ChronyBase() = default;
    
    /**
     * Check if chrony is installed
     * @throws std::runtime_error if chrony is not installed
     */
    void verifyChronoInstalled();
    
    /**
     * Check if chrony is properly synchronized
     * @throws std::runtime_error if chrony is not synchronized
     */
    void checkSynchronization();
    
    /**
     * Get chrony tracking information
     * @return Tracking information structure or empty if not available
     */
    virtual std::optional<time_sync::ChronyTrackingInfo> getTrackingInfo() const;
    
    /**
     * Get current synchronization status as string
     * @return Status string
     */
    virtual std::string getStatus() const;
    
    /**
     * Execute a chronyc command with the -a flag
     * @param command The chronyc command to execute
     * @return True if command succeeded, false otherwise
     */
    bool executeChronycCommand(const std::string& command);
    
protected:
    /**
     * Execute a command and capture its output
     * @param cmd Command to execute
     * @return Command output as string
     */
    std::string execCommand(const std::string& cmd) const;
    
    /**
     * Check if chrony is installed
     * @return True if chrony is installed
     */
    bool isChronyInstalled() const;
    
    /**
     * Check if chrony is synchronized with a time source
     * @return True if synchronized
     */
    bool isSynchronized() const;
    
    std::string config_dir_;
    uint16_t port_{323};
    mutable std::mutex mutex_;
};

} // namespace chrony
