/***
      __   __  _  _  __     __    ___ 
     / _\ (  )( \/ )(  )   /  \  / __)
    /    \ )(  )  ( / (_/\(  O )( (_ \
    \_/\_/(__)(_/\_)\____/ \__/  \___/
    version 0.16.0
    https://github.com/badaix/aixlog

    This file is part of aixlog
    Copyright (C) 2017  Johannes Pohl
    
    This software may be modified and distributed under the terms
    of the MIT license.  See the LICENSE file for details.
***/

/// inspired by "eater": 
/// https://stackoverflow.com/questions/2638654/redirect-c-stdclog-to-syslog-on-unix



#ifndef AIX_LOG_HPP
#define AIX_LOG_HPP

#ifndef _WIN32
#define _HAS_SYSLOG_ 1
#endif

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
#endif
#ifdef _HAS_SYSLOG_
#include <syslog.h>
#endif


/// Internal helper macros (exposed, but shouldn't be used directly)
#define AIXLOG_INTERNAL__LOG_WO_TAG(SEVERITY_) std::clog << (AixLog::Severity)AixLog::SEVERITY_
#define AIXLOG_INTERNAL__LOG_TAG(SEVERITY_, TAG_) std::clog << (AixLog::Severity)AixLog::SEVERITY_ << TAG(TAG_)

#define AIXLOG_INTERNAL__ONE_COLOR(FG_) AixLog::Color::FG_
#define AIXLOG_INTERNAL__TWO_COLOR(FG_, BG_) AixLog::TextColor(AixLog::Color::FG_, AixLog::Color::BG_)

//https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros
#define AIXLOG_INTERNAL__VAR_PARM(x,PARAM1_,PARAM2_,FUNC_, ...) FUNC_


/// External logger macros
// usage: LOG(SEVERITY) or LOG(SEVERITY, TAG)
// e.g.: LOG(NOTICE) or LOG(NOTICE, "my tag")
#define LOG(...) AIXLOG_INTERNAL__VAR_PARM(,##__VA_ARGS__, AIXLOG_INTERNAL__LOG_TAG(__VA_ARGS__), AIXLOG_INTERNAL__LOG_WO_TAG(__VA_ARGS__)) << TIMESTAMP << FUNC
#define SLOG(...) AIXLOG_INTERNAL__VAR_PARM(,##__VA_ARGS__, AIXLOG_INTERNAL__LOG_TAG(__VA_ARGS__), AIXLOG_INTERNAL__LOG_WO_TAG(__VA_ARGS__)) << TIMESTAMP << SPECIAL << FUNC

// usage: COLOR(TEXT_COLOR, BACKGROUND_COLOR) or COLOR(TEXT_COLOR)
// e.g.: COLOR(yellow, blue) or COLOR(red)
#define COLOR(...) AIXLOG_INTERNAL__VAR_PARM(,##__VA_ARGS__, AIXLOG_INTERNAL__TWO_COLOR(__VA_ARGS__), AIXLOG_INTERNAL__ONE_COLOR(__VA_ARGS__))

#define FUNC AixLog::Function(__func__, __FILE__, __LINE__)
#define TAG AixLog::Tag
#define COND AixLog::Conditional
#define SPECIAL AixLog::Type::special
#define TIMESTAMP AixLog::Timestamp(std::chrono::system_clock::now())



namespace AixLog
{

enum SEVERITY
{
// https://chromium.googlesource.com/chromium/mini_chromium/+/master/base/logging.cc

// Aixlog      Boost      Syslog      Android      macOS      Syslog Desc
//
//                        UNKNOWN
//                        DEFAULT
// trace       trace                 VERBOSE
// debug       debug      DEBUG      DEBUG        DEBUG      debug-level message
// info        info       INFO       INFO         INFO       informational message
// notice                 NOTICE                             normal, but significant, condition
// warning     warning    WARNING    WARN         DEFAULT    warning conditions
// error       error      ERROR      ERROR        ERROR      error conditions
// fatal       fatal      CRIT       FATAL        FAULT      critical conditions
//                        ALERT                              action must be taken immediately
//                        EMERG                              system is unusable

