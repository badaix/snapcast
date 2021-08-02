/***
     ____   __  ____  __
    (  _ \ /  \(  _ \(  )
     ) __/(  O )) __// (_/\
    (__)   \__/(__)  \____/
    version 1.3.0
    https://github.com/badaix/popl

    This file is part of popl (program options parser lib)
    Copyright (C) 2015-2021 Johannes Pohl

    This software may be modified and distributed under the terms
    of the MIT license.  See the LICENSE file for details.
***/

/// checked with clang-tidy:
/// run-clang-tidy-3.8.py -header-filter='.*'
/// -checks='*,-misc-definitions-in-headers,-google-readability-braces-around-statements,-readability-braces-around-statements,-cppcoreguidelines-pro-bounds-pointer-arithmetic,-google-build-using-namespace,-google-build-using-namespace'

#ifndef POPL_HPP
#define POPL_HPP

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef WINDOWS
#include <cctype>
#endif


namespace popl
{

#define POPL_VERSION "1.3.0"


/// Option's argument type
/**
 * Switch has "no" argument
 * Value has "required" argument
 * Implicit has "optional" argument
 */
enum class Argument
{
    no = 0,   // option never takes an argument
    required, // option always requires an argument
    optional  // option may take an argument
};


/// Option's attribute
/**
 * inactive: Option is not set and will not be parsed
 * hidden:   Option is active, but will not show up in the help message
 * required: Option must be set on the command line. Otherwise an exception will be thrown
 * optional: Option must not be set. Default attribute.
 * advanced: Option is advanced and will only show up in the advanced help message
 * expoert:  Option is expert and will only show up in the expert help message
 */
enum class Attribute
{
    inactive = 0,
    hidden = 1,
    required = 2,
    optional = 3,
    advanced = 4,
    expert = 5
};


/// Option name type. Used in invalid_option exception.
/**
 * unspecified: not specified
 * short_name:  The option's short name
 * long_name:   The option's long name
 */
enum class OptionName
{
    unspecified,
    short_name,
    long_name
};


/// Abstract Base class for Options
/**
 * Base class for Options
 * holds just configuration data, no runtime data.
 * Option is not bound to a special type "T"
 */
class Option
{
    friend class OptionParser;

public:
    /// Construct an Option
    /// @param short_name the options's short name. Must be empty or one character.
    /// @param long_name the option's long name. Can be empty.
    /// @param description the Option's description that will be shown in the help message
    Option(const std::string& short_name, const std::string& long_name, std::string description);

    /// Destructor
    virtual ~Option() = default;

    /// default copy constructor
    Option(const Option&) = default;

    /// default move constructor
    Option(Option&&) = default;

    /// default assignement operator
    Option& operator=(const Option&) = default;

    /// default move assignement operator
    Option& operator=(Option&&) = default;

    /// Get the Option's short name
    /// @return character of the options's short name or 0 if no short name is defined
    char short_name() const;

    /// Get the Option's long name
    /// @return the long name of the Option. Empty string if no long name is defined
    std::string long_name() const;

    /// Get the Option's long or short name
    /// @param what_name the option's name to return
    /// @param what_hyphen preced the returned name with (double-)hypen
    /// @return the requested name of the Option. Empty string if not defined.
    std::string name(OptionName what_name, bool with_hypen = false) const;

    /// Get the Option's description
    /// @return the description
    std::string description() const;

    /// Get the Option's default value
    /// @param out stream to write the default value to
    /// @return true if a default value is available, false if not
    virtual bool get_default(std::ostream& out) const = 0;

    /// Set the Option's attribute
    /// @param attribute
    void set_attribute(const Attribute& attribute);

    /// Get the Option's attribute
    /// @return the Options's attribute
    Attribute attribute() const;

    /// Get the Option's argument type
    /// @return argument type (no, required, optional)
    virtual Argument argument_type() const = 0;

    /// Check how often the Option is set on command line
    /// @return the Option's count on command line
    virtual size_t count() const = 0;

