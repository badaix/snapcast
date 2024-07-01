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

// local headers
#include "common/error_code.hpp"
#include "server_settings.hpp"

// 3rd party headers

// standard headers
#include <chrono>
#include <optional>
#include <string>
#include <system_error>

/// Authentication error codes
enum class AuthErrc
{
    auth_scheme_not_supported = 1,
    failed_to_create_token = 2,
    unknown_user = 3,
    wrong_password = 4,
    expired = 5,
    token_validation_failed = 6,
};

namespace snapcast::error::auth
{
const std::error_category& category();
}



namespace std
{
template <>
struct is_error_code_enum<AuthErrc> : public std::true_type
{
};
} // namespace std

std::error_code make_error_code(AuthErrc);

using snapcast::ErrorCode;
using snapcast::ErrorOr;

/// Authentication Info class
class AuthInfo
{
public:
    /// c'tor
    explicit AuthInfo(const ServerSettings& settings);
    // explicit AuthInfo(std::string authheader);
    /// d'tor
    virtual ~AuthInfo() = default;

    /// @return if authentication info is available
    bool hasAuthInfo() const;
    // ErrorCode isValid(const std::string& command) const;
    /// @return the username
    const std::string& username() const;

    /// Authenticate with basic scheme
    ErrorCode authenticateBasic(const std::string& credentials);
    /// Authenticate with bearer scheme
    ErrorCode authenticateBearer(const std::string& token);
    /// Authenticate with basic or bearer scheme with an auth header
    ErrorCode authenticate(const std::string& auth);
    /// Authenticate with scheme ("basic" or "bearer") and auth param
    ErrorCode authenticate(const std::string& scheme, const std::string& param);

    /// @return JWS token for @p username and @p password
    ErrorOr<std::string> getToken(const std::string& username, const std::string& password) const;
    /// @return if the authenticated user has permission to access @p ressource
    bool hasPermission(const std::string& resource) const;

private:
    /// has auth info
    bool has_auth_info_;
    /// auth user name
    std::string username_;
    /// optional token expiration
    std::optional<std::chrono::system_clock::time_point> expires_;
    /// server configuration
    ServerSettings settings_;

    /// Validate @p username and @p password
    /// @return true if username and password are correct
    ErrorCode validateUser(const std::string& username, const std::optional<std::string>& password = std::nullopt) const;
    /// @return if the authentication is expired
    bool isExpired() const;
};
