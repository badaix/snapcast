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

#ifdef HAS_LIBRIST

// prototype/interface header file
#include "client_connection_rist_bidirectional.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/utils.hpp"
#include "common/message/factory.hpp"

// standard headers
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

static constexpr auto LOG_TAG = "ConnectionRISTBi";

ClientConnectionRistBidirectional::ClientConnectionRistBidirectional(boost::asio::io_context& io_context, ClientSettings::Server server)
    : ClientConnection(io_context, std::move(server)), running_(false)
{
    LOG(INFO, LOG_TAG) << "Creating RIST client connection with RistTransport\n";
}

ClientConnectionRistBidirectional::~ClientConnectionRistBidirectional()
{
    LOG(DEBUG, LOG_TAG) << "~ClientConnectionRistBidirectional\n";
    disconnectInternal();
}

boost::system::error_code ClientConnectionRistBidirectional::doConnect(boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> endpoint)
{
    LOG(INFO, LOG_TAG) << "Connecting to RIST server: " << endpoint.address().to_string() << ":" << endpoint.port() << "\n";

    // Create and configure RIST transport in CLIENT mode
    rist_transport_ = std::make_unique<RistTransport>(RistTransport::Mode::CLIENT, this);
    
    if (!rist_transport_->configureClient(endpoint.address().to_string(), endpoint.port()))
    {
        LOG(ERROR, LOG_TAG) << "Failed to configure RIST client\n";
        return boost::system::errc::make_error_code(boost::system::errc::connection_refused);
    }

    if (!rist_transport_->start())
    {
        LOG(ERROR, LOG_TAG) << "Failed to start RIST client\n";
        return boost::system::errc::make_error_code(boost::system::errc::connection_refused);
    }

    running_ = true;
    LOG(INFO, LOG_TAG) << "RIST client connection established\n";
    return {};
}

void ClientConnectionRistBidirectional::disconnect()
{
    disconnectInternal();
}

void ClientConnectionRistBidirectional::disconnectInternal()
{
    LOG(DEBUG, LOG_TAG) << "Disconnecting RIST client\n";
    
    running_ = false;

    if (rist_transport_)
    {
        rist_transport_->stop();
        rist_transport_.reset();
    }

    LOG(DEBUG, LOG_TAG) << "RIST client disconnected\n";
}

std::string ClientConnectionRistBidirectional::getMacAddress()
{
    // For RIST, we don't have a socket, so create a temporary one to get MAC
    int temp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    std::string mac = ::getMacAddress(temp_socket);
    if (temp_socket >= 0)
        close(temp_socket);
    
    if (mac.empty())
        mac = "00:00:00:00:00:00";
    LOG(DEBUG, LOG_TAG) << "My MAC: \"" << mac << "\", host: " << server_.host << "\n";
    return mac;
}

void ClientConnectionRistBidirectional::getNextMessage(const MessageHandler<msg::BaseMessage>& handler)
{
    // LOG(TRACE, LOG_TAG) << "getNextMessage called\n";
    
    // Store the handler for when we receive the next message
    std::lock_guard<std::mutex> lock(next_message_mutex_);
    next_message_handler_ = handler;
}