    /// Check if the Option is set
    /// @return true if set at least once
    virtual bool is_set() const = 0;


protected:
    /// Parse the command line option and fill the internal data structure
    /// @param what_name short or long option name
    /// @param value the value as given on command line
    virtual void parse(OptionName what_name, const char* value) = 0;

    /// Clear the internal data structure
    virtual void clear() = 0;

    std::string short_name_;
    std::string long_name_;
    std::string description_;
    Attribute attribute_;
};



/// Value option with optional default value
/**
 * Value option with optional default value
 * If set, it requires an argument
 */
template <class T>
class Value : public Option
{
public:
    /// Construct an Value Option
    /// @param short_name the option's short name. Must be empty or one character.
    /// @param long_name the option's long name. Can be empty.
    /// @param description the Option's description that will be shown in the help message
    Value(const std::string& short_name, const std::string& long_name, const std::string& description);

    /// Construct an Value Option
    /// @param short_name the option's short name. Must be empty or one character.
    /// @param long_name the option's long name. Can be empty.
    /// @param description the Option's description that will be shown in the help message
    /// @param default_val the Option's default value
    /// @param assign_to pointer to a variable to assign the parsed command line value to
    Value(const std::string& short_name, const std::string& long_name, const std::string& description, const T& default_val, T* assign_to = nullptr);

    size_t count() const override;
    bool is_set() const override;

    /// Assign the last parsed command line value to "var"
    /// @param var pointer to the variable where is value is written to
    void assign_to(T* var);

    /// Manually set the Option's value. Deletes current value(s)
    /// @param value the new value of the option
    void set_value(const T& value);

    /// Get the Option's value. Will throw if option at index idx is not available
    /// @param idx the zero based index of the value (if set multiple times)
    /// @return the Option's value at index "idx"
    T value(size_t idx = 0) const;

    /// Get the Option's value, return default_value if not set.
    /// @param default_value return value if value is not set
    /// @param idx the zero based index of the value (if set multiple times)
    /// @return the Option's value at index "idx" or the default value or default_value
    T value_or(const T& default_value, size_t idx = 0) const;

    /// Set the Option's default value
    /// @param value the default value if not specified on command line
    void set_default(const T& value);

    /// Check if the Option has a default value
    /// @return true if the Option has a default value
    bool has_default() const;

    /// Get the Option's default value. Will throw if no default is set.
    /// @return the Option's default value
    T get_default() const;
    bool get_default(std::ostream& out) const override;

    Argument argument_type() const override;

protected:
    void parse(OptionName what_name, const char* value) override;
    std::unique_ptr<T> default_;

    virtual void update_reference();
    virtual void add_value(const T& value);
    void clear() override;

    T* assign_to_;
    std::vector<T> values_;
};



/// Value option with implicit default value
/**
 * Value option with implicit default value
 * If set, an argument is optional
 * -without argument it carries the implicit default value
 * -with argument it carries the explicit value
 */
template <class T>
class Implicit : public Value<T>
{
public:
    Implicit(const std::string& short_name, const std::string& long_name, const std::string& description, const T& implicit_val, T* assign_to = nullptr);

    Argument argument_type() const override;

protected:
    void parse(OptionName what_name, const char* value) override;
};



/// Value option without value
/**
 * Value option without value
 * Does not require an argument
 * Can be either set or not set
 */
class Switch : public Value<bool>
{
public:
    Switch(const std::string& short_name, const std::string& long_name, const std::string& description, bool* assign_to = nullptr);

    void set_default(const bool& value) = delete;
    Argument argument_type() const override;

protected:
    void parse(OptionName what_name, const char* value) override;
};



using Option_ptr = std::shared_ptr<Option>;

/// OptionParser manages all Options
/**
 * OptionParser manages all Options
 * Add Options (Option_Type = Value<T>, Implicit<T> or Switch) with "add<Option_Type>(option params)"
 * Call "parse(argc, argv)" to trigger parsing of the options and to
 * fill "non_option_args" and "unknown_options"
 */
class OptionParser
{
public:
    /// Construct the OptionParser
    /// @param description used for the help message
    explicit OptionParser(std::string description = "");

    /// Destructor
    virtual ~OptionParser() = default;

