/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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
#include "authinfo.hpp"

// 3rd party headers

// standard headers
#include <functional>
#include <memory>
#include <optional>
#include <string>


class ControlSession;
class StreamSession;

/// Interface: callback for a received message.
class ControlMessageReceiver
{
public:
    using ResponseHander = std::function<void(const std::string& response)>;
    // TODO: rename, error handling
    virtual void onMessageReceived(std::shared_ptr<ControlSession> session, const std::string& message, const ResponseHander& response_handler) = 0;
    virtual void onNewSession(std::shared_ptr<ControlSession> session) = 0;
    virtual void onNewSession(std::shared_ptr<StreamSession> session) = 0;
};


/// Endpoint for a connected control client.
/**
 * Endpoint for a connected control client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the ControlMessageReceiver callback
 */
class ControlSession : public std::enable_shared_from_this<ControlSession>
{
public:
    /// ctor. Received message from the client are passed to ControlMessageReceiver
    ControlSession(ControlMessageReceiver* receiver) : message_receiver_(receiver)
    {
    }
    virtual ~ControlSession() = default;
    virtual void start() = 0;
    virtual void stop() = 0;

    /// Sends a message to the client (asynchronous)
    virtual void sendAsync(const std::string& message) = 0;

    std::optional<AuthInfo> authinfo;

protected:
    ControlMessageReceiver* message_receiver_;
};
