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
#include "common/aixlog.hpp"
#include "message/pcm_chunk.hpp"
#include <boost/beast/http/file_body.hpp>
#include <iostream>

using namespace std;

static constexpr const char* HTTP_SERVER_NAME = "Snapcast";

namespace
{
// Return a reasonable mime type based on the extension of a file.
boost::beast::string_view mime_type(boost::beast::string_view path)
{
    using boost::beast::iequals;
    auto const ext = [&path] {
        auto const pos = path.rfind(".");
        if (pos == boost::beast::string_view::npos)
            return boost::beast::string_view{};
        return path.substr(pos);
    }();
    if (iequals(ext, ".htm"))
        return "text/html";
    if (iequals(ext, ".html"))
        return "text/html";
    if (iequals(ext, ".php"))
        return "text/html";
    if (iequals(ext, ".css"))
        return "text/css";
    if (iequals(ext, ".txt"))
        return "text/plain";
    if (iequals(ext, ".js"))
        return "application/javascript";
    if (iequals(ext, ".json"))
        return "application/json";
    if (iequals(ext, ".xml"))
        return "application/xml";
    if (iequals(ext, ".swf"))
        return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))
        return "video/x-flv";
    if (iequals(ext, ".png"))
        return "image/png";
    if (iequals(ext, ".jpe"))
        return "image/jpeg";
    if (iequals(ext, ".jpeg"))
        return "image/jpeg";
    if (iequals(ext, ".jpg"))
        return "image/jpeg";
    if (iequals(ext, ".gif"))
        return "image/gif";
    if (iequals(ext, ".bmp"))
        return "image/bmp";
    if (iequals(ext, ".ico"))
        return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff"))
        return "image/tiff";
    if (iequals(ext, ".tif"))
        return "image/tiff";
    if (iequals(ext, ".svg"))
        return "image/svg+xml";
    if (iequals(ext, ".svgz"))
        return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string path_cat(boost::beast::string_view base, boost::beast::string_view path)
{
    if (base.empty())
        return path.to_string();
    std::string result = base.to_string();
    char constexpr path_separator = '/';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    return result;
}
} // namespace

ControlSessionHttp::ControlSessionHttp(ControlMessageReceiver* receiver, tcp::socket&& socket, const ServerSettings::HttpSettings& settings)
    : ControlSession(receiver), socket_(std::move(socket)), settings_(settings)
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
        // TODO: Server: Snapcast/VERSION
        res.set(http::field::server, HTTP_SERVER_NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found = [&req](boost::beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, HTTP_SERVER_NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + target.to_string() + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error = [&req](boost::beast::string_view what) {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, HTTP_SERVER_NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + what.to_string() + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if ((req.method() != http::verb::get) && (req.method() != http::verb::head) && (req.method() != http::verb::post))
        return send(bad_request("Unknown HTTP-method"));

    // handle json rpc requests
    if (req.method() == http::verb::post)
    {
        if (req.target() != "/jsonrpc")
            return send(bad_request("Illegal request-target"));

        string response = message_receiver_->onMessageReceived(this, req.body());
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, HTTP_SERVER_NAME);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());
        res.body() = response;
        res.prepare_payload();
        return send(std::move(res));
    }

    // Request path must be absolute and not contain "..".
    if (req.target().empty() || req.target()[0] != '/' || req.target().find("..") != beast::string_view::npos)
        return send(bad_request("Illegal request-target"));

    if (settings_.doc_root.empty())
        return send(not_found(req.target()));

    // Build the path to the requested file
    std::string path = path_cat(settings_.doc_root, req.target());
    if (req.target().back() == '/')
        path.append("index.html");

    LOG(DEBUG) << "path: " << path << "\n";
    // Attempt to open the file
    beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if (ec == boost::system::errc::no_such_file_or_directory)
        return send(not_found(req.target()));

    // Handle an unknown error
    if (ec)
        return send(server_error(ec.message()));

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if (req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, HTTP_SERVER_NAME);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    // Respond to GET request
    http::response<http::file_body> res{std::piecewise_construct, std::make_tuple(std::move(body)), std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::server, HTTP_SERVER_NAME);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
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

    LOG(DEBUG) << "read: " << bytes_transferred << ", method: " << req_.method_string() << ", content type: " << req_[beast::http::field::content_type]
               << ", target: " << req_.target() << ", body: " << req_.body() << "\n";

    // See if it is a WebSocket Upgrade
    if (websocket::is_upgrade(req_) && (req_.target() == "/jsonrpc"))
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
        LOG(DEBUG) << "received: " << line << "\n";
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
