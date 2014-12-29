#include "log.h"
#include <iomanip>
#include <ctime>

Log::Log(std::string ident, int facility)
{
	facility_ = facility;
	priority_ = kLogDebug;
	strncpy(ident_, ident.c_str(), sizeof(ident_));
	ident_[sizeof(ident_)-1] = '\0';

	openlog(ident_, LOG_PID, facility_);
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
		if (priority_ == kDbg)
#ifdef DEBUG_LOG
			std::cout << Timestamp() << " [dbg] " << buffer_.c_str() << std::flush;
#else
			;
#endif
		else if (priority_ == kOut)
			std::cout << Timestamp() << " [out] " << buffer_.c_str() << std::flush;
		else if (priority_ == kErr)
			std::cerr << Timestamp() << " [err] " << buffer_.c_str() << std::flush;
		else
		{
			std::cout << Timestamp() << " [" << std::to_string(priority_) << "] " << buffer_.c_str() << std::flush;
			syslog(priority_, "%s", buffer_.c_str());
		}
		buffer_.erase();
		priority_ = kLogDebug; // default to debug for each message
	}
	return 0;
}


int Log::overflow(int c)
{
	if (c != EOF)
	{
		buffer_ += static_cast<char>(c);
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