	TRACE = 0,
	DEBUG = 1,
	INFO = 2,
	NOTICE = 3,
	WARNING = 4,
	ERROR = 5,
	FATAL = 6
};



enum class Type
{
	normal,
	special,
	all
};



enum class Severity : std::int8_t
{
	trace   = TRACE,
	debug   = DEBUG,
	info    = INFO,
	notice  = NOTICE,
	warning = WARNING,
	error   = ERROR,
	fatal   = FATAL
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



struct TextColor
{
	TextColor(Color foreground = Color::none, Color background = Color::none) :
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



struct Timestamp
{
	typedef std::chrono::time_point<std::chrono::system_clock> time_point_sys_clock;

	Timestamp(std::nullptr_t) : is_null_(true)
	{
	}

	Timestamp() : Timestamp(nullptr)
	{
	}

	Timestamp(const time_point_sys_clock& time_point) : time_point(time_point), is_null_(false)
	{
	}

	virtual explicit operator bool() const
	{
		return !is_null_;
	}

	/// strftime format + proprietary "#ms" for milliseconds
	std::string to_string(const std::string& format = "%Y-%m-%d %H-%M-%S.#ms") const
	{
		std::time_t now_c = std::chrono::system_clock::to_time_t(time_point);
		struct::tm now_tm = *std::localtime(&now_c);

		char buffer[256];
		strftime(buffer, sizeof buffer, format.c_str(), &now_tm);
		std::string result = buffer;
		size_t pos = result.find("#ms");
		if (pos != std::string::npos)
		{
			int ms_part = std::chrono::time_point_cast<std::chrono::milliseconds>(time_point).time_since_epoch().count() % 1000;
			char ms_str[4];
			sprintf(ms_str, "%03d", ms_part);
			result.replace(pos, 3, ms_str);
		}
		return result;
	}

	time_point_sys_clock time_point;

private:
	bool is_null_;
};



struct Tag
{
	Tag(std::nullptr_t) : text(""), is_null_(true)
	{
	}

	Tag() : Tag(nullptr)
	{
	}

	Tag(const std::string& text) : text(text), is_null_(false)
	{
	}

	virtual explicit operator bool() const
	{
		return !is_null_;
	}

	std::string text;

private:
	bool is_null_;
};



struct Function
{
	Function(const std::string& name, const std::string& file, size_t line) :
		name(name), file(file), line(line), is_null_(false)
	{
	}

	Function(std::nullptr_t) : name(""), file(""), line(0), is_null_(true)
	{
	}

	Function() : Function(nullptr)
	{
	}

	virtual explicit operator bool() const
	{
		return !is_null_;
	}

	std::string name;
	std::string file;
	size_t line;

private:
	bool is_null_;
};



struct Metadata
{
	Metadata() : 
		severity(Severity::trace), tag(nullptr), type(Type::normal), function(nullptr), timestamp(nullptr)
	{
	}

	Severity severity;
	Tag tag;
	Type type;
	Function function;
	Timestamp timestamp;
};



struct Sink
{
	Sink(Severity severity, Type type) : severity(severity), sink_type_(type)
	{
	}

	virtual ~Sink()
	{
	}

	virtual void log(const Metadata& metadata, const std::string& message) const = 0;
	virtual Type get_type() const
	{
		return sink_type_;
	}

	virtual Sink& set_type(Type sink_type)
	{
		sink_type_ = sink_type;
		return *this;
	}

	Severity severity;

protected:
	Type sink_type_;
};



static std::ostream& operator<< (std::ostream& os, const Severity& log_severity);
static std::ostream& operator<< (std::ostream& os, const Type& log_type);
static std::ostream& operator<< (std::ostream& os, const Timestamp& timestamp);
static std::ostream& operator<< (std::ostream& os, const Tag& tag);
static std::ostream& operator<< (std::ostream& os, const Function& function);
static std::ostream& operator<< (std::ostream& os, const Conditional& conditional);

typedef std::shared_ptr<Sink> log_sink_ptr;


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
		Log::instance().log_sinks_.clear();

		for (auto sink: log_sinks)
			Log::instance().add_logsink(sink);

		std::clog.rdbuf(&Log::instance());
	}

	void add_logsink(log_sink_ptr sink)
	{
		log_sinks_.push_back(sink);
	}

