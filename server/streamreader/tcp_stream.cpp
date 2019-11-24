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



TcpStream::TcpStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri) : PcmStream(pcmListener, ioc, uri), timer_(ioc)
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
    chunk_ = make_unique<msg::PcmChunk>(sampleFormat_, pcmReadMs_);
    LOG(DEBUG) << "Chunk size: " << chunk_->payloadSize << "\n";
}


TcpStream::~TcpStream()
{
}


void TcpStream::start()
{
    encoder_->init(this, sampleFormat_);
    active_ = true;
    do_accept();
}


void TcpStream::do_accept()
{
    auto self = shared_from_this();
    acceptor_->async_accept([this, self](boost::system::error_code ec, tcp::socket socket) {
        if (!ec)
        {
            LOG(DEBUG) << "New client connection\n";
            socket_ = make_unique<tcp::socket>(move(socket));
            tv_chunk_.tv_sec = 0;
            first_ = true;
            do_read();
        }
        else
        {
            LOG(ERROR) << "Accept failed: " << ec.message() << "\n";
        }
    });
}


void TcpStream::do_read()
{
    auto self = shared_from_this();
    chunk_->timestamp.sec = tv_chunk_.tv_sec;
    chunk_->timestamp.usec = tv_chunk_.tv_usec;
    boost::asio::async_read(*socket_, boost::asio::buffer(chunk_->payload, chunk_->payloadSize),
                            [this, self](boost::system::error_code ec, std::size_t length) mutable {
                                if (ec)
                                {
                                    LOG(ERROR) << "Error reading message: " << ec.message() << "\n";
                                    do_accept();
                                    return;
                                }
                                LOG(DEBUG) << "Read: " << length << " bytes\n";
                                if (first_)
                                {
                                    first_ = false;
                                    chronos::systemtimeofday(&tv_chunk_);
                                    chunk_->timestamp.sec = tv_chunk_.tv_sec;
                                    chunk_->timestamp.usec = tv_chunk_.tv_usec;
                                    tvEncodedChunk_ = tv_chunk_;
                                    nextTick_ = chronos::getTickCount();
                                }
                                encoder_->encode(chunk_.get());
                                nextTick_ += pcmReadMs_;
                                chronos::addUs(tv_chunk_, pcmReadMs_ * 1000);
                                long currentTick = chronos::getTickCount();

                                if (nextTick_ >= currentTick)
                                {
                                    setState(kPlaying);
                                    timer_.expires_from_now(boost::posix_time::milliseconds(nextTick_ - currentTick));
                                    timer_.async_wait([self, this](const boost::system::error_code& ec) {
                                        if (ec)
                                        {
                                            LOG(ERROR) << "Error during async wait: " << ec.message() << "\n";
                                        }
                                        else
                                        {
                                            do_read();
                                        }
                                    });
                                    return;
                                }
                                else
                                {
                                    chronos::systemtimeofday(&tv_chunk_);
                                    tvEncodedChunk_ = tv_chunk_;
                                    pcmListener_->onResync(this, currentTick - nextTick_);
                                    nextTick_ = currentTick;
                                    do_read();
                                }
                            });
}
