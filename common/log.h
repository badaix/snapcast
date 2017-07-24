/***
      __   __  _  _  __     __    ___ 
     / _\ (  )( \/ )(  )   /  \  / __)
    /    \ )(  )  ( / (_/\(  O )( (_ \
    \_/\_/(__)(_/\_)\____/ \__/  \___/
    version 0.6.0
    https://github.com/badaix/aixlog

    This file is part of aixlog
    Copyright (C) 2017  Johannes Pohl

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

/// inspired by "eater": 
/// https://stackoverflow.com/questions/2638654/redirect-c-stdclog-to-syslog-on-unix

/// TODO: add global log level


#ifndef AIX_LOG_HPP
#define AIX_LOG_HPP

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#ifdef __APPLE__
#include <os/log.h>
#endif
#ifdef _WIN32
#include <Windows.h>
#else
#include <syslog.h>
#endif


/// Internal helper defines
#define LOG_WO_TAG(P) std::clog << (LogPriority)P << Tag(__func__) << LogType::normal
#define SLOG_WO_TAG(P) std::clog << (LogPriority)P << Tag(__func__) << LogType::special

#define LOG_TAG(P, T) std::clog << (LogPriority)P << Tag(T) << LogType::normal
#define SLOG_TAG(P, T) std::clog << (LogPriority)P << Tag(T) << LogType::special

#define LOG_X(x,P,T,FUNC, ...)  FUNC
#define SLOG_X(x,P,T,FUNC, ...)  FUNC


/// External logger defines
#define LOG(...) LOG_X(,##__VA_ARGS__, LOG_TAG(__VA_ARGS__), LOG_WO_TAG(__VA_ARGS__))
#define SLOG(...) SLOG_X(,##__VA_ARGS__, SLOG_TAG(__VA_ARGS__), SLOG_WO_TAG(__VA_ARGS__))

#define FUNC __func__
#define TAG Tag
#define COND Conditional


enum class LogType
{
	normal,
	special
};


enum Priority
{
	EMERG = 0,   // 0 system is unusable
	ALERT = 1,   // 1 action must be taken immediately
	CRIT = 2,    // 2 critical conditions
	ERROR = 3,     // 3 error conditions
	WARNING = 4, // 4 warning conditions
	NOTICE = 5,  // 5 normal, but significant, condition
	INFO = 6,    // 6 informational message
	DEBUG = 7    // 7 debug-level message
};


enum class LogPriority : std::int8_t
{
	emerg   = EMERG,
	alert   = ALERT,
	critical= CRIT,
	error   = ERROR,
	warning = WARNING,
	notice  = NOTICE,
	info    = INFO,
	debug   = DEBUG
};



enum class Color
{
    none = 0,
    black = 1,
	red = 2,
	green = 3,
	yellow = 4,
	blue = 5,
	magenta = 6,
	cyan = 7,
	white = 8
};



struct LogColor
{
	LogColor(Color foreground = Color::none, Color background = Color::none) :
		foreground(foreground),
		background(background)
	{
	}

	Color foreground;
	Color background;
};



struct Conditional
{
	Conditional() : Conditional(true)
	{
	}

	Conditional(bool value) : is_true_(value)
	{
	}

	void set(bool value)
	{
		is_true_ = value;
	}

	bool is_true() const
	{
		return is_true_;
	}

private:
	bool is_true_;
};



struct Tag
{
	Tag(std::nullptr_t) : tag(""), is_null(true)
	{
	}

	Tag() : Tag(nullptr)
	{
	}

	Tag(const std::string& tag) : tag(tag), is_null(false)
	{
	}

	virtual explicit operator bool() const
	{
		return !is_null;
	}

	std::string tag;

private:
	bool is_null;
};


typedef std::chrono::time_point<std::chrono::system_clock> time_point_sys_clock;

struct LogSink
{
	enum class Type
	{
		normal,
		special,
		all
	};

	LogSink(LogPriority priority, Type type) : priority(priority), sink_type_(type)
	{
	}

	virtual ~LogSink()
	{
	}

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const = 0;
	virtual Type get_type() const
	{
		return sink_type_;
	}

	virtual LogSink& set_type(Type sink_type)
	{
		sink_type_ = sink_type;
		return *this;
	}

	LogPriority priority;

protected:
	Type sink_type_;
};



static std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority);
static std::ostream& operator<< (std::ostream& os, const LogType& log_type);
static std::ostream& operator<< (std::ostream& os, const Tag& tag);
static std::ostream& operator<< (std::ostream& os, const Conditional& conditional);

typedef std::shared_ptr<LogSink> log_sink_ptr;


class Log : public std::basic_streambuf<char, std::char_traits<char> >
{
public:
	static Log& instance()
	{
		static Log instance_;
		return instance_;
	}

	/// Without "init" every LOG(X) will simply go to clog
	static void init(const std::vector<log_sink_ptr> log_sinks = {})
	{
		for (auto sink: log_sinks)
			Log::instance().add_logsink(sink);

		std::clog.rdbuf(&Log::instance());
	}

	void add_logsink(log_sink_ptr sink)
	{
		logSinks.push_back(sink);
	}

	void remove_logsink(log_sink_ptr sink)
	{
		logSinks.erase(std::remove(logSinks.begin(), logSinks.end(), sink), logSinks.end());
	}

	static std::string toString(LogPriority logPriority)
	{
		switch (logPriority)
		{
			case LogPriority::emerg:
				return "Emerg";
			case LogPriority::alert:
				return "Alert";
			case LogPriority::critical:
				return "Crit";
			case LogPriority::error:
				return "Err";
			case LogPriority::warning:
				return "Warn";
			case LogPriority::notice:
				return "Notice";
			case LogPriority::info:
				return "Info";
			case LogPriority::debug:
				return "Debug";
			default:
				std::stringstream ss;
				ss << logPriority;
				return ss.str();
		}
	}


protected:
	Log() :	type_(LogType::normal)
	{
	}

	int sync()
	{
		if (!buffer_.str().empty())
		{
			auto now = std::chrono::system_clock::now();
			if (conditional_.is_true())
			{
				for (const auto sink: logSinks)
				{
					if (
							(sink->get_type() == LogSink::Type::all) ||
							((type_ == LogType::special) && (sink->get_type() == LogSink::Type::special)) ||
							((type_ == LogType::normal) && (sink->get_type() == LogSink::Type::normal))
					)
						if (priority_ <= sink->priority)
							sink->log(now, priority_, type_, tag_, buffer_.str());
				}
			}
			buffer_.str("");
			buffer_.clear();
			//priority_ = debug; // default to debug for each message
			//type_ = kNormal;
			tag_ = nullptr;
			conditional_.set(true);
		}
		return 0;
	}

	int overflow(int c)
	{
/*		if (
				(priority_ > loglevel_) && 
				((type_ == kNormal) || !syslog_enabled_) // || (syslogpriority_ > loglevel_))
		)
			return c;
*/		if (c != EOF)
		{
			if (c == '\n')
				sync();
			else
				buffer_ << static_cast<char>(c);
		}
		else
		{
			std::cout << "EOF\n";
			sync();
		}
		return c;
	}


