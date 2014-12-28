#include "log.h"
#include <iomanip>
#include <ctime>

Log::Log(std::string ident, int facility)
{
	facility_ = facility;
	priority_ = LOG_DEBUG;
	strncpy(ident_, ident.c_str(), sizeof(ident_));
	ident_[sizeof(ident_)-1] = '\0';

	openlog(ident_, LOG_PID, facility_);
}


std::string Log::LogPriorityToString(const LogPriority& priority)
{
	switch(priority)
	{
		case kLog: return "dbg";
		case kOut: return "out";
		case kErr: return "err";
		default: return std::to_string((int)priority);
	}
	return std::to_string((int)priority);
}


std::string Log::Timestamp()
{
    struct tm * dt;
    char buffer [30];
	std::time_t t = std::time(nullptr);
    dt = localtime(&t);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H-%M-%S", dt);
    return std::string(buffer);
}


int Log::sync()
{
	if (buffer_.length())
	{
//		if (priority_ == kLog)
//			std::cout << Timestamp() << " [" << LogPriorityToString(priority_) << "] " << buffer_.c_str();
//		else
		{
			std::cout << Timestamp() << " [" << LogPriorityToString((LogPriority)priority_) << "] " << buffer_.c_str();
			syslog(priority_, "%s", buffer_.c_str());
		}
		buffer_.erase();
		priority_ = LOG_DEBUG; // default to debug for each message
	}
	return 0;
}


int Log::overflow(int c)
{
	if (c != EOF)
	{
		buffer_ += static_cast<char>(c);
	}
	else
	{
		sync();
	}
	return c;
}

std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority)
{
	static_cast<Log *>(os.rdbuf())->priority_ = (int)log_priority;
//	if (log_priority == dbg)
//		os.flush();
	return os;
}


