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
#include "control_session_http.hpp"

// standard headers
#include <iostream>

// 3rd party headers
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/file_body.hpp>

// local headers
#include "common/aixlog.hpp"
#include "common/message/pcm_chunk.hpp"
#include "common/utils/file_utils.hpp"
#include "control_session_ws.hpp"
#include "stream_session_ws.hpp"

using namespace std;
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>

static constexpr auto LOG_TAG = "ControlSessionHTTP";


static constexpr const char* HTTP_SERVER_NAME = "Snapcast";
static constexpr const char* UNCONFIGURED =
    "<html><head><title>Snapcast Default Page</title></head>"
    "<body>"
    "  <h1>Snapcast Default Page</h1>"
    "  <p>"
    "    This is the default welcome page used to test the correct operation of the Snapcast built-in webserver."
    "  </p>"
    "  <p>"
    "    This webserver is a websocket endpoint for control clients (ws://<i>host</i>:1780/jsonrpc) and streaming clients"
    "    (ws://<i>host</i>:1780/stream), but it can also host simple web pages. To serve a web page, you must configure the"
    "    document root in the snapserver configuration file <b>snapserver.conf</b>, usually located in"
    "    <b>/etc/snapserver.conf</b>"
    "  </p>"
    "  <p>"
    "    The Snapserver installation should include a copy of <a href=\"https://github.com/badaix/snapweb\">Snapweb</a>,"
    "    located in <b>/usr/share/snapserver/snapweb/</b><br>"
    "    To activate it, please configure the <b>doc_root</b> as follows, and restart Snapserver to activate the changes:"
    "  </p>"
    "  <pre>"
    "# HTTP RPC #####################################\n"
    "#\n"
    "[http]\n"
    "\n"
    "...\n"
    "\n"
    "# serve a website from the doc_root location\n"
    "doc_root = /usr/share/snapserver/snapweb/\n"
    "\n"
    "#\n"
    "################################################</pre>"
    "</body>"
    "</html>";

