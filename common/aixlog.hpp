/***
      __   __  _  _  __     __    ___
     / _\ (  )( \/ )(  )   /  \  / __)
    /    \ )(  )  ( / (_/\(  O )( (_ \
    \_/\_/(__)(_/\_)\____/ \__/  \___/
    version 1.4.0
    https://github.com/badaix/aixlog

    This file is part of aixlog
    Copyright (C) 2017-2020 Johannes Pohl

    This software may be modified and distributed under the terms
    of the MIT license.  See the LICENSE file for details.
***/

/// inspired by "eater":
/// https://stackoverflow.com/questions/2638654/redirect-c-stdclog-to-syslog-on-unix

#ifndef AIX_LOG_HPP
#define AIX_LOG_HPP

#ifndef _WIN32
#define HAS_SYSLOG_ 1
#endif

#ifdef __APPLE__
#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1012
#define HAS_APPLE_UNIFIED_LOG_ 1
#endif
#endif
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifdef _WIN32
#include <Windows.h>
// ERROR macro is defined in Windows header
// To avoid conflict between these macro and declaration of ERROR / DEBUG in SEVERITY enum
// We save macro and undef it
#pragma push_macro("ERROR")
#pragma push_macro("DEBUG")
#undef ERROR
#undef DEBUG
#endif

#ifdef HAS_APPLE_UNIFIED_LOG_
#include <os/log.h>
#endif

#ifdef HAS_SYSLOG_
#include <syslog.h>
#endif

#ifdef __ANDROID__
// fix for bug "Android NDK __func__ definition is inconsistent with glibc and C++99"
// https://bugs.chromium.org/p/chromium/issues/detail?id=631489
#ifdef __GNUC__
#define AIXLOG_INTERNAL__FUNC __FUNCTION__
#else
#define AIXLOG_INTERNAL__FUNC __func__
#endif
#else
#define AIXLOG_INTERNAL__FUNC __func__
#endif

/// Internal helper macros (exposed, but shouldn't be used directly)
#define AIXLOG_INTERNAL__LOG_SEVERITY(SEVERITY_) std::clog << static_cast<AixLog::Severity>(SEVERITY_) << TAG()
#define AIXLOG_INTERNAL__LOG_SEVERITY_TAG(SEVERITY_, TAG_) std::clog << static_cast<AixLog::Severity>(SEVERITY_) << TAG(TAG_)

#define AIXLOG_INTERNAL__ONE_COLOR(FG_) AixLog::Color::FG_
#define AIXLOG_INTERNAL__TWO_COLOR(FG_, BG_) AixLog::TextColor(AixLog::Color::FG_, AixLog::Color::BG_)

// https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros
#define AIXLOG_INTERNAL__VAR_PARM(PARAM1_, PARAM2_, FUNC_, ...) FUNC_
#define AIXLOG_INTERNAL__LOG_MACRO_CHOOSER(...) AIXLOG_INTERNAL__VAR_PARM(__VA_ARGS__, AIXLOG_INTERNAL__LOG_SEVERITY_TAG, AIXLOG_INTERNAL__LOG_SEVERITY, )
#define AIXLOG_INTERNAL__COLOR_MACRO_CHOOSER(...) AIXLOG_INTERNAL__VAR_PARM(__VA_ARGS__, AIXLOG_INTERNAL__TWO_COLOR, AIXLOG_INTERNAL__ONE_COLOR, )

/// External logger macros
// usage: LOG(SEVERITY) or LOG(SEVERITY, TAG)
// e.g.: LOG(NOTICE) or LOG(NOTICE, "my tag")
#ifndef WIN32
#define LOG(...) AIXLOG_INTERNAL__LOG_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__) << TIMESTAMP << FUNC
#endif

// usage: COLOR(TEXT_COLOR, BACKGROUND_COLOR) or COLOR(TEXT_COLOR)
// e.g.: COLOR(yellow, blue) or COLOR(red)
#define COLOR(...) AIXLOG_INTERNAL__COLOR_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define FUNC AixLog::Function(AIXLOG_INTERNAL__FUNC, __FILE__, __LINE__)
#define TAG AixLog::Tag
#define COND AixLog::Conditional
#define TIMESTAMP AixLog::Timestamp(std::chrono::system_clock::now())