    /// Add an Option e.g. 'add<Value<int>>("i", "int", "description for the -i option")'
    /// @param T the option type (Value, Switch, Implicit)
    /// @param attribute the Option's attribute (inactive, hidden, required, optional, ...)
    /// @param Ts the Option's parameter
    template <typename T, Attribute attribute, typename... Ts>
    std::shared_ptr<T> add(Ts&&... params);

    /// Add an Option e.g. 'add<Value<int>>("i", "int", "description for the -i option")'
    /// @param T the option type (Value, Switch, Implicit)
    /// @param Ts the Option's parameter
    template <typename T, typename... Ts>
    std::shared_ptr<T> add(Ts&&... params);

    /// Parse an ini file into the added Options
    /// @param ini_filename full path of the ini file
    void parse(const std::string& ini_filename);

    /// Parse the command line into the added Options
    /// @param argc command line argument count
    /// @param argv command line arguments
    void parse(int argc, const char* const argv[]);

    /// Delete all parsed options
    void reset();

    /// Produce a help message
    /// @param max_attribute show options up to this level (optional, advanced, expert)
    /// @return the help message
    std::string help(const Attribute& max_attribute = Attribute::optional) const;

    /// Get the OptionParser's description
    /// @return the description as given during construction
    std::string description() const;

    /// Get all options that where added with "add"
    /// @return a vector of the contained Options
    const std::vector<Option_ptr>& options() const;

    /// Get command line arguments without option
    /// e.g. "-i 5 hello" => hello
    /// e.g. "-i 5 -- from here non option args" => "from", "here", "non", "option", "args"
    /// @return vector to "stand-alone" command line arguments
    const std::vector<std::string>& non_option_args() const;

    /// Get unknown command options
    /// e.g. '--some_unknown_option="hello"'
    /// @return vector to "stand-alone" command line arguments
    const std::vector<std::string>& unknown_options() const;

    /// Get an Option by it's long name
    /// @param the Option's long name
    /// @return a pointer of type "Value, Switch, Implicit" to the Option or nullptr
    template <typename T>
    std::shared_ptr<T> get_option(const std::string& long_name) const;

    /// Get an Option by it's short name
    /// @param the Option's short name
    /// @return a pointer of type "Value, Switch, Implicit" to the Option or nullptr
    template <typename T>
    std::shared_ptr<T> get_option(char short_name) const;

protected:
    std::vector<Option_ptr> options_;
    std::string description_;
    std::vector<std::string> non_option_args_;
    std::vector<std::string> unknown_options_;

    Option_ptr find_option(const std::string& long_name) const;
    Option_ptr find_option(char short_name) const;
};



class invalid_option : public std::invalid_argument
{
public:
    enum class Error
    {
        missing_argument,
        invalid_argument,
        too_many_arguments,
        missing_option
    };

    invalid_option(const Option* option, invalid_option::Error error, OptionName what_name, std::string value, const std::string& text)
        : std::invalid_argument(text.c_str()), option_(option), error_(error), what_name_(what_name), value_(std::move(value))
    {
    }

    invalid_option(const Option* option, invalid_option::Error error, const std::string& text)
        : invalid_option(option, error, OptionName::unspecified, "", text)
    {
    }

    const Option* option() const
    {
        return option_;
    }

    Error error() const
    {
        return error_;
    }

    OptionName what_name() const
    {
        return what_name_;
    }

    std::string value() const
    {
        return value_;
    }

private:
    const Option* option_;
    Error error_;
    OptionName what_name_;
    std::string value_;
};



/// Base class for an OptionPrinter
/**
 * OptionPrinter creates a help message for a given OptionParser
 */
class OptionPrinter
{
public:
    /// Constructor
    /// @param option_parser the OptionParser to create the help message from
    explicit OptionPrinter(const OptionParser* option_parser) : option_parser_(option_parser)
    {
    }

    /// Destructor
    virtual ~OptionPrinter() = default;

