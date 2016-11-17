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

#include "watchdog.h"
#include <chrono>


using namespace std;


Watchdog::Watchdog(WatchdogListener* listener) : listener_(listener), thread_(nullptr), active_(false)
{
}


Watchdog::~Watchdog()
{
	stop();
}


void Watchdog::start(size_t timeoutMs)
{
	timeoutMs_ = timeoutMs;
	if (!thread_ || !active_)
	{
		active_ = true;
		thread_.reset(new thread(&Watchdog::worker, this));
	}
	else
		trigger();
}


void Watchdog::stop()
{
	active_ = false;
	trigger();
	if (thread_ && thread_->joinable())
		thread_->join();
	thread_ = nullptr;
}


void Watchdog::trigger()
{
//	std::unique_lock<std::mutex> lck(mtx_);
	cv_.notify_one();
}


void Watchdog::worker()
{
	while (active_)
	{
		std::unique_lock<std::mutex> lck(mtx_);
		if (cv_.wait_for(lck, std::chrono::milliseconds(timeoutMs_)) == std::cv_status::timeout)
		{
			if (listener_)
			{
				listener_->onTimeout(this, timeoutMs_);
				break;
			}
		}
	}
	active_ = false;
}