private:
	friend std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority);
	friend std::ostream& operator<< (std::ostream& os, const LogType& log_type);
	friend std::ostream& operator<< (std::ostream& os, const Tag& tag);
	friend std::ostream& operator<< (std::ostream& os, const Conditional& conditional);

	std::stringstream buffer_;
	LogPriority priority_;
	LogType type_;
	Tag tag_;
	Conditional conditional_;
	std::vector<log_sink_ptr> logSinks;
};



struct LogSinkFormat : public LogSink
{
	LogSinkFormat(LogPriority priority, Type type, const std::string& format = "%Y-%m-%d %H-%M-%S [#prio] (#tag)") : // #logline") : 
		LogSink(priority, type), 
		format_(format)
	{
	}

	virtual void set_format(const std::string& format)
	{
		format_ = format;
	}

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const = 0;


protected:
	/// strftime format + proprietary "#ms" for milliseconds
	virtual void do_log(std::ostream& stream, const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
		std::time_t now_c = std::chrono::system_clock::to_time_t(timestamp);
		struct::tm now_tm = *std::localtime(&now_c);

		char buffer[256];
		strftime(buffer, sizeof buffer, format_.c_str(), &now_tm);
		std::string result = buffer;
		size_t pos = result.find("#ms");
		if (pos != std::string::npos)
		{
			int ms_part = std::chrono::time_point_cast<std::chrono::milliseconds>(timestamp).time_since_epoch().count() % 1000;
			char ms_str[4];
			sprintf(ms_str, "%03d", ms_part);
			result.replace(pos, 3, ms_str);
		}

		pos = result.find("#prio");
		if (pos != std::string::npos)
			result.replace(pos, 5, Log::toString(priority));


		pos = result.find("#tag");
		if (pos != std::string::npos)
			result.replace(pos, 4, tag?tag.tag:"log");

		pos = result.find("#logline");
		if (pos != std::string::npos)
		{
			result.replace(pos, 8, message);
			stream << result << std::endl;
		}
		else
		{
			if (result.empty() || (result.back() == ' '))
				stream << message << std::endl;
			else
				stream << result << " " << message << std::endl;
		}
	}