    /// Create a help message
    /// @param max_attribute show options up to this level (optional, advanced, expert)
    /// @return the help message
    virtual std::string print(const Attribute& max_attribute = Attribute::optional) const = 0;

protected:
    const OptionParser* option_parser_;
};



/// Option printer for the console
/**
 * Standard console option printer
 * Creates a human readable help message
 */
class ConsoleOptionPrinter : public OptionPrinter
{
public:
    explicit ConsoleOptionPrinter(const OptionParser* option_parser);
    ~ConsoleOptionPrinter() override = default;

    std::string print(const Attribute& max_attribute = Attribute::optional) const override;

private:
    std::string to_string(Option_ptr option) const;
};



/// Option printer for man pages
/**
 * Creates help messages in groff format that can be used in man pages
 */
class GroffOptionPrinter : public OptionPrinter
{
public:
    explicit GroffOptionPrinter(const OptionParser* option_parser);
    ~GroffOptionPrinter() override = default;

    std::string print(const Attribute& max_attribute = Attribute::optional) const override;

private:
    std::string to_string(Option_ptr option) const;
};



/// Option printer for bash completion
/**
 * Creates a script with all options (short and long) that can be used for bash completion
 */
class BashCompletionOptionPrinter : public OptionPrinter
{
public:
    BashCompletionOptionPrinter(const OptionParser* option_parser, std::string program_name);
    ~BashCompletionOptionPrinter() override = default;

