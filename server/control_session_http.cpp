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

#include "control_session_http.hpp"
#include "aixlog.hpp"
#include "message/pcmChunk.h"
#include <boost/beast/http/file_body.hpp>
#include <iostream>

using namespace std;


ControlSessionHttp::ControlSessionHttp(ControlMessageReceiver* receiver, tcp::socket&& socket) : ControlSession(receiver), socket_(std::move(socket))
{
    LOG(DEBUG) << "ControlSessionHttp\n";
}


ControlSessionHttp::~ControlSessionHttp()
{
    LOG(DEBUG) << "ControlSessionHttp::~ControlSessionHttp()\n";
    stop();
}


void ControlSessionHttp::start()
{
    auto self = shared_from_this();
    http::async_read(socket_, buffer_, req_, [this, self](boost::system::error_code ec, std::size_t bytes) { on_read(ec, bytes); });
}


// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Body, class Allocator, class Send>
void ControlSessionHttp::handle_request(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send)
{
    // Returns a bad request response
    auto const bad_request = [&req](boost::beast::string_view why) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found = [&req](boost::beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + target.to_string() + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error = [&req](boost::beast::string_view what) {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + what.to_string() + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if (req.method() != http::verb::post)
        return send(bad_request("Unknown HTTP-method"));

    // Request path must be absolute and not contain "..".
    // if (req.target().empty() || req.target()[0] != '/' || req.target().find("..") != boost::beast::string_view::npos)
    //     return send(bad_request("Illegal request-target"));

    LOG(DEBUG) << "content type: " << req[beast::http::field::content_type] << "\n";
    LOG(DEBUG) << "body: " << req.body() << "\n";

    // TODO: error handling: bad request, ...
    string response = message_receiver_->onMessageReceived(this, req.body());

    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    res.body() = response; // R"({"jsonrpc": "2.0", "id": 1, "result": "stopped"})";
    res.prepare_payload();
    return send(std::move(res));
}

void ControlSessionHttp::on_read(beast::error_code ec, std::size_t bytes_transferred)
{
    // This means they closed the connection
    if (ec == http::error::end_of_stream)
    {
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        return;
    }

    // Handle the error, if any
    if (ec)
    {
        LOG(ERROR) << "ControlSessionHttp::on_read error: " << ec.message() << "\n";
        return;
    }

    // TODO: error handling
    // urls should be:
    //  http://<host>/snapcast/rpc
    //  ws://<host>/snapcast/ws or ws://<host>/snapcast/rpc?

    // See if it is a WebSocket Upgrade
    if (websocket::is_upgrade(req_))
    {
        // Create a WebSocket session by transferring the socket
        // std::make_shared<websocket_session>(std::move(socket_), state_)->run(std::move(req_));
        ws_ = make_unique<websocket::stream<beast::tcp_stream>>(std::move(socket_));
        auto self = shared_from_this();
        ws_->async_accept(req_, [this, self](beast::error_code ec) { on_accept_ws(ec); });
        LOG(DEBUG) << "websocket upgrade\n";
        return;
    }

    // Send the response
    handle_request(std::move(req_), [this](auto&& response) {
        // The lifetime of the message has to extend
        // for the duration of the async operation so
        // we use a shared_ptr to manage it.
        using response_type = typename std::decay<decltype(response)>::type;
        auto sp = std::make_shared<response_type>(std::forward<decltype(response)>(response));

        // Write the response
        auto self = this->shared_from_this();
        http::async_write(this->socket_, *sp, [this, self, sp](beast::error_code ec, std::size_t bytes) { this->on_write(ec, bytes, sp->need_eof()); });
    });
}


void ControlSessionHttp::on_write(beast::error_code ec, std::size_t, bool close)
{
    // Handle the error, if any
    if (ec)
    {
        LOG(ERROR) << "ControlSessionHttp::on_write, error: " << ec.message() << "\n";
        return;
    }

    if (close)
    {
        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        return;
    }

    // Clear contents of the request message,
    // otherwise the read behavior is undefined.
    req_ = {};

    // Read another request
    http::async_read(socket_, buffer_, req_, [ this, self = shared_from_this() ](beast::error_code ec, std::size_t bytes) { on_read(ec, bytes); });
}


void ControlSessionHttp::stop()
{
}


void ControlSessionHttp::sendAsync(const std::string& message)
{
    if (!ws_)
        return;

    auto self(shared_from_this());
    ws_->async_write(boost::asio::buffer(message), [this, self](std::error_code ec, std::size_t length) {
        if (ec)
        {
            LOG(ERROR) << "Error while writing to control socket: " << ec.message() << "\n";
        }
        else
        {
            LOG(DEBUG) << "Wrote " << length << " bytes to control socket\n";
        }
    });
}


bool ControlSessionHttp::send(const std::string& message)
{
    if (!ws_)
        return false;

    boost::system::error_code ec;
    ws_->write(boost::asio::buffer(message), ec);
    return !ec;
}

void ControlSessionHttp::on_accept_ws(beast::error_code ec)
{
    if (ec)
    {
        LOG(ERROR) << "ControlSessionWs::on_accept, error: " << ec.message() << "\n";
        return;
    }

    // Read a message
    do_read_ws();
}

void ControlSessionHttp::do_read_ws()
{
    // Read a message into our buffer
    auto self(shared_from_this());
    ws_->async_read(buffer_, [this, self](beast::error_code ec, std::size_t bytes_transferred) { on_read_ws(ec, bytes_transferred); });
}


void ControlSessionHttp::on_read_ws(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == websocket::error::closed)
        return;

    if (ec)
    {
        LOG(ERROR) << "ControlSessionHttp::on_read_ws error: " << ec.message() << "\n";
        return;
    }

    std::string line{boost::beast::buffers_to_string(buffer_.data())};
    if (!line.empty())
    {
        LOG(INFO) << "received: " << line << "\n";
        if ((message_receiver_ != nullptr) && !line.empty())
        {
            string response = message_receiver_->onMessageReceived(this, line);
            if (!response.empty())
            {
                sendAsync(response);
            }
        }
    }
    buffer_.consume(bytes_transferred);
    do_read_ws();
}
