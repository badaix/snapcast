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

#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

#include <future>
#include <set>
#include <signal.h>


static std::future<int> install_signal_handler(std::set<int> signals)
{
    static std::promise<int> promise;
    std::future<int> future = promise.get_future();

    for (auto signal : signals)
    {
        ::signal(signal, [](int sig) {
            std::cerr << "signal: " << sig << "\n";
            try
            {
                promise.set_value(sig);
            }
            catch (const std::future_error&)
            {
            }
        });
    }
    return future;
}

#endif