	void remove_logsink(log_sink_ptr sink)
	{
		log_sinks_.erase(std::remove(log_sinks_.begin(), log_sinks_.end(), sink), log_sinks_.end());
	}

	static std::string to_string(Severity logSeverity)
	{
		switch (logSeverity)
		{
			case Severity::trace:
				return "Trace";
			case Severity::debug:
				return "Debug";
			case Severity::info:
				return "Info";
			case Severity::notice:
				return "Notice";
			case Severity::warning:
				return "Warn";
			case Severity::error:
				return "Err";
			case Severity::fatal:
				return "Fatal";
			default:
				std::stringstream ss;
				ss << logSeverity;
				return ss.str();
		}
	}


protected:
	Log()
	{
	}

	int sync()
	{
		if (!buffer_.str().empty())
		{
			if (conditional_.is_true())
			{
				for (const auto sink: log_sinks_)
				{
					if (
							(metadata_.type == Type::all) ||
							(sink->get_type() == Type::all) ||
							((metadata_.type == Type::special) && (sink->get_type() == Type::special)) ||
							((metadata_.type == Type::normal) && (sink->get_type() == Type::normal))
					)
						if (metadata_.severity >= sink->severity)
							sink->log(metadata_, buffer_.str());
				}
			}
			buffer_.str("");
			buffer_.clear();
			metadata_.severity = Severity::trace;
			metadata_.type = Type::normal;
			metadata_.timestamp = nullptr;
			metadata_.tag = nullptr;
			metadata_.function = nullptr;
			conditional_.set(true);
		}
		return 0;
	}