    std::string print(const Attribute& max_attribute = Attribute::optional) const override;

private:
    std::string program_name_;
};



/// Option implementation /////////////////////////////////

inline Option::Option(const std::string& short_name, const std::string& long_name, std::string description)
    : short_name_(short_name), long_name_(long_name), description_(std::move(description)), attribute_(Attribute::optional)
{
    if (short_name.size() > 1)
        throw std::invalid_argument("length of short name must be <= 1: '" + short_name + "'");

    if (short_name.empty() && long_name.empty())
        throw std::invalid_argument("short and long name are empty");
}


inline char Option::short_name() const
{
    if (!short_name_.empty())
        return short_name_[0];
    return 0;
}


inline std::string Option::long_name() const
{
    return long_name_;
}


inline std::string Option::name(OptionName what_name, bool with_hypen) const
{
    if (what_name == OptionName::short_name)
        return short_name_.empty() ? "" : ((with_hypen ? "-" : "") + short_name_);
    if (what_name == OptionName::long_name)
        return long_name_.empty() ? "" : ((with_hypen ? "--" : "") + long_name_);
    return "";
}


inline std::string Option::description() const
{
    return description_;
}


inline void Option::set_attribute(const Attribute& attribute)
{
    attribute_ = attribute;
}


inline Attribute Option::attribute() const
{
    return attribute_;
}



/// Value implementation /////////////////////////////////

template <class T>
inline Value<T>::Value(const std::string& short_name, const std::string& long_name, const std::string& description)
    : Option(short_name, long_name, description), assign_to_(nullptr)
{
}


template <class T>
inline Value<T>::Value(const std::string& short_name, const std::string& long_name, const std::string& description, const T& default_val, T* assign_to)
    : Value<T>(short_name, long_name, description)
{
    assign_to_ = assign_to;
    set_default(default_val);
}


template <class T>
inline size_t Value<T>::count() const
{
    return values_.size();
}


template <class T>
inline bool Value<T>::is_set() const
{
    return !values_.empty();
}


template <class T>
inline void Value<T>::assign_to(T* var)
{
    assign_to_ = var;
    update_reference();
}


template <class T>
inline void Value<T>::set_value(const T& value)
{
    clear();
    add_value(value);
}

template <class T>
inline T Value<T>::value_or(const T& default_value, size_t idx) const
{
    if (idx < values_.size())
        return values_[idx];
    else if (default_)
        return *default_;
    else
        return default_value;
}

template <class T>
inline T Value<T>::value(size_t idx) const
{
    if (!this->is_set() && default_)
        return *default_;

    if (!is_set() || (idx >= count()))
    {
        std::stringstream optionStr;
        if (!is_set())
            optionStr << "option not set: \"";
        else
            optionStr << "index out of range (" << idx << ") for \"";

        if (short_name() != 0)
            optionStr << "-" << short_name();
        else
            optionStr << "--" << long_name();

        optionStr << "\"";
        throw std::out_of_range(optionStr.str());
    }

    return values_[idx];
}



template <class T>
inline void Value<T>::set_default(const T& value)
{
    this->default_.reset(new T);
    *this->default_ = value;
    update_reference();
}


template <class T>
inline bool Value<T>::has_default() const
{
    return (this->default_ != nullptr);
}


template <class T>
inline T Value<T>::get_default() const
{
    if (!has_default())
        throw std::runtime_error("no default value set");
    return *this->default_;
}


template <class T>
inline bool Value<T>::get_default(std::ostream& out) const
{
    if (!has_default())
        return false;
    out << *this->default_;
    return true;
}


template <class T>
inline Argument Value<T>::argument_type() const
{
    return Argument::required;
}


template <>
inline void Value<std::string>::parse(OptionName what_name, const char* value)
{
    if (strlen(value) == 0)
        throw invalid_option(this, invalid_option::Error::missing_argument, what_name, value, "missing argument for " + name(what_name, true));

    add_value(value);
}


template <>
inline void Value<bool>::parse(OptionName /*what_name*/, const char* value)
{
    bool val =
        ((value != nullptr) && ((strcmp(value, "1") == 0) || (strcmp(value, "true") == 0) || (strcmp(value, "True") == 0) || (strcmp(value, "TRUE") == 0)));
    add_value(val);
}


template <class T>
inline void Value<T>::parse(OptionName what_name, const char* value)
{
    T parsed_value;
    std::string strValue;
    if (value != nullptr)
        strValue = value;

    std::istringstream is(strValue);
    int valuesRead = 0;
    while (is.good())
    {
        if (is.peek() != EOF)
            is >> parsed_value;
        else
            break;

        valuesRead++;
    }

    if (is.fail())
        throw invalid_option(this, invalid_option::Error::invalid_argument, what_name, value,
                             "invalid argument for " + name(what_name, true) + ": '" + strValue + "'");

    if (valuesRead > 1)
        throw invalid_option(this, invalid_option::Error::too_many_arguments, what_name, value,
                             "too many arguments for " + name(what_name, true) + ": '" + strValue + "'");

    if (strValue.empty())
        throw invalid_option(this, invalid_option::Error::missing_argument, what_name, "", "missing argument for " + name(what_name, true));

    this->add_value(parsed_value);
}


template <class T>
inline void Value<T>::update_reference()
{
    if (this->assign_to_)
    {
        if (!this->is_set() && default_)
            *this->assign_to_ = *default_;
        else if (this->is_set())
            *this->assign_to_ = values_.back();
    }
}


template <class T>
inline void Value<T>::add_value(const T& value)
{
    values_.push_back(value);
    update_reference();
}


template <class T>
inline void Value<T>::clear()
{
    values_.clear();
    update_reference();
}



/// Implicit implementation /////////////////////////////////

template <class T>
inline Implicit<T>::Implicit(const std::string& short_name, const std::string& long_name, const std::string& description, const T& implicit_val, T* assign_to)
    : Value<T>(short_name, long_name, description, implicit_val, assign_to)
{
}


template <class T>
inline Argument Implicit<T>::argument_type() const
{
    return Argument::optional;
}


template <class T>
inline void Implicit<T>::parse(OptionName what_name, const char* value)
{
    if ((value != nullptr) && (strlen(value) > 0))
        Value<T>::parse(what_name, value);
    else
        this->add_value(*this->default_);
}



/// Switch implementation /////////////////////////////////

inline Switch::Switch(const std::string& short_name, const std::string& long_name, const std::string& description, bool* assign_to)
    : Value<bool>(short_name, long_name, description, false, assign_to)
{
}


inline void Switch::parse(OptionName /*what_name*/, const char* /*value*/)
{
    add_value(true);
}


inline Argument Switch::argument_type() const
{
    return Argument::no;
}



/// OptionParser implementation /////////////////////////////////

inline OptionParser::OptionParser(std::string description) : description_(std::move(description))
{
}


template <typename T, typename... Ts>
inline std::shared_ptr<T> OptionParser::add(Ts&&... params)
{
    return add<T, Attribute::optional>(std::forward<Ts>(params)...);
}


template <typename T, Attribute attribute, typename... Ts>
inline std::shared_ptr<T> OptionParser::add(Ts&&... params)
{
    static_assert(std::is_base_of<Option, typename std::decay<T>::type>::value, "type T must be Switch, Value or Implicit");
    std::shared_ptr<T> option = std::make_shared<T>(std::forward<Ts>(params)...);

    for (const auto& o : options_)
    {
        if ((option->short_name() != 0) && (option->short_name() == o->short_name()))
            throw std::invalid_argument("duplicate short option name '-" + std::string(1, option->short_name()) + "'");
        if (!option->long_name().empty() && (option->long_name() == (o->long_name())))
            throw std::invalid_argument("duplicate long option name '--" + option->long_name() + "'");
    }
    option->set_attribute(attribute);
    options_.push_back(option);
    return option;
}


inline std::string OptionParser::description() const
{
    return description_;
}


inline const std::vector<Option_ptr>& OptionParser::options() const
{
    return options_;
}


inline const std::vector<std::string>& OptionParser::non_option_args() const
{
    return non_option_args_;
}


inline const std::vector<std::string>& OptionParser::unknown_options() const
{
    return unknown_options_;
}


inline Option_ptr OptionParser::find_option(const std::string& long_name) const
{
    for (const auto& option : options_)
        if (option->long_name() == long_name)
            return option;
    return nullptr;
}


inline Option_ptr OptionParser::find_option(char short_name) const
{
    for (const auto& option : options_)
        if (option->short_name() == short_name)
            return option;
    return nullptr;
}


template <typename T>
inline std::shared_ptr<T> OptionParser::get_option(const std::string& long_name) const
{
    Option_ptr option = find_option(long_name);
    if (!option)
        throw std::invalid_argument("option not found: " + long_name);
    auto result = std::dynamic_pointer_cast<T>(option);
    if (!result)
        throw std::invalid_argument("cannot cast option to T: " + long_name);
    return result;
}


template <typename T>
inline std::shared_ptr<T> OptionParser::get_option(char short_name) const
{
    Option_ptr option = find_option(short_name);
    if (!option)
        throw std::invalid_argument("option not found: " + std::string(1, short_name));
    auto result = std::dynamic_pointer_cast<T>(option);
    if (!result)
        throw std::invalid_argument("cannot cast option to T: " + std::string(1, short_name));
    return result;
}

inline void OptionParser::parse(const std::string& ini_filename)
{
    std::ifstream file(ini_filename.c_str());
    std::string line;

    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
        return s;
    };