	std::string format_;
};



struct LogSinkCout : public LogSinkFormat
{
	LogSinkCout(LogPriority priority, Type type, const std::string& format = "%Y-%m-%d %H-%M-%S.#ms [#prio] (#tag)") : // #logline") :
		LogSinkFormat(priority, type, format)
	{
	}

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
		if (priority <= this->priority)
			do_log(std::cout, timestamp, priority, type, tag, message);
	}
};



struct LogSinkCerr : public LogSinkFormat
{
	LogSinkCerr(LogPriority priority, Type type, const std::string& format = "%Y-%m-%d %H-%M-%S.#ms [#prio] (#tag)") : // #logline") :
		LogSinkFormat(priority, type, format)
	{
	}

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
		if (priority <= this->priority)
			do_log(std::cerr, timestamp, priority, type, tag, message);
	}
};



/// Not tested due to unavailability of Windows
struct LogSinkOutputDebugString : LogSink
{
	LogSinkOutputDebugString(LogPriority priority, Type type = Type::all, const std::string& default_tag = "") : LogSink(priority, type)
	{
	}

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
#ifdef _WIN32
		OutputDebugString(message.c_str());
#endif
	}
};



struct LogSinkUnifiedLogging : LogSink
{
	LogSinkUnifiedLogging(LogPriority priority, Type type = Type::all) : LogSink(priority, type)
	{
	}

#ifdef __APPLE__
	os_log_type_t get_os_log_type(LogPriority priority) const
	{
		switch (priority)
		{
			case LogPriority::emerg:
			case LogPriority::alert:
			case LogPriority::critical:
				return OS_LOG_TYPE_FAULT;
			case LogPriority::error:
				return OS_LOG_TYPE_ERROR;
			case LogPriority::warning:
			case LogPriority::notice:
				return OS_LOG_TYPE_DEFAULT;
			case LogPriority::info:
				return OS_LOG_TYPE_INFO;
			case LogPriority::debug:
				return OS_LOG_TYPE_DEBUG;
			default: 
				return OS_LOG_TYPE_DEFAULT;
		}
	}
#endif

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
#ifdef __APPLE__
		os_log_with_type(OS_LOG_DEFAULT, get_os_log_type(priority), "%{public}s", message.c_str());
#endif
	}
};



struct LogSinkSyslog : public LogSink
{
	LogSinkSyslog(const char* ident, LogPriority priority, Type type) : LogSink(priority, type)
	{
#ifndef _WIN32
		openlog(ident, LOG_PID, LOG_USER);
#endif
	}

	virtual ~LogSinkSyslog()
	{
		closelog();
	}

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
#ifndef _WIN32
		syslog((int)priority, "%s", message.c_str());
#endif
	}
};



struct LogSinkAndroid : public LogSink
{
	LogSinkAndroid(const std::string& ident, LogPriority priority, Type type = Type::all) : LogSink(priority, type), ident_(ident)
	{
	}

#ifdef __ANDROID__
	android_LogPriority get_android_prio(LogPriority priority) const
	{
		switch (priority)
		{
			case LogPriority::emerg:
			case LogPriority::alert:
			case LogPriority::critical:
				return ANDROID_LOG_FATAL;
			case LogPriority::error:
				return ANDROID_LOG_ERROR;
			case LogPriority::warning:
				return ANDROID_LOG_WARN;
			case LogPriority::notice:
				return ANDROID_LOG_DEFAULT;
			case LogPriority::info:
				return ANDROID_LOG_INFO;
			case LogPriority::debug:
				return ANDROID_LOG_DEBUG;
			default: 
				return ANDROID_LOG_UNKNOWN;
		}
	}
#endif

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
#ifdef __ANDROID__
		std::string log_tag;// = default_tag_;
		if (tag)
		{
			if (!ident_.empty())
				log_tag = ident_ + "." + tag.tag;
			else
				log_tag = tag.tag;
		}
		else
			log_tag = ident_;