void ClientConnectionRistBidirectional::write(boost::asio::streambuf& buffer, WriteHandler&& write_handler)
{
    if (!rist_transport_)
    {
        LOG(ERROR, LOG_TAG) << "Cannot send data - RIST transport not available\n";
        write_handler(boost::system::error_code(boost::asio::error::not_connected), 0);
        return;
    }

    if (!running_)
    {
        LOG(ERROR, LOG_TAG) << "Cannot send data - RIST client not connected\n";
        write_handler(boost::system::error_code(boost::asio::error::not_connected), 0);
        return;
    }

    // Get data from streambuf
    // buffer.data() returns const_buffers_1 (a buffer sequence)
    auto buffer_sequence = buffer.data();
    // Get raw pointer to the underlying data
    const char* data_ptr = static_cast<const char*>(buffer_sequence.data());
    auto data_size = buffer.size();

    if (data_size == 0)
    {
        LOG(WARNING, LOG_TAG) << "Attempting to send empty buffer\n";
        write_handler(boost::system::error_code(), 0);
        return;
    }

    try {
        // Parse message header to get type and ID for logging (but don't re-serialize)
        msg::BaseMessage base_message;
        base_message.deserialize(const_cast<char*>(data_ptr));
        
        LOG(DEBUG, LOG_TAG) << "Sending message type " << base_message.type << " (id=" << base_message.id << ") via RIST backchannel\n";

        // Send raw data directly via RIST transport's sendRawData method
        bool success = rist_transport_->sendRawData(RistTransport::VPORT_BACKCHANNEL, data_ptr, data_size);
        if (success)
        {
            LOG(DEBUG, LOG_TAG) << "Successfully sent " << data_size << " bytes via RIST backchannel\n";
            write_handler(boost::system::error_code(), data_size);
        }
        else
        {
            LOG(ERROR, LOG_TAG) << "Failed to send " << data_size << " bytes via RIST backchannel\n";
            write_handler(boost::system::error_code(boost::asio::error::broken_pipe), 0);
        }
    }
    catch (const std::exception& e) {
        LOG(ERROR, LOG_TAG) << "Exception while sending message via RIST: " << e.what() << "\n";
        write_handler(boost::system::error_code(boost::asio::error::invalid_argument), 0);
    }
}

void ClientConnectionRistBidirectional::sendRequest(const msg::message_ptr& message, const chronos::usec& timeout, const MessageHandler<msg::BaseMessage>& handler)
{
    if (!rist_transport_)
    {
        LOG(ERROR, LOG_TAG) << "❌ RIST sendRequest failed: transport not available\n";
        handler(boost::system::errc::make_error_code(boost::system::errc::not_connected), nullptr);
        return;
    }
    
    // Assign proper request ID (copied from base ClientConnection::sendRequest)
    static constexpr uint16_t max_req_id = 10000;
    if (++reqId_ >= max_req_id)
        reqId_ = 1;
    message->id = reqId_;
    
    // LOG(TRACE, LOG_TAG) << "🚀 RIST sendRequest: message type=" << message->type << " (id=" << message->id << ") via backchannel\n";
    
    // Store the pending request for correlation with response
    auto request = std::make_shared<PendingRequest>(strand_, message->id, handler);
    {
        std::lock_guard<std::mutex> lock(pending_requests_mutex_);
        pending_requests_[message->id] = request;
    }
    
    // Send message via RIST backchannel (vport 3000)
    if (rist_transport_->sendMessage(RistTransport::VPORT_BACKCHANNEL, *message))
    {
        // LOG(TRACE, LOG_TAG) << "✅ RIST sendRequest: sent message type=" << message->type << " (id=" << message->id << ")\n";
        // Start timeout timer
        request->startTimer(timeout);
    }
    else
    {
        LOG(ERROR, LOG_TAG) << "❌ RIST sendRequest: failed to send message type=" << message->type << " (id=" << message->id << ")\n";
        // Remove from pending requests
        {
            std::lock_guard<std::mutex> lock(pending_requests_mutex_);
            pending_requests_.erase(message->id);
        }
        handler(boost::system::errc::make_error_code(boost::system::errc::io_error), nullptr);
    }
}

