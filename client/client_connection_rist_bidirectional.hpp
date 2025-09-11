/***
    This file is part of snapcast
    Copyright (C) 2025  aanno <aannoaanno@gmail.com>

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

#ifdef HAS_LIBRIST

// local headers
#include "client_connection.hpp"
#include "client_settings.hpp"
#include "common/rist_transport.hpp"

// 3rd party headers
#include <boost/asio/ip/tcp.hpp>

// standard headers
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <unordered_map>

/// Simple RIST client connection using RistTransport
/**
 * RIST client connection using the clean RistTransport class.
 * Follows the testrist pattern with direct message handling.
 * Uses virtual ports for communication:
 * - Audio data: virtual port 1000 (received)
 * - Control messages: virtual port 2000 (received) 
 * - Backchannel: virtual port 3000 (sent)
 */
class ClientConnectionRistBidirectional : public ClientConnection, public RistTransportReceiver
{
public:
    /// c'tor
    ClientConnectionRistBidirectional(boost::asio::io_context& io_context, ClientSettings::Server server);
    /// d'tor
    virtual ~ClientConnectionRistBidirectional();

    void disconnect() override;
    std::string getMacAddress() override;
    void getNextMessage(const MessageHandler<msg::BaseMessage>& handler) override;
    
    // Override sendRequest to route requests through RIST backchannel
    void sendRequest(const msg::message_ptr& message, const chronos::usec& timeout, const MessageHandler<msg::BaseMessage>& handler) override;
    
    // Template version for typed responses
    template <typename Message>
    void sendRequest(const msg::message_ptr& message, const chronos::usec& timeout, const MessageHandler<Message>& handler)
    {
        sendRequest(message, timeout, [handler](const boost::system::error_code& ec, std::unique_ptr<msg::BaseMessage> response)
        {
            if (ec)
                handler(ec, nullptr);
            else if (auto casted_response = msg::message_cast<Message>(std::move(response)))
                handler(ec, std::move(casted_response));
            else
                handler(boost::system::errc::make_error_code(boost::system::errc::bad_message), nullptr);
        });
    }

    // RistTransportReceiver interface
    void onRistMessageReceived(const msg::BaseMessage& baseMessage, const std::string& payload, 
                              const char* payload_ptr /*, size_t payload_size, uint16_t vport */) override;
    void onRistClientConnected(const std::string& clientId) override;
    void onRistClientDisconnected(const std::string& clientId) override;
    
    // RIST parameter management
    void updateRistParameters(uint32_t recovery_length_min, uint32_t recovery_length_max);

protected:
    // non virtual variant of disconnect for use in d'tor
    void disconnectInternal();

private:
    boost::system::error_code doConnect(boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> endpoint) override;
    void write(boost::asio::streambuf& buffer, WriteHandler&& write_handler) override;

private:
    std::unique_ptr<RistTransport> rist_transport_;        ///< RIST transport instance
    
    MessageHandler<msg::BaseMessage> next_message_handler_; ///< Handler for next message
    std::mutex next_message_mutex_;                        ///< Protect next message handler
    
    // Request/response correlation for sendRequest()
    std::unordered_map<uint16_t, std::shared_ptr<PendingRequest>> pending_requests_; ///< Pending requests
    std::mutex pending_requests_mutex_;                     ///< Protect pending requests
    
    std::atomic<bool> running_;                            ///< Running flag
};

#endif // HAS_LIBRIST
