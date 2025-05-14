/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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

#pragma once

// standard headers
#include <optional>
#include <string>
#include <system_error>
#include <variant>


namespace snapcast
{

/// Error codes with detail information
struct ErrorCode : public std::error_code
{
    /// c'tor
    ErrorCode() : std::error_code(), detail_(std::nullopt)
    {
    }

    /// c'tor
    /// @param code the std error code
    ErrorCode(const std::error_code& code) : std::error_code(code), detail_(std::nullopt)
    {
    }

    /// c'tor
    /// @param code the std error code
    /// @param detail additional details
    ErrorCode(const std::error_code& code, std::string detail) : std::error_code(code), detail_(std::move(detail))
    {
    }

    /// Move c'tor
    ErrorCode(ErrorCode&& other) = default;
    /// Move assignment
    ErrorCode& operator=(ErrorCode&& rhs) = default;

    /// @return detaiöed error message
    std::string detailed_message() const
    {
        if (detail_.has_value())
            return message() + ": " + *detail_;
        return message();
    }

private:
    /// Optional error detais
    std::optional<std::string> detail_;
};


/// Storage for an ErrorCode or a type T
/// Mostly used as return type
template <class T>
struct ErrorOr
{
    /// Move construct with @p t
    ErrorOr(T&& t) : var(std::move(t))
    {
    }

    /// Construct with @p t
    ErrorOr(const T& t) : var(t)
    {
    }

    /// Move construct with an error
    ErrorOr(ErrorCode&& error) : var(std::move(error))
    {
    }

    /// Construct with an error
    ErrorOr(const ErrorCode& error) : var(error)
    {
    }

    /// @return true if contains a value (i.e. no error)
    bool hasValue()
    {
        return std::holds_alternative<T>(var);
    }

    /// @return true if contains an error (i.e. no value)
    bool hasError()
    {
        return std::holds_alternative<ErrorCode>(var);
    }

    /// @return the value
    const T& getValue()
    {
        return std::get<T>(var);
    }

    /// @return the moved value
    T takeValue()
    {
        return std::move(std::get<T>(var));
    }

    /// @return the error
    const ErrorCode& getError()
    {
        return std::get<ErrorCode>(var);
    }

    /// @return the moved error
    ErrorCode takeError()
    {
        auto ec = std::move(std::get<ErrorCode>(var));
        return ec;
    }

private:
    /// The stored ErrorCode or value
    std::variant<ErrorCode, T> var;
};


} // namespace snapcast
