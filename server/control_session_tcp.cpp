/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

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

// prototype/interface header file
#include "control_session_tcp.hpp"

// 3rd party headers
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

// local headers
#include "common/aixlog.hpp"
#include "common/message/pcm_chunk.hpp"


using namespace std;

static constexpr auto LOG_TAG = "ControlSessionTCP";

// https://stackoverflow.com/questions/7754695/boost-asio-async-write-how-to-not-interleaving-async-write-calls/7756894


ControlSessionTcp::ControlSessionTcp(ControlMessageReceiver* receiver, tcp::socket&& socket)
    : ControlSession(receiver), socket_(std::move(socket)), strand_(boost::asio::make_strand(socket_.get_executor()))
{
}


ControlSessionTcp::~ControlSessionTcp()
{
    LOG(DEBUG, LOG_TAG) << "ControlSessionTcp::~ControlSessionTcp()\n";
    stop();
}


void ControlSessionTcp::do_read()
{
    const std::string delimiter = "\n";
    boost::asio::async_read_until(socket_, streambuf_, delimiter,
                                  [this, self = shared_from_this(), delimiter](const std::error_code& ec, std::size_t bytes_transferred)
                                  {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Error while reading from control socket: " << ec.message() << "\n";
            return;
        }

        // Extract up to the first delimiter.
        std::string line{buffers_begin(streambuf_.data()), buffers_begin(streambuf_.data()) + bytes_transferred - delimiter.length()};
        if (!line.empty())
        {
            if (line.back() == '\r')
                line.resize(line.size() - 1);
            // LOG(DEBUG, LOG_TAG) << "received: " << line << "\n";
            if ((message_receiver_ != nullptr) && !line.empty())
            {
                message_receiver_->onMessageReceived(shared_from_this(), line,
                                                     [this](const std::string& response)
                                                     {
                    if (!response.empty())
                        sendAsync(response);
                });
            }
        }
        streambuf_.consume(bytes_transferred);
        do_read();
    });
}


void ControlSessionTcp::start()
{
    do_read();
}


void ControlSessionTcp::stop()
{
    LOG(DEBUG, LOG_TAG) << "ControlSession::stop\n";
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
        LOG(ERROR, LOG_TAG) << "Error in socket shutdown: " << ec.message() << "\n";
    socket_.close(ec);
    if (ec)
        LOG(ERROR, LOG_TAG) << "Error in socket close: " << ec.message() << "\n";
    LOG(DEBUG, LOG_TAG) << "ControlSession ControlSession stopped\n";
}


void ControlSessionTcp::sendAsync(const std::string& message)
{
    boost::asio::post(strand_,
                      [this, self = shared_from_this(), message]()
                      {
        messages_.emplace_back(message + "\r\n");
        if (messages_.size() > 1)
        {
            LOG(DEBUG, LOG_TAG) << "TCP session outstanding async_writes: " << messages_.size() << "\n";
            return;
        }
        send_next();
    });
}

void ControlSessionTcp::send_next()
{
    boost::asio::async_write(socket_, boost::asio::buffer(messages_.front()),
                             [this, self = shared_from_this()](std::error_code ec, std::size_t length)
                             {
        messages_.pop_front();
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Error while writing to control socket: " << ec.message() << "\n";
        }
        else
        {
            LOG(TRACE, LOG_TAG) << "Wrote " << length << " bytes to control socket\n";
        }
        if (!messages_.empty())
            send_next();
    });
}