// stijnvdb: sorry! :) LOG(SEV, "tag") was not working for Windows and I couldn't figure out how to fix it for windows without potentially breaking everything
// else...
// https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros (Jason Deng)
#ifdef WIN32
#define LOG_2(severity, tag) AIXLOG_INTERNAL__LOG_SEVERITY_TAG(severity, tag)
#define LOG_1(severity) AIXLOG_INTERNAL__LOG_SEVERITY(severity)
#define LOG_0() LOG_1(0)

#define FUNC_CHOOSER(_f1, _f2, _f3, ...) _f3
#define FUNC_RECOMPOSER(argsWithParentheses) FUNC_CHOOSER argsWithParentheses
#define CHOOSE_FROM_ARG_COUNT(...) FUNC_RECOMPOSER((__VA_ARGS__, LOG_2, LOG_1, FUNC_, ...))
#define MACRO_CHOOSER(...) CHOOSE_FROM_ARG_COUNT(__VA_ARGS__())
#define LOG(...) MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__) << TIMESTAMP << FUNC
#endif

/**
 * @brief
 * Severity of the log message
 */
enum SEVERITY
{
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    NOTICE = 3,
    WARNING = 4,
    ERROR = 5,
    FATAL = 6
};

namespace AixLog
{

/**
 * @brief
 * Severity of the log message
 *
 * Mandatory parameter for the LOG macro
 */
enum class Severity : std::int8_t
{
    // Mapping table from AixLog to other loggers. Boost is just for information.
    // https://chromium.googlesource.com/chromium/mini_chromium/+/master/base/logging.cc
    //
    // Aixlog      Boost       Syslog      Android     macOS       EventLog      Syslog Desc
    //
    // trace       trace       DEBUG       VERBOSE     DEBUG       INFORMATION
    // debug       debug       DEBUG       DEBUG       DEBUG       INFORMATION   debug-level message
    // info        info        INFO        INFO        INFO        SUCCESS       informational message
    // notice                  NOTICE      INFO        INFO        SUCCESS       normal, but significant, condition
    // warning     warning     WARNING     WARN        DEFAULT     WARNING       warning conditions
    // error       error       ERROR       ERROR       ERROR       ERROR         error conditions
    // fatal       fatal       CRIT        FATAL       FAULT       ERROR         critical conditions
    //                         ALERT                                             action must be taken immediately
    //                         EMERG                                             system is unusable

    trace = SEVERITY::TRACE,
    debug = SEVERITY::DEBUG,
    info = SEVERITY::INFO,
    notice = SEVERITY::NOTICE,
    warning = SEVERITY::WARNING,
    error = SEVERITY::ERROR,
    fatal = SEVERITY::FATAL
};


static Severity to_severity(std::string severity, Severity def = Severity::info)
{
    std::transform(severity.begin(), severity.end(), severity.begin(), [](unsigned char c) { return std::tolower(c); });
    if (severity == "trace")
        return Severity::trace;
    else if (severity == "debug")
        return Severity::debug;
    else if (severity == "info")
        return Severity::info;
    else if (severity == "notice")
        return Severity::notice;
    else if (severity == "warning")
        return Severity::warning;
    else if (severity == "error")
        return Severity::error;
    else if (severity == "fatal")
        return Severity::fatal;
    else
        return def;
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
            return "Error";
        case Severity::fatal:
            return "Fatal";
        default:
            std::stringstream ss;
            ss << static_cast<int>(logSeverity);
            return ss.str();
    }
}

/**
 * @brief
 * Color constants used for console colors
 */
enum class Color
{
    none = 0,
    NONE = 0,
    black = 1,
    BLACK = 1,
    red = 2,
    RED = 2,
    green = 3,
    GREEN = 3,
    yellow = 4,
    YELLOW = 4,
    blue = 5,
    BLUE = 5,
    magenta = 6,
    MAGENTA = 6,
    cyan = 7,
    CYAN = 7,
    white = 8,
    WHITE = 8
};