    auto trim_copy = [trim](const std::string& s) {
        std::string copy(s);
        return trim(copy);
    };

    auto split = [trim_copy](const std::string& s) -> std::pair<std::string, std::string> {
        size_t pos = s.find('=');
        if (pos == std::string::npos)
            return {"", ""};
        return {trim_copy(s.substr(0, pos)), trim_copy(s.substr(pos + 1, std::string::npos))};
    };

    std::string section;
    while (std::getline(file, line))
    {
        trim(line);
        if (line.empty())
            continue;
        if (line.front() == '#')
            continue;

        if ((line.front() == '[') && (line.back() == ']'))
        {
            section = trim_copy(line.substr(1, line.size() - 2));
            continue;
        }
        auto key_value = split(line);
        if (key_value.first.empty())
            continue;

        std::string key = section.empty() ? key_value.first : section + "." + key_value.first;
        Option_ptr option = find_option(key);
        if (option && (option->attribute() == Attribute::inactive))
            option = nullptr;

        if (option)
            option->parse(OptionName::long_name, key_value.second.c_str());
        else
            unknown_options_.push_back(key);
    }
}

inline void OptionParser::parse(int argc, const char* const argv[])
{
    for (int n = 1; n < argc; ++n)
    {
        const std::string arg(argv[n]);
        if (arg == "--")
        {
            /// from here on only non opt args
            for (int m = n + 1; m < argc; ++m)
                non_option_args_.emplace_back(argv[m]);
        }
        else if (arg.find("--") == 0)
        {
            /// long option arg
            std::string opt = arg.substr(2);
            std::string optarg;
            size_t equalIdx = opt.find('=');
            if (equalIdx != std::string::npos)
            {
                optarg = opt.substr(equalIdx + 1);
                opt.resize(equalIdx);
            }

            Option_ptr option = find_option(opt);
            if (option && (option->attribute() == Attribute::inactive))
                option = nullptr;
            if (option)
            {
                if (option->argument_type() == Argument::no)
                {
                    if (!optarg.empty())
                        option = nullptr;
                }
                else if (option->argument_type() == Argument::required)
                {
                    if (optarg.empty() && n < argc - 1)
                        optarg = argv[++n];
                }
            }

            if (option)
                option->parse(OptionName::long_name, optarg.c_str());
            else
                unknown_options_.push_back(arg);
        }
        else if (arg.find('-') == 0)
        {
            /// short option arg
            std::string opt = arg.substr(1);
            bool unknown = false;
            for (size_t m = 0; m < opt.size(); ++m)
            {
                char c = opt[m];
                std::string optarg;

                Option_ptr option = find_option(c);
                if (option && (option->attribute() == Attribute::inactive))
                    option = nullptr;
                if (option)
                {
                    if (option->argument_type() == Argument::required)
                    {
                        /// use the rest of the current argument as optarg
                        optarg = opt.substr(m + 1);
                        /// or the next arg
                        if (optarg.empty() && n < argc - 1)
                            optarg = argv[++n];
                        m = opt.size();
                    }
                    else if (option->argument_type() == Argument::optional)
                    {
                        /// use the rest of the current argument as optarg
                        optarg = opt.substr(m + 1);
                        m = opt.size();
                    }
                }

                if (option)
                    option->parse(OptionName::short_name, optarg.c_str());
                else
                    unknown = true;
            }
            if (unknown)
                unknown_options_.push_back(arg);
        }
        else
        {
            non_option_args_.push_back(arg);
        }
    }

    for (auto& opt : options_)
    {
        if ((opt->attribute() == Attribute::required) && !opt->is_set())
        {
            std::string option = opt->long_name().empty() ? std::string(1, opt->short_name()) : opt->long_name();
            throw invalid_option(opt.get(), invalid_option::Error::missing_option, "option \"" + option + "\" is required");
        }
    }
}


