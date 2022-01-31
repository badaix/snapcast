/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

#ifndef WATCH_DOG_HPP
#define WATCH_DOG_HPP

// 3rd party headers
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <memory>


namespace streamreader
{

class Watchdog;


class WatchdogListener
{
public:
    virtual void onTimeout(const Watchdog& watchdog, std::chrono::milliseconds ms) = 0;
};


/// Watchdog
class Watchdog
{
public:
    Watchdog(const boost::asio::any_io_executor& executor, WatchdogListener* listener = nullptr);
    virtual ~Watchdog();

    void start(const std::chrono::milliseconds& timeout);
    void stop();
    void trigger();

private:
    boost::asio::steady_timer timer_;
    WatchdogListener* listener_;
    std::chrono::milliseconds timeout_ms_;
};

} // namespace streamreader

#endif