/**
 * @brief
 * Encapsulation of foreground and background color
 */
struct TextColor
{
    TextColor(Color foreground = Color::none, Color background = Color::none) : foreground(foreground), background(background)
    {
    }

    Color foreground;
    Color background;
};

/**
 * @brief
 * For Conditional logging of a log line
 */
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

/**
 * @brief
 * Timestamp of a log line
 *
 * to_string will convert the time stamp into a string, using the strftime syntax
 */
struct Timestamp
{
    using time_point_sys_clock = std::chrono::time_point<std::chrono::system_clock>;

    Timestamp(std::nullptr_t) : is_null_(true)
    {
    }

    Timestamp() : Timestamp(nullptr)
    {
    }

    Timestamp(const time_point_sys_clock& time_point) : time_point(time_point), is_null_(false)
    {
    }

    Timestamp(time_point_sys_clock&& time_point) : time_point(std::move(time_point)), is_null_(false)
    {
    }

    virtual ~Timestamp() = default;

    explicit operator bool() const
    {
        return !is_null_;
    }

    /// strftime format + proprietary "#ms" for milliseconds
    std::string to_string(const std::string& format = "%Y-%m-%d %H-%M-%S.#ms") const
    {
        std::time_t now_c = std::chrono::system_clock::to_time_t(time_point);
        struct ::tm now_tm = localtime_xp(now_c);
        char buffer[256];
        strftime(buffer, sizeof buffer, format.c_str(), &now_tm);
        std::string result(buffer);
        size_t pos = result.find("#ms");
        if (pos != std::string::npos)
        {
            int ms_part = std::chrono::time_point_cast<std::chrono::milliseconds>(time_point).time_since_epoch().count() % 1000;
            char ms_str[4];
            if (snprintf(ms_str, 4, "%03d", ms_part) >= 0)
                result.replace(pos, 3, ms_str);
        }
        return result;
    }

    time_point_sys_clock time_point;

private:
    bool is_null_;

    inline std::tm localtime_xp(std::time_t timer) const
    {
        std::tm bt;
#if defined(__unix__)
        localtime_r(&timer, &bt);
#elif defined(_MSC_VER)
        localtime_s(&bt, &timer);
#else
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        bt = *std::localtime(&timer);
#endif
        return bt;
    }
};

/**
 * @brief
 * Tag (string) for log line
 */
struct Tag
{
    Tag(std::nullptr_t) : text(""), is_null_(true)
    {
    }

    Tag() : Tag(nullptr)
    {
    }

    Tag(const char* text) : text(text), is_null_(false)
    {
    }

    Tag(const std::string& text) : text(text), is_null_(false)
    {
    }

    Tag(std::string&& text) : text(std::move(text)), is_null_(false)
    {
    }

    virtual ~Tag() = default;

    explicit operator bool() const
    {
        return !is_null_;
    }

    bool operator<(const Tag& other) const
    {
        return (text < other.text);
    }

    std::string text;

private:
    bool is_null_;
};

/**
 * @brief
 * Capture function, file and line number of the log line
 */
struct Function
{
    Function(const std::string& name, const std::string& file, size_t line) : name(name), file(file), line(line), is_null_(false)
    {
    }

    Function(std::string&& name, std::string&& file, size_t line) : name(std::move(name)), file(std::move(file)), line(line), is_null_(false)
    {
    }

    Function(std::nullptr_t) : name(""), file(""), line(0), is_null_(true)
    {
    }

    Function() : Function(nullptr)
    {
    }

    virtual ~Function() = default;

    explicit operator bool() const
    {
        return !is_null_;
    }

    std::string name;
    std::string file;
    size_t line;

private:
    bool is_null_;
};

/**
 * @brief
 * Collection of a log line's meta data
 */
struct Metadata
{
    Metadata() : severity(Severity::trace), tag(nullptr), function(nullptr), timestamp(nullptr)
    {
    }

    Severity severity;
    Tag tag;
    Function function;
    Timestamp timestamp;
};


class Filter
{
public:
    Filter()
    {
    }

    Filter(Severity severity)
    {
        add_filter(severity);
    }

