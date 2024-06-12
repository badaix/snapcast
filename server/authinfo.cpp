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
#include <chrono>
#include <fstream>
#include <string>
#include <string_view>


using namespace std;

static constexpr auto LOG_TAG = "AuthInfo";

AuthInfo::AuthInfo(std::string authheader)
{
    LOG(INFO, LOG_TAG) << "Authorization: " << authheader << "\n";
    std::string token(std::move(authheader));
    static constexpr auto bearer = "bearer"sv;
    auto pos = utils::string::tolower_copy(token).find(bearer);
    if (pos != string::npos)
    {
        token = token.erase(0, pos + bearer.length());
        utils::string::trim(token);
        std::ifstream ifs("certs/snapserver.crt");
        std::string certificate((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        Jwt jwt;
        jwt.parse(token, certificate);
        if (jwt.getExp().has_value())
            expires_ = jwt.getExp().value();
        username_ = jwt.getSub().value_or("");
        LOG(INFO, LOG_TAG) << "Authorization token: " << token << ", user: " << username_ << ", claims: " << jwt.claims.dump() << "\n";
    }
    static constexpr auto basic = "basic"sv;
    pos = utils::string::tolower_copy(token).find(basic);
    if (pos != string::npos)
    {
        token = token.erase(0, pos + basic.length());
        utils::string::trim(token);
        username_ = base64_decode(token);
        std::string password;
        username_ = utils::string::split_left(username_, ':', password);
        LOG(INFO, LOG_TAG) << "Authorization basic: " << token << ", user: " << username_ << ", password: " << password << "\n";
    }
}


bool AuthInfo::valid() const
{
    if (expires_.has_value())
    {
        LOG(INFO, LOG_TAG) << "Expires in " << std::chrono::duration_cast<std::chrono::seconds>(expires_.value() - std::chrono::system_clock::now()).count()
                           << " sec\n";
        return expires_ > std::chrono::system_clock::now();
    }
    return true;
}

const std::string& AuthInfo::username() const
{
    return username_;
}
