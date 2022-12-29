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

// prototype/interface header file
#include "watchdog.hpp"

// local headers
#include "common/aixlog.hpp"

// 3rd party headers

// standard headers
#include <chrono>


static constexpr auto LOG_TAG = "Watchdog";


using namespace std;

namespace streamreader
{

Watchdog::Watchdog(const boost::asio::any_io_executor& executor, WatchdogListener* listener) : timer_(executor), listener_(listener)
{
}


Watchdog::~Watchdog()
{
    stop();
}


void Watchdog::start(const std::chrono::milliseconds& timeout)
{
    LOG(INFO, LOG_TAG) << "Starting watchdog, timeout: " << std::chrono::duration_cast<std::chrono::seconds>(timeout).count() << "s\n";
    timeout_ms_ = timeout;
    trigger();
}


void Watchdog::stop()
{
    timer_.cancel();
}


void Watchdog::trigger()
{
    timer_.cancel();
    timer_.expires_after(timeout_ms_);
    timer_.async_wait(
        [this](const boost::system::error_code& ec)
        {
        if (!ec)
        {
            LOG(INFO, LOG_TAG) << "Timed out: " << std::chrono::duration_cast<std::chrono::seconds>(timeout_ms_).count() << "s\n";
            listener_->onTimeout(*this, timeout_ms_);
        }
    });
}

} // namespace streamreader