    bool match(const Metadata& metadata) const
    {
        if (tag_filter_.empty())
            return true;

        auto iter = tag_filter_.find(metadata.tag);
        if (iter != tag_filter_.end())
            return (metadata.severity >= iter->second);

        iter = tag_filter_.find("*");
        if (iter != tag_filter_.end())
            return (metadata.severity >= iter->second);

        return false;
    }

    void add_filter(const Tag& tag, Severity severity)
    {
        tag_filter_[tag] = severity;
    }

    void add_filter(Severity severity)
    {
        tag_filter_["*"] = severity;
    }

    void add_filter(const std::string& filter)
    {
        auto pos = filter.find(":");
        if (pos != std::string::npos)
            add_filter(filter.substr(0, pos), to_severity(filter.substr(pos + 1)));
        else
            add_filter(to_severity(filter));
    }

private:
    std::map<Tag, Severity> tag_filter_;
};


/**
 * @brief
 * Abstract log sink
 *
 * All log sinks must inherit from this Sink
 */
struct Sink
{
    Sink(const Filter& filter) : filter(filter)
    {
    }

    virtual ~Sink() = default;

    virtual void log(const Metadata& metadata, const std::string& message) = 0;

    Filter filter;
};

/// ostream operators << for the meta data structs
static std::ostream& operator<<(std::ostream& os, const Severity& log_severity);
static std::ostream& operator<<(std::ostream& os, const Timestamp& timestamp);
static std::ostream& operator<<(std::ostream& os, const Tag& tag);
static std::ostream& operator<<(std::ostream& os, const Function& function);
static std::ostream& operator<<(std::ostream& os, const Conditional& conditional);
static std::ostream& operator<<(std::ostream& os, const Color& color);
static std::ostream& operator<<(std::ostream& os, const TextColor& text_color);

using log_sink_ptr = std::shared_ptr<Sink>;

/**
 * @brief
 * Main Logger class with "Log::init"
 *
 * Don't use it directly, but call once "Log::init" with your log sink instances.
 * The Log class will simply redirect clog to itself (as a streambuf) and
 * forward whatever went to clog to the log sink instances
 */
class Log : public std::basic_streambuf<char, std::char_traits<char>>
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

        for (const auto& sink : log_sinks)
            Log::instance().add_logsink(sink);
    }

    template <typename T, typename... Ts>
    static std::shared_ptr<T> init(Ts&&... params)
    {
        std::shared_ptr<T> sink = Log::instance().add_logsink<T>(std::forward<Ts>(params)...);
        init({sink});
        return sink;
    }

    template <typename T, typename... Ts>
    std::shared_ptr<T> add_logsink(Ts&&... params)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        static_assert(std::is_base_of<Sink, typename std::decay<T>::type>::value, "type T must be a Sink");
        std::shared_ptr<T> sink = std::make_shared<T>(std::forward<Ts>(params)...);
        log_sinks_.push_back(sink);
        return sink;
    }

    void add_logsink(const log_sink_ptr& sink)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        log_sinks_.push_back(sink);
    }

    void remove_logsink(const log_sink_ptr& sink)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        log_sinks_.erase(std::remove(log_sinks_.begin(), log_sinks_.end(), sink), log_sinks_.end());
    }

protected:
    Log() noexcept : last_buffer_(nullptr)
    {
        std::clog.rdbuf(this);
        std::clog << Severity() << Tag() << Function() << Conditional() << AixLog::Color::NONE << std::flush;
    }

    virtual ~Log()
    {
        sync();
    }

    int sync() override
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!get_stream().str().empty())
        {
            if (conditional_.is_true())
            {
                for (const auto& sink : log_sinks_)
                {
                    if (sink->filter.match(metadata_))
                        sink->log(metadata_, get_stream().str());
                }
            }
            get_stream().str("");
            get_stream().clear();
        }

        return 0;
    }

    int overflow(int c) override
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (c != EOF)
        {
            if (c == '\n')
                sync();
            else
                get_stream() << static_cast<char>(c);
        }
        else
        {
            sync();
        }
        return c;
    }

