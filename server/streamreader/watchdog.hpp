/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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

#pragma once


// 3rd party headers
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>

// standard headers


namespace streamreader
{

class Watchdog;


/// Watchdog
class Watchdog
{
public:
    /// Watchdog timeout handler
    using TimeoutHandler = std::function<void(std::chrono::milliseconds ms)>;

    /// c'tor
    explicit Watchdog(const boost::asio::any_io_executor& executor);
    /// d'tor
    virtual ~Watchdog();

    /// start the watchdog, call @p handler on @p timeout
    void start(const std::chrono::milliseconds& timeout, TimeoutHandler&& handler);
    /// stop the watchdog
    void stop();
    /// trigger the watchdog (reset timeout)
    void trigger();

private:
    boost::asio::steady_timer timer_;
    TimeoutHandler handler_;
    std::chrono::milliseconds timeout_ms_;
};

} // namespace streamreader
