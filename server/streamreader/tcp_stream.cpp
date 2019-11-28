/***
    This file is part of snapcast
    Copyright (C) 2014-2019  Johannes Pohl

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

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"
#include "encoder/encoder_factory.hpp"
#include "tcp_stream.hpp"


using namespace std;



TcpStream::TcpStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri)
    : AsioStream<tcp::socket>(pcmListener, ioc, uri), reconnect_timer_(ioc)
{
    host_ = uri_.host;
    auto host_port = utils::string::split(host_, ':');
    port_ = 4953;
    if (host_port.size() == 2)
    {
        host_ = host_port[0];
        port_ = cpt::stoi(host_port[1], port_);
    }

    auto mode = uri_.getQuery("mode", "server");
    if (mode == "server")
        is_server_ = true;
    else if (mode == "client")
        is_server_ = false;
    else
        throw SnapException("mode must be 'client' or 'server'");

    port_ = cpt::stoi(uri_.getQuery("port", cpt::to_string(port_)), port_);

    LOG(INFO) << "TcpStream host: " << host_ << ", port: " << port_ << ", is server: " << is_server_ << "\n";
    if (is_server_)
        acceptor_ = make_unique<tcp::acceptor>(ioc_, tcp::endpoint(boost::asio::ip::address::from_string(host_), port_));
}


void TcpStream::connect()
{
    auto self = shared_from_this();

    if (is_server_)
    {
        acceptor_->async_accept([this, self](boost::system::error_code ec, tcp::socket socket) {
            if (!ec)
            {
                LOG(DEBUG) << "New client connection\n";
                stream_ = make_unique<tcp::socket>(move(socket));
                on_connect();
            }
            else
            {
                LOG(ERROR) << "Accept failed: " << ec.message() << "\n";
            }
        });
    }
    else
    {
        stream_ = make_unique<tcp::socket>(ioc_);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(host_), port_);
        stream_->async_connect(endpoint, [self, this](const boost::system::error_code& ec) {
            if (!ec)
            {
                LOG(DEBUG) << "Connected\n";
                on_connect();
            }
            else
            {
                LOG(DEBUG) << "Connect failed: " << ec.message() << "\n";
                reconnect_timer_.expires_from_now(boost::posix_time::milliseconds(1000));
                reconnect_timer_.async_wait([self, this](const boost::system::error_code& ec) {
                    if (!ec)
                        connect();
                });
            }
        });
    }
}


void TcpStream::disconnect()
{
    if (stream_)
        stream_->close();
}
