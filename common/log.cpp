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

#include "log.h"
#include <iomanip>
#include <ctime>
#include <sstream>
#include <cstdio>


Log::Log(std::string ident, int facility)
{
	facility_ = facility;
	priority_ = kLogDebug;
	strncpy(ident_, ident.c_str(), sizeof(ident_));
	ident_[sizeof(ident_)-1] = '\0';

	openlog(ident_, LOG_PID, facility_);
}


std::string Log::Timestamp() const
{
	struct tm * dt;
	char buffer [30];
	std::time_t t = std::time(nullptr);
	dt = localtime(&t);
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H-%M-%S", dt);
	return std::string(buffer);
}


std::string Log::toString(LogPriority logPriority) const
{
	switch (logPriority)
	{
		case kDbg:
			return "dbg";
		case kOut:
			return "out";
		case kState:
			return "state";
		case kErr:
			return "err";

		case kLogEmerg:
			return "Emerg";
		case kLogAlert:
			return "Alert";
		case kLogCrit:
			return "Crit";
		case kLogErr:
			return "Err";
		case kLogWarning:
			return "Warning";
		case kLogNotice:
			return "Notice";
		case kLogInfo:
			return "Info";
		case kLogDebug:
			return "Debug";
		default:
			std::stringstream ss;
			ss << logPriority;
			return ss.str();
	}
}


int Log::sync()
{
	if (buffer_.str().length())
	{
		if (priority_ == kDbg)
#ifdef DEBUG_LOG
			std::cout << Timestamp() << " [dbg] " << buffer_.str() << std::flush;
#else
			;
#endif
		else if ((priority_ == kOut) || (priority_ == kState) || (priority_ == kErr))
			std::cout << Timestamp() << " [" << toString(priority_) << "] " << buffer_.str() << std::flush;
		else
		{
			std::cout << Timestamp() << " [" << toString(priority_) << "] " << buffer_.str() << std::flush;
			syslog(priority_, "%s", buffer_.str().c_str());
		}
		buffer_.str("");
		buffer_.clear();
		priority_ = kLogDebug; // default to debug for each message
	}
	return 0;
}


int Log::overflow(int c)
{
	if (c != EOF)
	{
		buffer_ << static_cast<char>(c);
		if (c == '\n')
			sync();
	}
	else
	{
		sync();
	}
	return c;
}


std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority)
{
	static_cast<Log*>(os.rdbuf())->priority_ = log_priority;
	return os;
}