	int overflow(int c)
	{
/*		if (
				(severity_ > loglevel_) && 
				((type_ == kNormal) || !syslog_enabled_) // || (syslogseverity_ > loglevel_))
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
			sync();
		}
		return c;
	}


private:
	friend std::ostream& operator<< (std::ostream& os, const Severity& log_severity);
	friend std::ostream& operator<< (std::ostream& os, const Type& log_type);
	friend std::ostream& operator<< (std::ostream& os, const Timestamp& timestamp);
	friend std::ostream& operator<< (std::ostream& os, const Tag& tag);
	friend std::ostream& operator<< (std::ostream& os, const Function& function);
	friend std::ostream& operator<< (std::ostream& os, const Conditional& conditional);

	std::stringstream buffer_;
	Metadata metadata_;
	Conditional conditional_;
	std::vector<log_sink_ptr> log_sinks_;
};



struct SinkFormat : public Sink
{
	SinkFormat(Severity severity, Type type, const std::string& format = "%Y-%m-%d %H-%M-%S [#severity] (#tag)") :
		Sink(severity, type), 
		format_(format)
	{
	}

	virtual void set_format(const std::string& format)
	{
		format_ = format;
	}

	virtual void log(const Metadata& metadata, const std::string& message) const = 0;


protected:
	virtual void do_log(std::ostream& stream, const Metadata& metadata, const std::string& message) const
	{
		std::string result = format_;
		if (metadata.timestamp)
			result = metadata.timestamp.to_string(result);

		size_t pos = result.find("#severity");
		if (pos != std::string::npos)
			result.replace(pos, 9, Log::to_string(metadata.severity));

		pos = result.find("#tag");
		if (pos != std::string::npos)
			result.replace(pos, 4, metadata.tag?metadata.tag.text:(metadata.function?metadata.function.name:"log"));

		pos = result.find("#function");
		if (pos != std::string::npos)
			result.replace(pos, 9, metadata.function?metadata.function.name:"");

		pos = result.find("#message");
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



struct SinkCout : public SinkFormat
{
	SinkCout(Severity severity, Type type, const std::string& format = "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag)") :
		SinkFormat(severity, type, format)
	{
	}

	virtual void log(const Metadata& metadata, const std::string& message) const
	{
		if (severity >= this->severity)
			do_log(std::cout, metadata, message);
	}
};



struct SinkCerr : public SinkFormat
{
	SinkCerr(Severity severity, Type type, const std::string& format = "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag)") :
		SinkFormat(severity, type, format)
	{
	}

	virtual void log(const Metadata& metadata, const std::string& message) const
	{
		if (severity >= this->severity)
			do_log(std::cerr, metadata, message);
	}
};



/// Not tested due to unavailability of Windows
struct SinkOutputDebugString : public Sink
{
	SinkOutputDebugString(Severity severity, Type type = Type::all, const std::string& default_tag = "") : Sink(severity, type)
	{
	}

	virtual void log(const Metadata& metadata, const std::string& message) const
	{
#ifdef _WIN32
		OutputDebugString(message.c_str());
#endif
	}
};



struct SinkUnifiedLogging : public Sink
{
	SinkUnifiedLogging(Severity severity, Type type = Type::all) : Sink(severity, type)
	{
	}

#ifdef __APPLE__
	os_log_type_t get_os_log_type(Severity severity) const
	{
		// https://developer.apple.com/documentation/os/os_log_type_t?language=objc
		switch (severity)
		{
			case Severity::trace:
			case Severity::debug:
				return OS_LOG_TYPE_DEBUG;
			case Severity::info:
			case Severity::notice:
				return OS_LOG_TYPE_INFO;
			case Severity::warning:
				return OS_LOG_TYPE_DEFAULT;
			case Severity::error:
				return OS_LOG_TYPE_ERROR;
			case Severity::fatal:
				return OS_LOG_TYPE_FAULT;
			default: 
				return OS_LOG_TYPE_DEFAULT;
		}
	}
#endif

	virtual void log(const Metadata& metadata, const std::string& message) const
	{
#ifdef __APPLE__
		os_log_with_type(OS_LOG_DEFAULT, get_os_log_type(metadata.severity), "%{public}s", message.c_str());
#endif
	}
};



struct SinkSyslog : public Sink
{
	SinkSyslog(const char* ident, Severity severity, Type type) : Sink(severity, type)
	{
#ifdef _HAS_SYSLOG_
		openlog(ident, LOG_PID, LOG_USER);
#endif
	}

	virtual ~SinkSyslog()
	{
#ifdef _HAS_SYSLOG_
		closelog();
#endif
	}

#ifdef _HAS_SYSLOG_
	int get_syslog_priority(Severity severity) const
	{
		// http://unix.superglobalmegacorp.com/Net2/newsrc/sys/syslog.h.html
		switch (severity)
		{
			case Severity::trace:
			case Severity::debug:
				return LOG_DEBUG;
			case Severity::info:
				return LOG_INFO;
			case Severity::notice:
				return LOG_NOTICE;
			case Severity::warning:
				return LOG_WARNING;
			case Severity::error:
				return LOG_ERR;
			case Severity::fatal:
				return LOG_CRIT;
			default: 
				return LOG_INFO;
		}
	}
#endif


	virtual void log(const Metadata& metadata, const std::string& message) const
	{
#ifdef _HAS_SYSLOG_
		syslog(get_syslog_priority(metadata.severity), "%s", message.c_str());
#endif
	}
};



struct SinkAndroid : public Sink
{
	SinkAndroid(const std::string& ident, Severity severity, Type type = Type::all) : Sink(severity, type), ident_(ident)
	{
	}

#ifdef __ANDROID__
	android_LogPriority get_android_prio(Severity severity) const
	{
		// https://developer.android.com/ndk/reference/log_8h.html
		switch (severity)
		{
			case Severity::trace:
				return ANDROID_LOG_VERBOSE;
			case Severity::debug:
				return ANDROID_LOG_DEBUG;
			case Severity::info:
			case Severity::notice:
				return ANDROID_LOG_INFO;
			case Severity::warning:
				return ANDROID_LOG_WARN;
			case Severity::error:
				return ANDROID_LOG_ERROR;
			case Severity::fatal:
				return ANDROID_LOG_FATAL;
			default: 
				return ANDROID_LOG_UNKNOWN;
		}
	}
#endif

	virtual void log(const Metadata& metadata, const std::string& message) const
	{
#ifdef __ANDROID__
		std::string tag = metadata.tag?metadata.tag.text:(metadata.function?metadata.function.name:"");
		std::string log_tag;
		if (!ident_.empty() && !tag.empty())
			log_tag = ident_ + "." + tag;
		else if (!ident_.empty())
			log_tag = ident_;
		else if (!tag.empty())
			log_tag = tag;
		else
			log_tag = "log";
			
		__android_log_write(get_android_prio(metadata.severity), log_tag.c_str(), message.c_str());
#endif
	}

protected:
	std::string ident_;
};



/// Not tested due to unavailability of Windows
struct SinkEventLog : public Sink
{
	SinkEventLog(const std::string& ident, Severity severity, Type type = Type::all) : Sink(severity, type)
	{
#ifdef _WIN32
		event_log = RegisterEventSource(NULL, ident.c_str());
#endif
	}

#ifdef _WIN32
	WORD get_type(Severity severity) const
	{
		// https://msdn.microsoft.com/de-de/library/windows/desktop/aa363679(v=vs.85).aspx
		switch (severity)
		{
			case Severity::trace:
			case Severity::debug:
				return EVENTLOG_INFORMATION_TYPE;
			case Severity::info:
			case Severity::notice:
				return EVENTLOG_SUCCESS;
			case Severity::warning:
				return EVENTLOG_WARNING_TYPE;
			case Severity::error:
			case Severity::fatal:
				return EVENTLOG_ERROR_TYPE;
			default: 
				return EVENTLOG_INFORMATION_TYPE;
		}
	}
#endif

	virtual void log(const Metadata& metadata, const std::string& message) const
	{
#ifdef _WIN32
		ReportEvent(event_log, get_type(metadata.severity), 0, 0, NULL, 1, 0, &message.c_str(), NULL);
#endif
	}

protected:
#ifdef _WIN32
	HANDLE event_log;
#endif
};



struct SinkNative : public Sink
{
	SinkNative(const std::string& ident, Severity severity, Type type = Type::all) : 
		Sink(severity, type), 
		log_sink_(nullptr), 
		ident_(ident)
	{
#ifdef __ANDROID__
		log_sink_ = std::make_shared<SinkAndroid>(ident_, severity, type);
#elif __APPLE__
		log_sink_ = std::make_shared<SinkUnifiedLogging>(severity, type);
#elif _WIN32
		log_sink_ = std::make_shared<SinkEventLog>(severity, type);
#elif _HAS_SYSLOG_
		log_sink_ = std::make_shared<SinkSyslog>(ident_.c_str(), severity, type);
#else
		/// will not throw or something. Use "get_logger()" to check for success
		log_sink_ = nullptr;
#endif
	}

	virtual log_sink_ptr get_logger()
	{
		return log_sink_;
	}

	virtual void log(const Metadata& metadata, const std::string& message) const
	{
		if (log_sink_)
			log_sink_->log(metadata, message);
	}

protected:
	log_sink_ptr log_sink_;
	std::string ident_;
};



struct SinkCallback : public Sink
{
	typedef std::function<void(const Metadata& metadata, const std::string& message)> callback_fun;

	SinkCallback(Severity severity, Type type, callback_fun callback) : Sink(severity, type), callback_(callback)
	{
	}

	virtual void log(const Metadata& metadata, const std::string& message) const
	{
		if (callback_ && (severity >= this->severity))
			callback_(metadata, message);
	}

private:
	callback_fun callback_;
};



static std::ostream& operator<< (std::ostream& os, const Severity& log_severity)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log && (log->metadata_.severity != log_severity))
	{
		log->sync();
		log->metadata_.severity = log_severity;
	}
	return os;
}



static std::ostream& operator<< (std::ostream& os, const Type& log_type)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log)
		log->metadata_.type = log_type;
	return os;
}



static std::ostream& operator<< (std::ostream& os, const Timestamp& timestamp)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log)
		log->metadata_.timestamp = timestamp;
	return os;
}



static std::ostream& operator<< (std::ostream& os, const Tag& tag)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log)
		log->metadata_.tag = tag;
	return os;
}



static std::ostream& operator<< (std::ostream& os, const Function& function)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log)
		log->metadata_.function = function;
	return os;
}



static std::ostream& operator<< (std::ostream& os, const Conditional& conditional)
{
	Log* log = dynamic_cast<Log*>(os.rdbuf());
	if (log)
		log->conditional_.set(conditional.is_true());
	return os;
}



static std::ostream& operator<< (std::ostream& os, const TextColor& log_color)
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
	os << TextColor(color);
	return os;
}


} /// namespace AixLog


#endif /// AIX_LOG_HPP