private:
    friend std::ostream& operator<<(std::ostream& os, const Severity& log_severity);
    friend std::ostream& operator<<(std::ostream& os, const Timestamp& timestamp);
    friend std::ostream& operator<<(std::ostream& os, const Tag& tag);
    friend std::ostream& operator<<(std::ostream& os, const Function& function);
    friend std::ostream& operator<<(std::ostream& os, const Conditional& conditional);

    std::stringstream& get_stream()
    {
        auto id = std::this_thread::get_id();
        if ((last_buffer_ == nullptr) || (last_id_ != id))
        {
            last_id_ = id;
            last_buffer_ = &(buffer_[id]);
        }
        return *last_buffer_;
    }

    std::map<std::thread::id, std::stringstream> buffer_;
    std::thread::id last_id_;
    std::stringstream* last_buffer_ = nullptr;
    Metadata metadata_;
    Conditional conditional_;
    std::vector<log_sink_ptr> log_sinks_;
    std::recursive_mutex mutex_;
};

/**
 * @brief
 * Null log sink
 *
 * Discards all log messages
 */
struct SinkNull : public Sink
{
    SinkNull() : Sink(Filter())
    {
    }

    void log(const Metadata& /*metadata*/, const std::string& /*message*/) override
    {
    }
};


/**
 * @brief
 * Abstract log sink with support for formatting log message
 *
 * "format" in the c'tor defines a log pattern.
 * For every log message, these placeholders will be substituded:
 * - strftime syntax is used to format the logging time stamp (%Y, %m, %d, ...)
 * - #ms: milliseconds part of the logging time stamp with leading zeros
 * - #severity: log severity
 * - #tag_func: the log tag. If empty, the function
 * - #tag: the log tag
 * - #function: the function
 * - #message: the log message
 */
struct SinkFormat : public Sink
{
    SinkFormat(const Filter& filter, const std::string& format) : Sink(filter), format_(format)
    {
    }

    virtual void set_format(const std::string& format)
    {
        format_ = format;
    }

    void log(const Metadata& metadata, const std::string& message) override = 0;

protected:
    virtual void do_log(std::ostream& stream, const Metadata& metadata, const std::string& message) const
    {
        std::string result = format_;
        if (metadata.timestamp)
            result = metadata.timestamp.to_string(result);

        size_t pos = result.find("#severity");
        if (pos != std::string::npos)
            result.replace(pos, 9, to_string(metadata.severity));

        pos = result.find("#color_severity");
        if (pos != std::string::npos)
        {
            std::stringstream ss;
            ss << TextColor(Color::RED) << to_string(metadata.severity) << TextColor(Color::NONE);
            result.replace(pos, 15, ss.str());
        }

        pos = result.find("#tag_func");
        if (pos != std::string::npos)
            result.replace(pos, 9, metadata.tag ? metadata.tag.text : (metadata.function ? metadata.function.name : "log"));

        pos = result.find("#tag");
        if (pos != std::string::npos)
            result.replace(pos, 4, metadata.tag ? metadata.tag.text : "");

        pos = result.find("#function");
        if (pos != std::string::npos)
            result.replace(pos, 9, metadata.function ? metadata.function.name : "");

        pos = result.find("#message");
        if (pos != std::string::npos)
        {
            result.replace(pos, 8, message);
            stream << result << std::endl;
        }
        else
        {
            if (result.empty() || (result.back() == ' '))
                stream << result << message << std::endl;
            else
                stream << result << " " << message << std::endl;
        }
    }

    std::string format_;
};

/**
 * @brief
 * Formatted logging to cout
 */
struct SinkCout : public SinkFormat
{
    SinkCout(const Filter& filter, const std::string& format = "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)") : SinkFormat(filter, format)
    {
    }

    void log(const Metadata& metadata, const std::string& message) override
    {
        do_log(std::cout, metadata, message);
    }
};

/**
 * @brief
 * Formatted logging to cerr
 */
struct SinkCerr : public SinkFormat
{
    SinkCerr(const Filter& filter, const std::string& format = "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)") : SinkFormat(filter, format)
    {
    }

    void log(const Metadata& metadata, const std::string& message) override
    {
        do_log(std::cerr, metadata, message);
    }
};

