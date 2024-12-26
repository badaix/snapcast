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

// prototype/interface header file
#include "authinfo.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/base64.h"
#include "common/jwt.hpp"
#include "common/utils/string_utils.hpp"

// 3rd party headers

// standard headers
#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <system_error>


using namespace std;

static constexpr auto LOG_TAG = "AuthInfo";


namespace snapcast::error::auth
{

namespace detail
{

/// Error category for auth errors
struct category : public std::error_category
{
public:
    /// @return category name
    const char* name() const noexcept override;
    /// @return error message for @p value
    std::string message(int value) const override;
};


const char* category::name() const noexcept
{
    return "auth";
}

std::string category::message(int value) const
{
    switch (static_cast<AuthErrc>(value))
    {
        case AuthErrc::auth_scheme_not_supported:
            return "Authentication scheme not supported";
        case AuthErrc::failed_to_create_token:
            return "Failed to create token";
        case AuthErrc::unknown_user:
            return "Unknown user";
        case AuthErrc::wrong_password:
            return "Wrong password";
        case AuthErrc::expired:
            return "Expired";
        case AuthErrc::token_validation_failed:
            return "Token validation failed";
        default:
            return "Unknown";
    }
}

} // namespace detail

const std::error_category& category()
{
    // The category singleton
    static detail::category instance;
    return instance;
}

} // namespace snapcast::error::auth

std::error_code make_error_code(AuthErrc errc)
{
    return std::error_code(static_cast<int>(errc), snapcast::error::auth::category());
}


AuthInfo::AuthInfo(const ServerSettings& settings) : has_auth_info_(false), settings_(settings)
{
}


ErrorCode AuthInfo::validateUser(const std::string& username, const std::optional<std::string>& password) const
{
    auto iter = std::find_if(settings_.users.begin(), settings_.users.end(), [&](const ServerSettings::User& user) { return user.name == username; });
    if (iter == settings_.users.end())
        return ErrorCode{AuthErrc::unknown_user};
    if (password.has_value() && (iter->password != password.value()))
        return ErrorCode{AuthErrc::wrong_password};
    return {};
}


ErrorCode AuthInfo::authenticate(const std::string& scheme, const std::string& param)
{
    std::string scheme_normed = utils::string::trim_copy(utils::string::tolower_copy(scheme));
    std::string param_normed = utils::string::trim_copy(param);
    if (scheme_normed == "bearer")
        return authenticateBearer(param_normed);
    else if (scheme_normed == "basic")
        return authenticateBasic(param_normed);

    return {AuthErrc::auth_scheme_not_supported, "Scheme must be 'Basic' or 'Bearer'"};
}


ErrorCode AuthInfo::authenticate(const std::string& auth)
{
    LOG(INFO, LOG_TAG) << "authenticate: " << auth << "\n";
    std::string param;
    std::string scheme = utils::string::split_left(utils::string::trim_copy(auth), ' ', param);
    return authenticate(scheme, param);
}


ErrorCode AuthInfo::authenticateBasic(const std::string& credentials)
{
    has_auth_info_ = false;
    std::string username = base64_decode(credentials);
    std::string password;
    username_ = utils::string::split_left(username, ':', password);
    auto ec = validateUser(username_, password);

    LOG(INFO, LOG_TAG) << "Authorization basic: " << credentials << ", user: " << username_ << ", password: " << password << "\n";
    has_auth_info_ = (ec.value() == 0);
    return ec;
}


ErrorCode AuthInfo::authenticateBearer(const std::string& token)
{
    has_auth_info_ = false;
    std::ifstream ifs(settings_.ssl.certificate);
    std::string certificate((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    Jwt jwt;
    if (!jwt.parse(token, certificate))
        return {AuthErrc::token_validation_failed};
    if (jwt.getExp().has_value())
        expires_ = jwt.getExp().value();
    username_ = jwt.getSub().value_or("");

    LOG(INFO, LOG_TAG) << "Authorization token: " << token << ", user: " << username_ << ", claims: " << jwt.claims.dump() << "\n";

    if (auto ec = validateUser(username_); ec)
        return ec;

    if (isExpired())
        return {AuthErrc::expired};

    has_auth_info_ = true;
    return {};
}


ErrorOr<std::string> AuthInfo::getToken(const std::string& username, const std::string& password) const
{
    ErrorCode ec = validateUser(username, password);
    if (ec)
        return ec;

    Jwt jwt;
    auto now = std::chrono::system_clock::now();
    jwt.setIat(now);
    jwt.setExp(now + 10h);
    jwt.setSub(username);
    std::ifstream ifs(settings_.ssl.certificate_key);
    std::string certificate_key((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (!ifs.good())
        return ErrorCode{std::make_error_code(std::errc::io_error), "Failed to read private key file"};
    // TODO tls: eroor handling
    std::optional<std::string> token = jwt.getToken(certificate_key);
    if (!token.has_value())
        return ErrorCode{AuthErrc::failed_to_create_token};
    return token.value();
}


bool AuthInfo::isExpired() const
{
    if (expires_.has_value())
    {
        LOG(INFO, LOG_TAG) << "Expires in " << std::chrono::duration_cast<std::chrono::seconds>(expires_.value() - std::chrono::system_clock::now()).count()
                           << " sec\n";
        if (std::chrono::system_clock::now() > expires_.value())
            return true;
    }
    return false;
}


bool AuthInfo::hasAuthInfo() const
{
    return has_auth_info_;
}


// ErrorCode AuthInfo::isValid(const std::string& command) const
// {
//     std::ignore = command;
//     if (isExpired())
//         return {AuthErrc::expired};

//     return {};
// }

const std::string& AuthInfo::username() const
{
    return username_;
}


bool AuthInfo::hasPermission(const std::string& resource) const
{
    if (!hasAuthInfo())
        return false;

    auto iter = std::find_if(settings_.users.begin(), settings_.users.end(), [&](const ServerSettings::User& user) { return user.name == username_; });
    if (iter == settings_.users.end())
        return false;

    auto perm_iter = std::find_if(iter->permissions.begin(), iter->permissions.end(),
                                  [&](const std::string& permission) { return utils::string::wildcardMatch(permission, resource); });
    if (perm_iter != iter->permissions.end())
    {
        LOG(DEBUG, LOG_TAG) << "Found permission for ressource '" << resource << "': '" << *perm_iter << "'\n";
        return true;
    }
    return false;
}
