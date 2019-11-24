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

#ifndef UDP_STREAM_HPP
#define UDP_STREAM_HPP

#include "pcm_stream.hpp"
#include <boost/asio.hpp>
#include <memory>

using boost::asio::ip::udp;


/// Reads and decodes PCM data from a named pipe
/**
 * Reads PCM from a named pipe and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 */
class UdpStream : public PcmStream, public std::enable_shared_from_this<UdpStream>
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    UdpStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri);
    ~UdpStream() override;

    void start() override;

protected:
    void do_read();
    std::unique_ptr<udp::socket> socket_;
    std::unique_ptr<msg::PcmChunk> chunk_;
    timeval tv_chunk_;
    udp::endpoint sender_endpoint_;
};


#endif
