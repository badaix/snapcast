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

#ifndef TCP_STREAM_H
#define TCP_STREAM_H

#include "pcm_stream.hpp"
#include <boost/asio.hpp>
#include <memory>

using boost::asio::ip::tcp;


/// Reads and decodes PCM data from a named pipe
/**
 * Reads PCM from a named pipe and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 */
class TcpStream : public PcmStream, public std::enable_shared_from_this<TcpStream>
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    TcpStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri);
    ~TcpStream() override;

    void start() override;

protected:
    void do_accept();
    void do_read();
    std::unique_ptr<tcp::acceptor> acceptor_;
    std::unique_ptr<tcp::socket> socket_;
    std::unique_ptr<msg::PcmChunk> chunk_;
    timeval tv_chunk_;
    bool first_;
    long nextTick_;
    boost::asio::deadline_timer timer_;
};


#endif