inline void OptionParser::reset()
{
    unknown_options_.clear();
    non_option_args_.clear();
    for (auto& opt : options_)
        opt->clear();
}


inline std::string OptionParser::help(const Attribute& max_attribute) const
{
    ConsoleOptionPrinter option_printer(this);
    return option_printer.print(max_attribute);
}



/// ConsoleOptionPrinter implementation /////////////////////////////////

inline ConsoleOptionPrinter::ConsoleOptionPrinter(const OptionParser* option_parser) : OptionPrinter(option_parser)
{
}


inline std::string ConsoleOptionPrinter::to_string(Option_ptr option) const
{
    std::stringstream line;
    if (option->short_name() != 0)
    {
        line << "  -" << option->short_name();
        if (!option->long_name().empty())
            line << ", ";
    }
    else
        line << "  ";
    if (!option->long_name().empty())
        line << "--" << option->long_name();

    if (option->argument_type() == Argument::required)
    {
        line << " arg";
        std::stringstream defaultStr;
        if (option->get_default(defaultStr))
        {
            if (!defaultStr.str().empty())
                line << " (=" << defaultStr.str() << ")";
        }
    }
    else if (option->argument_type() == Argument::optional)
    {
        std::stringstream defaultStr;
        if (option->get_default(defaultStr))
            line << " [=arg(=" << defaultStr.str() << ")]";
    }

    return line.str();
}