/**
 * @brief
 * Formatted logging to file
 */
struct SinkFile : public SinkFormat
{
    SinkFile(const Filter& filter, const std::string& filename, const std::string& format = "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)")
        : SinkFormat(filter, format)
    {
        ofs.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    }

    ~SinkFile() override
    {
        ofs.close();
    }

    void log(const Metadata& metadata, const std::string& message) override
    {
        do_log(ofs, metadata, message);
    }

protected:
    mutable std::ofstream ofs;
};

#ifdef _WIN32
/**
 * @brief
 * Windows: Logging to OutputDebugString
 *
 * Not tested due to unavailability of Windows
 */
struct SinkOutputDebugString : public Sink
{
    SinkOutputDebugString(const Filter& filter) : Sink(filter)
    {
    }

    void log(const Metadata& metadata, const std::string& message) override
    {
        std::wstring wide = std::wstring(message.begin(), message.end());
        OutputDebugString(wide.c_str());
    }
};
#endif

#ifdef HAS_APPLE_UNIFIED_LOG_
/**
 * @brief
 * macOS: Logging to Apples system logger
 */
struct SinkUnifiedLogging : public Sink
{
    SinkUnifiedLogging(const Filter& filter) : Sink(filter)
    {
    }

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

    void log(const Metadata& metadata, const std::string& message) override
    {
        os_log_with_type(OS_LOG_DEFAULT, get_os_log_type(metadata.severity), "%{public}s", message.c_str());
    }
};
#endif

#ifdef HAS_SYSLOG_
/**
 * @brief
 * UNIX: Logging to syslog
 */
struct SinkSyslog : public Sink
{
    SinkSyslog(const char* ident, const Filter& filter) : Sink(filter)
    {
        openlog(ident, LOG_PID, LOG_USER);
    }

    ~SinkSyslog() override
    {
        closelog();
    }

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

    void log(const Metadata& metadata, const std::string& message) override
    {
        syslog(get_syslog_priority(metadata.severity), "%s", message.c_str());
    }
};
#endif

#ifdef __ANDROID__
/**
 * @brief
 * Android: Logging to android log
 *
 * Use logcat to read the logs
 */
struct SinkAndroid : public Sink
{
    SinkAndroid(const std::string& ident, const Filter& filter) : Sink(filter), ident_(ident)
    {
    }

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

    void log(const Metadata& metadata, const std::string& message) override
    {
        std::string tag = metadata.tag ? metadata.tag.text : (metadata.function ? metadata.function.name : "");
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
    }

protected:
    std::string ident_;
};
#endif

#ifdef _WIN32
/**
 * @brief
 * Windows: Logging to event logger
 *
 * Not tested due to unavailability of Windows
 */
struct SinkEventLog : public Sink
{
    SinkEventLog(const std::string& ident, const Filter& filter) : Sink(filter)
    {
        std::wstring wide = std::wstring(ident.begin(), ident.end()); // stijnvdb: RegisterEventSource expands to RegisterEventSourceW which takes wchar_t
        event_log = RegisterEventSource(NULL, wide.c_str());
    }

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

    void log(const Metadata& metadata, const std::string& message) override
    {
        std::wstring wide = std::wstring(message.begin(), message.end());
        // We need this temp variable because we cannot take address of rValue
        const wchar_t* c_str = wide.c_str();

        ReportEvent(event_log, get_type(metadata.severity), 0, 0, NULL, 1, 0, &c_str, NULL);
    }

protected:
    HANDLE event_log;
};
#endif

/**
 * @brief
 * Log to the system's native sys logger
 *
 * - Android: Android log
 * - macOS:   unified log
 * - Windows: event log
 * - Unix:    syslog
 */
