#ifndef LOG_H
#define LOG_H

#include <syslog.h>
#include <iostream>
#include <cstring>

#define logD std::clog << kLog
#define logO std::clog << kOut
#define logE std::clog << kErr
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
	kLog, kOut, kErr
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
	std::string Timestamp();
	std::string LogPriorityToString(const LogPriority& priority);
	std::string buffer_;
	int facility_;
	int priority_;
	char ident_[50];
};


#endif


