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
#include "server_settings.hpp"

// 3rd party headers

// standard headers
#include <functional>
#include <memory>
#include <string>


class ControlSession;
class StreamSession;

/// Interface: callback for a received message.
class ControlMessageReceiver
{
public:
    /// Response callback function for requests
    using ResponseHandler = std::function<void(const std::string& response)>;
    // TODO: rename, error handling
    /// Called when a comtrol message @p message is received by @p session, response is written to @p response_handler
    virtual void onMessageReceived(std::shared_ptr<ControlSession> session, const std::string& message, const ResponseHandler& response_handler) = 0;
    /// Called when a comtrol session is created
    virtual void onNewSession(std::shared_ptr<ControlSession> session) = 0;
    /// Called when a stream session is created
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
    ControlSession(ControlMessageReceiver* receiver, const ServerSettings& settings) : authinfo(settings), message_receiver_(receiver)
    {
    }
    virtual ~ControlSession() = default;
    /// Start the control session
    virtual void start() = 0;
    /// Stop the control session
    virtual void stop() = 0;

    /// Sends a message to the client (asynchronous)
    virtual void sendAsync(const std::string& message) = 0;

    /// Authentication info attached to this session
    AuthInfo authinfo;

protected:
    /// The control message receiver
    ControlMessageReceiver* message_receiver_;
};
