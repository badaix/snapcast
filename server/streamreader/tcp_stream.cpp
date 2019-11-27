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
#include "encoder/encoder_factory.hpp"
#include "tcp_stream.hpp"


using namespace std;



TcpStream::TcpStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri) : AsioStream<tcp::socket>(pcmListener, ioc, uri)
{
    size_t port = 4953;
    try
    {
        port = cpt::stoi(uri_.getQuery("port", cpt::to_string(port)));
    }
    catch (...)
    {
    }

    LOG(INFO) << "TcpStream port: " << port << "\n";
    acceptor_ = make_unique<tcp::acceptor>(ioc_, tcp::endpoint(tcp::v4(), port));
}


void TcpStream::connect()
{
    auto self = shared_from_this();
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


void TcpStream::disconnect()
{
    stream_->close();
}