struct SinkNative : public Sink
{
    SinkNative(const std::string& ident, const Filter& filter) : Sink(filter), log_sink_(nullptr), ident_(ident)
    {
#ifdef __ANDROID__
        log_sink_ = std::make_shared<SinkAndroid>(ident_, filter);
#elif HAS_APPLE_UNIFIED_LOG_
        log_sink_ = std::make_shared<SinkUnifiedLogging>(filter);
#elif _WIN32
        log_sink_ = std::make_shared<SinkEventLog>(ident, filter);
#elif HAS_SYSLOG_
        log_sink_ = std::make_shared<SinkSyslog>(ident_.c_str(), filter);
#else
        /// will not throw or something. Use "get_logger()" to check for success
        log_sink_ = nullptr;
#endif
    }

    virtual log_sink_ptr get_logger()
    {
        return log_sink_;
    }

    void log(const Metadata& metadata, const std::string& message) override
    {
        if (log_sink_ != nullptr)
            log_sink_->log(metadata, message);
    }

protected:
    log_sink_ptr log_sink_;
    std::string ident_;
};

/**
 * @brief
 * Forward log messages to a callback function
 *
 * Pass the callback function to the c'tor.
 * This can be any function that matches the signature of "callback_fun"
 * Might also be a lambda function
 */
struct SinkCallback : public Sink
{
    using callback_fun = std::function<void(const Metadata& metadata, const std::string& message)>;

    SinkCallback(const Filter& filter, callback_fun callback) : Sink(filter), callback_(callback)
    {
    }

    void log(const Metadata& metadata, const std::string& message) override
    {
        if (callback_)
            callback_(metadata, message);
    }

private:
    callback_fun callback_;
};

/**
 * @brief
 * ostream << operator for "Severity"
 *
 * Severity must be the first thing that is logged into clog, since it will reset the loggers metadata.
 */
static std::ostream& operator<<(std::ostream& os, const Severity& log_severity)
{
    Log* log = dynamic_cast<Log*>(os.rdbuf());
    if (log != nullptr)
    {
        std::lock_guard<std::recursive_mutex> lock(log->mutex_);
        if (log->metadata_.severity != log_severity)
        {
            log->sync();
            log->metadata_.severity = log_severity;
            log->metadata_.timestamp = nullptr;
            log->metadata_.tag = nullptr;
            log->metadata_.function = nullptr;
            log->conditional_.set(true);
        }
    }
    else
    {
        os << to_string(log_severity);
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, const Timestamp& timestamp)
{
    Log* log = dynamic_cast<Log*>(os.rdbuf());
    if (log != nullptr)
    {
        std::lock_guard<std::recursive_mutex> lock(log->mutex_);
        log->metadata_.timestamp = timestamp;
    }
    else if (timestamp)
    {
        os << timestamp.to_string();
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, const Tag& tag)
{
    Log* log = dynamic_cast<Log*>(os.rdbuf());
    if (log != nullptr)
    {
        std::lock_guard<std::recursive_mutex> lock(log->mutex_);
        log->metadata_.tag = tag;
    }
    else if (tag)
    {
        os << tag.text;
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, const Function& function)
{
    Log* log = dynamic_cast<Log*>(os.rdbuf());
    if (log != nullptr)
    {
        std::lock_guard<std::recursive_mutex> lock(log->mutex_);
        log->metadata_.function = function;
    }
    else if (function)
    {
        os << function.name;
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, const Conditional& conditional)
{
    Log* log = dynamic_cast<Log*>(os.rdbuf());
    if (log != nullptr)
    {
        std::lock_guard<std::recursive_mutex> lock(log->mutex_);
        log->conditional_.set(conditional.is_true());
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, const TextColor& text_color)
{
    os << "\033[";
    if ((text_color.foreground == Color::none) && (text_color.background == Color::none))
        os << "0"; // reset colors if no params

    if (text_color.foreground != Color::none)
    {
        os << 29 + static_cast<int>(text_color.foreground);
        if (text_color.background != Color::none)
            os << ";";
    }
    if (text_color.background != Color::none)
        os << 39 + static_cast<int>(text_color.background);
    os << "m";

    return os;
}

static std::ostream& operator<<(std::ostream& os, const Color& color)
{
    os << TextColor(color);
    return os;
}

} // namespace AixLog

#ifdef _WIN32
// We restore the ERROR Windows macro
#pragma pop_macro("ERROR")
#pragma pop_macro("DEBUG")
#endif

#endif // AIX_LOG_HPP