inline std::string ConsoleOptionPrinter::print(const Attribute& max_attribute) const
{
    if (option_parser_ == nullptr)
        return "";

    if (max_attribute < Attribute::optional)
        throw std::invalid_argument("attribute must be 'optional', 'advanced', or 'default'");

    std::stringstream s;
    if (!option_parser_->description().empty())
        s << option_parser_->description() << ":\n";

    size_t optionRightMargin(20);
    const size_t maxDescriptionLeftMargin(40);
    //	const size_t descriptionRightMargin(80);

    for (const auto& option : option_parser_->options())
        optionRightMargin = std::max(optionRightMargin, to_string(option).size() + 2);
    optionRightMargin = std::min(maxDescriptionLeftMargin - 2, optionRightMargin);

    for (const auto& option : option_parser_->options())
    {
        if ((option->attribute() <= Attribute::hidden) || (option->attribute() > max_attribute))
            continue;
        std::string optionStr = to_string(option);
        if (optionStr.size() < optionRightMargin)
            optionStr.resize(optionRightMargin, ' ');
        else
            optionStr += "\n" + std::string(optionRightMargin, ' ');
        s << optionStr;

        std::string line;
        std::vector<std::string> lines;
        std::stringstream description(option->description());
        while (std::getline(description, line, '\n'))
            lines.push_back(line);

        std::string empty(optionRightMargin, ' ');
        for (size_t n = 0; n < lines.size(); ++n)
        {
            if (n > 0)
                s << "\n" << empty;
            s << lines[n];
        }
        s << "\n";
    }

    return s.str();
}



/// GroffOptionPrinter implementation /////////////////////////////////

inline GroffOptionPrinter::GroffOptionPrinter(const OptionParser* option_parser) : OptionPrinter(option_parser)
{
}


inline std::string GroffOptionPrinter::to_string(Option_ptr option) const
{
    std::stringstream line;
    if (option->short_name() != 0)
    {
        line << "-" << option->short_name();
        if (!option->long_name().empty())
            line << ", ";
    }
    if (!option->long_name().empty())
        line << "--" << option->long_name();

    if (option->argument_type() == Argument::required)
    {
        line << " arg";
        std::stringstream defaultStr;
        if (option->get_default(defaultStr))
        {
            if (!defaultStr.str().empty())
                line << " (=" << defaultStr.str() << ")";
        }
    }
    else if (option->argument_type() == Argument::optional)
    {
        std::stringstream defaultStr;
        if (option->get_default(defaultStr))
            line << " [=arg(=" << defaultStr.str() << ")]";
    }

    return line.str();
}


inline std::string GroffOptionPrinter::print(const Attribute& max_attribute) const
{
    if (option_parser_ == nullptr)
        return "";

    if (max_attribute < Attribute::optional)
        throw std::invalid_argument("attribute must be 'optional', 'advanced', or 'default'");

    std::stringstream s;
    if (!option_parser_->description().empty())
        s << ".SS " << option_parser_->description() << ":\n";

    for (const auto& option : option_parser_->options())
    {
        if ((option->attribute() <= Attribute::hidden) || (option->attribute() > max_attribute))
            continue;
        s << ".TP\n\\fB" << to_string(option) << "\\fR\n";
        if (!option->description().empty())
            s << option->description() << "\n";
    }

    return s.str();
}



/// BashCompletionOptionPrinter implementation /////////////////////////////////

inline BashCompletionOptionPrinter::BashCompletionOptionPrinter(const OptionParser* option_parser, std::string program_name)
    : OptionPrinter(option_parser), program_name_(std::move(program_name))
{
}


inline std::string BashCompletionOptionPrinter::print(const Attribute& /*max_attribute*/) const
{
    if (option_parser_ == nullptr)
        return "";

    std::stringstream s;
    s << "_" << program_name_ << "()\n";
    s << R"({
	local cur prev opts
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	opts=")";

    for (const auto& option : option_parser_->options())
    {
        if (option->attribute() > Attribute::hidden)
        {
            if (option->short_name() != 0)
                s << "-" << option->short_name() << " ";
            if (!option->long_name().empty())
                s << "--" << option->long_name() << " ";
        }
    }

    s << R"("
	if [[ ${cur} == -* ]] ; then
		COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
		return 0
	fi
}
complete -F )";
    s << "_" << program_name_ << " " << program_name_ << "\n";

    return s.str();
}



static inline std::ostream& operator<<(std::ostream& out, const OptionParser& op)
{
    return out << op.help();
}


} // namespace popl


#endif // POPL_HPP