		__android_log_write(get_android_prio(priority), log_tag.c_str(), message.c_str());
#endif
	}

protected:
	std::string ident_;
};



/// Not tested due to unavailability of Windows
struct LogSinkEventLog : public LogSink
{
	LogSinkEventLog(const std::string& ident, LogPriority priority, Type type = Type::all) : LogSink(priority, type)
	{
#ifdef _WIN32
		event_log = RegisterEventSource(NULL, ident.c_str());
#endif
	}

#ifdef _WIN32
	WORD get_type(LogPriority priority) const
	{
		switch (priority)
		{
			case LogPriority::emerg:
			case LogPriority::alert:
			case LogPriority::critical:
			case LogPriority::error:
				return EVENTLOG_ERROR_TYPE;
			case LogPriority::warning:
				return EVENTLOG_WARNING_TYPE;
			case LogPriority::notice:
				return EVENTLOG_SUCCESS;
			case LogPriority::info:
			case LogPriority::debug:
				return EVENTLOG_INFORMATION_TYPE;
			default: 
				return EVENTLOG_INFORMATION_TYPE;
		}
	}
#endif

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
#ifdef _WIN32
		ReportEvent(event_log, get_type(priority), 0, 0, NULL, 1, 0, &message.c_str(), NULL);
#endif
	}

protected:
#ifdef _WIN32
	HANDLE event_log;
#endif
};



struct LogSinkNative : public LogSink
{
	LogSinkNative(const std::string& ident, LogPriority priority, Type type = Type::all) : 
		LogSink(priority, type), 
		log_sink_(nullptr), 
		ident_(ident)
	{
#ifdef __ANDROID__
		log_sink_ = std::make_shared<LogSinkAndroid>(ident_, priority, type);
#elif __APPLE__
		log_sink_ = std::make_shared<LogSinkUnifiedLogging>(priority, type);
#elif _WIN32
		log_sink_ = std::make_shared<LogSinkEventLog>(priority, type);
#else
		log_sink_ = std::make_shared<LogSinkSyslog>(ident_.c_str(), priority, type);
#endif
	}

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
		if (log_sink_)
			log_sink_->log(timestamp, priority, type, tag, message);
	}

protected:
	log_sink_ptr log_sink_;
	std::string ident_;
};



struct LogSinkCallback : public LogSink
{
	typedef std::function<void(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message)> callback_fun;

	LogSinkCallback(LogPriority priority, Type type, callback_fun callback) : LogSink(priority, type), callback(callback)
	{
	}

	virtual void log(const time_point_sys_clock& timestamp, LogPriority priority, LogType type, const Tag& tag, const std::string& message) const
	{
		if (callback && (priority <= this->priority))
			callback(timestamp, priority, type, tag, message);
	}

private:
	callback_fun callback;
};



static std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log && (log->priority_ != log_priority))
	{
		log->sync();
		log->priority_ = log_priority;
	}
	return os;
}



static std::ostream& operator<< (std::ostream& os, const LogType& log_type)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log)
		log->type_ = log_type;
	return os;
}



static std::ostream& operator<< (std::ostream& os, const Tag& tag)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log)
		log->tag_ = tag;
	return os;
}



static std::ostream& operator<< (std::ostream& os, const Conditional& conditional)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log)
		log->conditional_.set(conditional.is_true());
	return os;
}



static std::ostream& operator<< (std::ostream& os, const LogColor& log_color)
{
    os << "\033[";
    if ((log_color.foreground == Color::none) && (log_color.background == Color::none))
        os << "0"; // reset colors if no params

    if (log_color.foreground != Color::none) 
	{
        os << 29 + (int)log_color.foreground;
        if (log_color.background != Color::none) 
			os << ";";
    }
    if (log_color.background != Color::none) 
        os << 39 + (int)log_color.background;
    os << "m";

	return os;
}



static std::ostream& operator<< (std::ostream& os, const Color& color)
{
    os << LogColor(color);
	return os;
}



#endif /// AIX_LOG_HPP