namespace
{
// Return a reasonable mime type based on the extension of a file.
boost::beast::string_view mime_type(boost::beast::string_view path)
{
    using boost::beast::iequals;
    auto const ext = [&path]
    {
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
        return std::string(path);
    std::string result = std::string(base);
    char constexpr path_separator = '/';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    return result;
}
} // namespace

ControlSessionHttp::ControlSessionHttp(ControlMessageReceiver* receiver, tcp::socket&& socket, const ServerSettings::Http& settings)
    : ControlSession(receiver), socket_(std::move(socket)), settings_(settings)
{
    LOG(DEBUG, LOG_TAG) << "ControlSessionHttp, Local IP: " << socket_.local_endpoint().address().to_string() << "\n";
}


ControlSessionHttp::~ControlSessionHttp()
{
    LOG(DEBUG, LOG_TAG) << "ControlSessionHttp::~ControlSessionHttp()\n";
    stop();
}


void ControlSessionHttp::start()
{
    http::async_read(socket_, buffer_, req_, [this, self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) { on_read(ec, bytes); });
}


// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Body, class Allocator, class Send>
void ControlSessionHttp::handle_request(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send)
{
    // Returns a bad request response
    auto const bad_request = [&req](boost::beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        // TODO: Server: Snapcast/VERSION
        res.set(http::field::server, HTTP_SERVER_NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found = [&req](boost::beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, HTTP_SERVER_NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a configuration help
    auto const unconfigured = [&req]()
    {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, HTTP_SERVER_NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = UNCONFIGURED;
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error = [&req](boost::beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, HTTP_SERVER_NAME);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
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

        std::string request = req.body();
        return message_receiver_->onMessageReceived(shared_from_this(), request,
                                                    [req = std::move(req), send = std::move(send)](const std::string& response)
                                                    {
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::server, HTTP_SERVER_NAME);
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = response;
            res.prepare_payload();
            return send(std::move(res));
        });
    }

    // Request path must be absolute and not contain "..".
    auto target = req.target();
    if (target.empty() || target[0] != '/' || target.find("..") != beast::string_view::npos)
        return send(bad_request("Illegal request-target"));

    static string image_cache_target = "/__image_cache?name=";
    auto pos = target.find(image_cache_target);
    if (pos != std::string::npos)
    {
        pos += image_cache_target.size();
        target = target.substr(pos);
        auto image = settings_.image_cache.getImage(std::string(target));
        LOG(DEBUG, LOG_TAG) << "image cache: " << target << ", found: " << image.has_value() << "\n";
        if (image.has_value())
        {
            http::response<http::buffer_body> res{http::status::ok, req.version()};
            res.body().data = image.value().data();
            const auto size = image.value().size();
            res.body().size = size;
            res.body().more = false;
            res.set(http::field::server, HTTP_SERVER_NAME);
            res.set(http::field::content_type, mime_type(target));
            res.content_length(size);
            res.keep_alive(req.keep_alive());
            return send(std::move(res));
        }
        return send(not_found(req.target()));
    }

    // Build the path to the requested file
    std::string path = path_cat(settings_.doc_root, target);
    if (req.target().back() == '/')
        path.append("index.html");

    if (settings_.doc_root.empty())
    {
        static constexpr auto default_page = "/usr/share/snapserver/index.html";
        if (utils::file::exists(default_page))
            path = default_page;
        else
            return send(unconfigured());
    }

    LOG(DEBUG, LOG_TAG) << "path: " << path << "\n";
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
        LOG(ERROR, LOG_TAG) << "ControlSessionHttp::on_read error: " << ec.message() << "\n";
        return;
    }

    LOG(DEBUG, LOG_TAG) << "read: " << bytes_transferred << ", method: " << req_.method_string() << ", content type: " << req_[beast::http::field::content_type]
                        << ", target: " << req_.target() << ", body: " << req_.body() << "\n";

    // See if it is a WebSocket Upgrade
    if (websocket::is_upgrade(req_))
    {
        LOG(DEBUG, LOG_TAG) << "websocket upgrade, target: " << req_.target() << "\n";
        if (req_.target() == "/jsonrpc")
        {
            // Create a WebSocket session by transferring the socket
            // std::make_shared<websocket_session>(std::move(socket_), state_)->run(std::move(req_));
            auto ws = std::make_shared<websocket::stream<beast::tcp_stream>>(std::move(socket_));
            ws->async_accept(req_,
                             [this, ws, self = shared_from_this()](beast::error_code ec)
                             {
                if (ec)
                {
                    LOG(ERROR, LOG_TAG) << "Error during WebSocket handshake (control): " << ec.message() << "\n";
                }
                else
                {
                    auto ws_session = make_shared<ControlSessionWebsocket>(message_receiver_, std::move(*ws));
                    message_receiver_->onNewSession(std::move(ws_session));
                }
            });
        }
        else if (req_.target() == "/stream")
        {
            // Create a WebSocket session by transferring the socket
            // std::make_shared<websocket_session>(std::move(socket_), state_)->run(std::move(req_));
            auto ws = std::make_shared<websocket::stream<beast::tcp_stream>>(std::move(socket_));
            ws->async_accept(req_,
                             [this, ws, self = shared_from_this()](beast::error_code ec)
                             {
                if (ec)
                {
                    LOG(ERROR, LOG_TAG) << "Error during WebSocket handshake (stream): " << ec.message() << "\n";
                }
                else
                {
                    auto ws_session = make_shared<StreamSessionWebsocket>(nullptr, std::move(*ws));
                    message_receiver_->onNewSession(std::move(ws_session));
                }
            });
        }
        return;
    }

    // Send the response
    handle_request(std::move(req_),
                   [this](auto&& response)
                   {
        // The lifetime of the message has to extend
        // for the duration of the async operation so
        // we use a shared_ptr to manage it.
        using response_type = typename std::decay<decltype(response)>::type;
        auto sp = std::make_shared<response_type>(std::forward<decltype(response)>(response));

        // Write the response
        http::async_write(this->socket_, *sp,
                          [this, self = this->shared_from_this(), sp](beast::error_code ec, std::size_t bytes) { this->on_write(ec, bytes, sp->need_eof()); });
    });
}


void ControlSessionHttp::on_write(beast::error_code ec, std::size_t bytes, bool close)
{
    std::ignore = bytes;

    // Handle the error, if any
    if (ec)
    {
        LOG(ERROR, LOG_TAG) << "ControlSessionHttp::on_write, error: " << ec.message() << "\n";
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
    http::async_read(socket_, buffer_, req_, [this, self = shared_from_this()](beast::error_code ec, std::size_t bytes) { on_read(ec, bytes); });
}


void ControlSessionHttp::stop()
{
}


void ControlSessionHttp::sendAsync(const std::string& /*message*/)
{
}