// RistTransportReceiver interface
void ClientConnectionRistBidirectional::onRistMessageReceived(const msg::BaseMessage& baseMessage, const std::string& payload, 
                                                            const char* payload_ptr /*, size_t payload_size, uint16_t vport */)
{
    // LOG(TRACE, LOG_TAG) << "RIST message received: type=" << baseMessage.type << " (id=" << baseMessage.id << "), vport=" << vport << "\n";
    
    try
    {
        // Set base_message_ to match what TCP client does - this is used by messageReceived() for correlation
        base_message_ = baseMessage;
        // Set received timestamp to current time, exactly like TCP implementation does
        tv now;
        base_message_.received = now;
        
        // Create message from received data using the updated base_message_
        // For zero-copy optimization: use raw pointer if payload is empty (large audio data)
        const char* data_ptr = payload.empty() ? payload_ptr : payload.data();
        /*
        if (payload.empty()) {
            LOG(TRACE, LOG_TAG) << "🎵 CLIENT ZERO-COPY: Using direct pointer for message type=" << baseMessage.type << " (" << payload_size << " bytes)\n";
        } else {
            LOG(TRACE, LOG_TAG) << "📝 CLIENT COPY: Using copied data for message type=" << baseMessage.type << " (" << payload.size() << " bytes)\n";
        }
        */
        auto message = msg::factory::createMessage(base_message_, const_cast<char*>(data_ptr));
        if (!message)
        {
            LOG(ERROR, LOG_TAG) << "Failed to create message from RIST data\n";
            return;
        }
        
        // Check if this is a response to a pending request (has refersTo field)
        if (message->refersTo != 0)
        {
            // LOG(TRACE, LOG_TAG) << "📨 RIST response received: type=" << message->type << " (id=" << message->id << ") refersTo=" << message->refersTo << "\n";
            
            // Find and handle pending request
            std::shared_ptr<PendingRequest> request;
            {
                std::lock_guard<std::mutex> lock(pending_requests_mutex_);
                auto it = pending_requests_.find(message->refersTo);
                if (it != pending_requests_.end())
                {
                    request = it->second;
                    pending_requests_.erase(it);
                }
            }
            
            if (request)
            {
                // LOG(TRACE, LOG_TAG) << "✅ RIST response: delivering to pending request (id=" << message->refersTo << ")\n";
                request->setValue(std::move(message));
                return; // Response handled, don't pass to normal message handler
            }
            else
            {
                LOG(WARNING, LOG_TAG) << "⚠️ RIST response: no pending request found for refersTo=" << message->refersTo << "\n";
                // Fall through to normal message handling
            }
        }
        
        // Use the normal handler mechanism for non-response messages
        MessageHandler<msg::BaseMessage> handler;
        {
            std::lock_guard<std::mutex> lock(next_message_mutex_);
            handler = next_message_handler_;
            next_message_handler_ = nullptr;
        }

        if (handler)
        {
            // LOG(TRACE, LOG_TAG) << "📥 RIST message: processing type=" << message->type << " through normal pipeline\n";
            messageReceived(std::move(message), handler);
        }
        else
        {
            LOG(WARNING, LOG_TAG) << "No handler available for RIST message type: " << message->type << "\n";
        }
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Error processing RIST message: " << e.what() << "\n";
    }
}

void ClientConnectionRistBidirectional::onRistClientConnected(const std::string& clientId)
{
    LOG(INFO, LOG_TAG) << "RIST connection established: " << clientId << "\n";
}

void ClientConnectionRistBidirectional::onRistClientDisconnected(const std::string& clientId)
{
    LOG(INFO, LOG_TAG) << "RIST connection lost: " << clientId << "\n";
}

void ClientConnectionRistBidirectional::updateRistParameters(uint32_t recovery_length_min, uint32_t recovery_length_max)
{
    if (rist_transport_)
    {
        LOG(INFO, LOG_TAG) << "Updating RIST parameters from server: recovery_length_min=" << recovery_length_min 
                           << ", recovery_length_max=" << recovery_length_max << "\n";
        rist_transport_->updateClientParameters(recovery_length_min, recovery_length_max);
    }
    else
    {
        LOG(WARNING, LOG_TAG) << "Cannot update RIST parameters - transport not available\n";
    }
}

#endif // HAS_LIBRIST
