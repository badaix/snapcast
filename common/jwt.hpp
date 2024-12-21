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
#include "common/json.hpp"

// standard headers
#include <chrono>
#include <optional>
#include <string>


/*
https://datatracker.ietf.org/doc/html/rfc7518#section-3

   +--------------+-------------------------------+--------------------+
   | "alg" Param  | Digital Signature or MAC      | Implementation     |
   | Value        | Algorithm                     | Requirements       |
   +--------------+-------------------------------+--------------------+
   | HS256        | HMAC using SHA-256            | Required           |
   | HS384        | HMAC using SHA-384            | Optional           |
   | HS512        | HMAC using SHA-512            | Optional           |
   | RS256        | RSASSA-PKCS1-v1_5 using       | Recommended        |
   |              | SHA-256                       |                    |
   | RS384        | RSASSA-PKCS1-v1_5 using       | Optional           |
   |              | SHA-384                       |                    |
   | RS512        | RSASSA-PKCS1-v1_5 using       | Optional           |
   |              | SHA-512                       |                    |
   | ES256        | ECDSA using P-256 and SHA-256 | Recommended+       |
   | ES384        | ECDSA using P-384 and SHA-384 | Optional           |
   | ES512        | ECDSA using P-521 and SHA-512 | Optional           |
   | PS256        | RSASSA-PSS using SHA-256 and  | Optional           |
   |              | MGF1 with SHA-256             |                    |
   | PS384        | RSASSA-PSS using SHA-384 and  | Optional           |
   |              | MGF1 with SHA-384             |                    |
   | PS512        | RSASSA-PSS using SHA-512 and  | Optional           |
   |              | MGF1 with SHA-512             |                    |
   | none         | No digital signature or MAC   | Optional           |
   |              | performed                     |                    |
   +--------------+-------------------------------+--------------------+


https://auth0.com/docs/secure/tokens/json-web-tokens/json-web-token-claims

Registered claims
The JWT specification defines seven reserved claims that are not required, but are recommended to allow interoperability with third-party applications. These
are:
- iss (issuer): Issuer of the JWT
- sub (subject): Subject of the JWT (the user)
- aud (audience): Recipient for which the JWT is intended
- exp (expiration time): Time after which the JWT expires
- nbf (not before time): Time before which the JWT must not be accepted for processing
- iat (issued at time): Time at which the JWT was issued; can be used to determine age of the JWT
- jti (JWT ID): Unique identifier; can be used to prevent the JWT from being replayed (allows a token to be used only once)


// https://techdocs.akamai.com/iot-token-access-control/docs/generate-jwt-rsa-keys
*/

using json = nlohmann::json;


/// Json Web Token in RS256 format
class Jwt
{
public:
    /// c'tor
    Jwt();

    /// Parse an base64 url encoded token of the form "<header>.<payload>.<signature>"
    /// @param token The token
    /// @param pem_cert Certificate in PEM format to verify the signature
    /// @return true on success, else false
    bool parse(const std::string& token, const std::string& pem_cert);
    /// Create an base64 url encoded token of the form "<header>.<payload>.<signature>"
    /// @param pem_key Private key in PEM format to sign the token
    /// @return the token or nullopt if failed
    std::optional<std::string> getToken(const std::string& pem_key) const;

    /// Get the iat "Issued at time" claim
    /// @return the claim or nullopt, if not present
    std::optional<std::chrono::system_clock::time_point> getIat() const;
    /// Set the iat "Issued at time" claim, use nullopt to delete the iat
    void setIat(const std::optional<std::chrono::system_clock::time_point>& iat);

    /// Get the exp "Expiration time" claim
    /// @return the claim or nullopt, if not present
    std::optional<std::chrono::system_clock::time_point> getExp() const;
    /// Set the exp "Expiration time" claim, use nullopt to delete the exp
    void setExp(const std::optional<std::chrono::system_clock::time_point>& exp);

    /// Get the sub "Subject" claim
    /// @return the claim or nullopt, if not present
    std::optional<std::string> getSub() const;
    /// Set the sub "Subject" claim, use nullopt to delete the sub
    void setSub(const std::optional<std::string>& sub);

    /// The token's raw payload (claims) in json format
    json claims;
};
