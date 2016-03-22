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


#ifndef LOG_H
#define LOG_H

#include <syslog.h>
#include <iostream>
#include <cstring>
#include <sstream>

#define logD std::clog << kDbg
#define logO std::clog << kOut
#define logE std::clog << kErr
#define logState std::clog << kState
#define logS(P) std::clog << P
#define log logO

enum LogPriority
{
	kLogEmerg   = LOG_EMERG,   // system is unusable
	kLogAlert   = LOG_ALERT,   // action must be taken immediately
	kLogCrit    = LOG_CRIT,    // critical conditions
	kLogErr     = LOG_ERR,     // error conditions
	kLogWarning = LOG_WARNING, // warning conditions
	kLogNotice  = LOG_NOTICE,  // normal, but significant, condition
	kLogInfo    = LOG_INFO,    // informational message
	kLogDebug   = LOG_DEBUG,   // debug-level message
	kDbg, kOut, kState, kErr
};

std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority);

class Log : public std::basic_streambuf<char, std::char_traits<char> >
{
public:
	explicit Log(std::string ident, int facility);

protected:
	int sync();
	int overflow(int c);

private:
	friend std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority);
	std::string toString(LogPriority logPriority) const;
	std::string Timestamp() const;
	std::stringstream buffer_;
	int facility_;
	LogPriority priority_;
	char ident_[50];
};


#endif


