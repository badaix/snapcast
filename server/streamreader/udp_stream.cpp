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
#include "udp_stream.hpp"


using namespace std;



UdpStream::UdpStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri) : PcmStream(pcmListener, ioc, uri)
{
    size_t port = 5004;
    try
    {
        port = cpt::stoi(uri_.getQuery("port", cpt::to_string(port)));
    }
    catch (...)
    {
    }

    LOG(INFO) << "UdpStream port: " << port << "\n";
    socket_ = make_unique<udp::socket>(ioc_, udp::endpoint(udp::v4(), port));
    chunk_ = make_unique<msg::PcmChunk>(sampleFormat_, pcmReadMs_);
    LOG(DEBUG) << "Chunk size: " << chunk_->payloadSize << "\n";
}


UdpStream::~UdpStream()
{
}


void UdpStream::start()
{
    encoder_->init(this, sampleFormat_);
    active_ = true;
    tv_chunk_.tv_sec = 0;
    do_read();
}


void UdpStream::do_read()
{
    auto self = shared_from_this();
    if (tv_chunk_.tv_sec != 0)
    {
        chunk_->timestamp.sec = tv_chunk_.tv_sec;
        chunk_->timestamp.usec = tv_chunk_.tv_usec;
        //        tvEncodedChunk_ = tv_chunk_;
        chronos::addUs(tv_chunk_, pcmReadMs_ * 1000);
        //        chronos::systemtimeofday(&tv_chunk_);
    }
    socket_->async_receive_from(boost::asio::buffer(chunk_->payload, chunk_->payloadSize), sender_endpoint_,
                                [this, self](boost::system::error_code ec, std::size_t bytes_recvd) {
                                    if (ec)
                                    {
                                        LOG(ERROR) << "Error reading message: " << ec.message() << "\n";
                                        return;
                                    }
                                    LOG(DEBUG) << "Read: " << bytes_recvd << " bytes\n";
                                    if (tv_chunk_.tv_sec == 0)
                                    {
                                        chronos::systemtimeofday(&tv_chunk_);
                                        chunk_->timestamp.sec = tv_chunk_.tv_sec;
                                        chunk_->timestamp.usec = tv_chunk_.tv_usec;
                                        tvEncodedChunk_ = tv_chunk_;
                                    }
                                    // encoder_->encode(chunk_.get());
                                    do_read();
                                });
}
