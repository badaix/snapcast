/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#ifndef WATCH_DOG_H
#define WATCH_DOG_H

#include <mutex>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>


class Watchdog;


class WatchdogListener
{
public:
	virtual void onTimeout(const Watchdog* watchdog, size_t ms) = 0;
};


/// Watchdog
class Watchdog
{
public:
	Watchdog(WatchdogListener* listener = nullptr);
	virtual ~Watchdog();

	void start(size_t timeoutMs);
	void stop();
	void trigger();

private:
	WatchdogListener* listener_;
	std::condition_variable cv_;
	std::mutex mtx_;
	std::unique_ptr<std::thread> thread_;
	size_t timeoutMs_;
	std::atomic<bool> active_;

	void worker();
};


#endif

