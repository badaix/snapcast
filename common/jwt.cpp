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
#include "jwt.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/base64.h"
#include "common/utils/string_utils.hpp"

// 3rd party headers
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

// standard headers
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <memory>
#include <optional>
#include <sys/types.h>
#include <vector>


static constexpr auto LOG_TAG = "JWT";



namespace
{

EVP_PKEY* readKey(const std::string& key)
{
    // Reads PEM information and retrieves some details
    std::shared_ptr<BIO> keybio(BIO_new_mem_buf((void*)key.c_str(), -1), [](auto p) { BIO_free(p); });
    if (keybio == nullptr)
    {
        LOG(ERROR, LOG_TAG) << "BIO_new_mem_buf failed\n";
        return nullptr;
    }

    char* name = nullptr;
    char* header = nullptr;
    uint8_t* data = nullptr;
    long datalen = 0;
    if (PEM_read_bio(keybio.get(), &name, &header, &data, &datalen) == 1)
    {
        // Copies the data pointer. D2I functions update it
        const auto* data_pkey = reinterpret_cast<const uint8_t*>(data);
        // Detects type and decodes the private key
        EVP_PKEY* pkey = d2i_AutoPrivateKey(nullptr, &data_pkey, datalen);
        if (pkey == nullptr)
        {
            LOG(ERROR, LOG_TAG) << "d2i_AutoPrivateKey failed\n";
        }

        // Free is only required after a PEM_bio_read successful return
        if (name != nullptr)
            OPENSSL_free(name);
        if (header != nullptr)
            OPENSSL_free(header);
        if (data != nullptr)
            OPENSSL_free(data);
        return pkey;
    }
    return nullptr;
}


EVP_PKEY* readCert(const std::string& key)
{
    // Reads PEM information and retrieves some details
    std::shared_ptr<BIO> keybio(BIO_new_mem_buf((void*)key.c_str(), -1), [](auto p) { BIO_free(p); });
    if (keybio == nullptr)
    {
        LOG(ERROR, LOG_TAG) << "BIO_new_mem_buf failed\n";
        return nullptr;
    }

    char* name = nullptr;
    char* header = nullptr;
    uint8_t* data = nullptr;
    long datalen = 0;
    if (PEM_read_bio(keybio.get(), &name, &header, &data, &datalen) == 1)
    {
        // Copies the data pointer. D2I functions update it
        const auto* data_pkey = reinterpret_cast<const uint8_t*>(data);
        // Detects type and decodes the private key
        std::shared_ptr<X509> x509(d2i_X509(nullptr, &data_pkey, datalen), [](auto* p) { X509_free(p); });
        EVP_PKEY* pkey = X509_get_pubkey(x509.get());
        if (pkey == nullptr)
        {
            LOG(ERROR, LOG_TAG) << "d2i_AutoPrivateKey failed\n";
        }

        // Free is only required after a PEM_bio_read successful return
        if (name != nullptr)
            OPENSSL_free(name);
        if (header != nullptr)
            OPENSSL_free(header);
        if (data != nullptr)
            OPENSSL_free(data);
        return pkey;
    }
    return nullptr;
}

bool sign(const std::string& pem_key, const std::string& msg, std::vector<unsigned char>& encoded)
{
    std::shared_ptr<EVP_PKEY> key(readKey(pem_key), [](auto p) { EVP_PKEY_free(p); });
    std::shared_ptr<EVP_MD_CTX> ctx(EVP_MD_CTX_create(), [](auto p) { EVP_MD_CTX_free(p); });

    if (EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestSignInit failed\n";
        return false;
    }
    if (EVP_DigestSignUpdate(ctx.get(), msg.c_str(), msg.size()) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestSignUpdate failed\n";
        return false;
    }
    size_t siglen;
    if (EVP_DigestSignFinal(ctx.get(), nullptr, &siglen) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestSignFinal failed\n";
        return false;
    }

    encoded.resize(siglen);
    if (EVP_DigestSignFinal(ctx.get(), encoded.data(), &siglen) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestSignFinal failed\n";
        return false;
    }
    return true;
}


bool verifySignature(const std::string& pem_cert, const unsigned char* MsgHash, size_t MsgHashLen, const char* Msg, size_t MsgLen, bool& Authentic)
{
    Authentic = false;
    std::shared_ptr<EVP_PKEY> key(readCert(pem_cert), [](auto p) { EVP_PKEY_free(p); });
    std::shared_ptr<EVP_MD_CTX> ctx(EVP_MD_CTX_create(), [](auto p) { EVP_MD_CTX_free(p); });

    if (EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestVerifyInit failed\n";
        return false;
    }
    if (EVP_DigestVerifyUpdate(ctx.get(), Msg, MsgLen) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestVerifyInit failed\n";
        return false;
    }
    int authStatus = EVP_DigestVerifyFinal(ctx.get(), MsgHash, MsgHashLen);
    if (authStatus == 1)
    {
        Authentic = true;
        return true;
    }
    if (authStatus == 0)
    {
        Authentic = false;
        return true;
    }
    LOG(ERROR, LOG_TAG) << "EVP_DigestVerifyFinal failed: " << authStatus << "\n";
    return false;
}

} // namespace

Jwt::Jwt() : claims({})
{
}

std::optional<std::chrono::system_clock::time_point> Jwt::getIat() const
{
    if (!claims.contains("iat"))
        return std::nullopt;
    return std::chrono::system_clock::from_time_t(claims.at("iat").get<int64_t>());
}

void Jwt::setIat(const std::optional<std::chrono::system_clock::time_point>& iat)
{
    if (iat.has_value())
        claims["iat"] = std::chrono::system_clock::to_time_t(iat.value());
    else if (claims.contains("iat"))
        claims.erase("iat");
}

std::optional<std::chrono::system_clock::time_point> Jwt::getExp() const
{
    if (!claims.contains("exp"))
        return std::nullopt;
    return std::chrono::system_clock::from_time_t(claims.at("exp").get<int64_t>());
}

void Jwt::setExp(const std::optional<std::chrono::system_clock::time_point>& exp)
{
    if (exp.has_value())
        claims["exp"] = std::chrono::system_clock::to_time_t(exp.value());
    else if (claims.contains("exp"))
        claims.erase("exp");
}

std::optional<std::string> Jwt::getSub() const
{
    if (!claims.contains("sub"))
        return std::nullopt;
    return claims.at("sub").get<std::string>();
}

void Jwt::setSub(const std::optional<std::string>& sub)
{
    if (sub.has_value())
        claims["sub"] = sub.value();
    else if (claims.contains("sub"))
        claims.erase("sub");
}

bool Jwt::parse(const std::string& token, const std::string& pem_cert)
{
    std::vector<std::string> parts = utils::string::split(token, '.');
    if (parts.size() != 3)
    {
        LOG(ERROR, LOG_TAG) << "Token '" << token << "' not in the format <header>.<payload>.<signature>\n";
        return false;
    }

    std::string header = base64url_decode(parts[0]);
    std::string payload = base64url_decode(parts[1]);
    LOG(DEBUG, LOG_TAG) << "Header: " << header << ", payload: " << payload << "\n";

    try
    {
        json jheader = json::parse(header);
        claims = json::parse(payload);
        std::string signature = parts[2];
        LOG(INFO, LOG_TAG) << "Header: " << jheader << "\n";
        LOG(INFO, LOG_TAG) << "Payload: " << claims << "\n";
        LOG(INFO, LOG_TAG) << "Signature: " << signature << "\n";
        auto binary = base64url_decode(signature);
        std::string msg = parts[0] + "." + parts[1];
        bool auth;
        if (!verifySignature(pem_cert, reinterpret_cast<unsigned char*>(binary.data()), binary.size(), msg.c_str(), msg.size(), auth))
        {
            LOG(ERROR, LOG_TAG) << "Failed to verify signature\n";
            return false;
        }
        if (!auth)
        {
            LOG(ERROR, LOG_TAG) << "Wrong signature\n";
            return false;
        }
        return true;
    }
    catch (std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Error parsing JWT header or payload: " << e.what() << "\n";
    }
    return false;
}


std::optional<std::string> Jwt::getToken(const std::string& pem_key) const
{
    json header = {{"typ", "JWT"}};
    if (pem_key.find("-----BEGIN PRIVATE KEY-----") == 0)
        header["alg"] = "RS256";
    // if (pem_key.find("-----BEGIN EC PRIVATE KEY-----") == 0)
    //     header["alg"] = "ES256";
    else
    {
        LOG(ERROR, LOG_TAG) << "PEM key must be an RSA key\n";
        return std::nullopt;
    }

    LOG(DEBUG, LOG_TAG) << "Header: " << header << ", payload: " << claims << "\n";
    std::string msg = base64url_encode(header.dump()) + "." + base64url_encode(claims.dump());
    LOG(DEBUG, LOG_TAG) << "Encoded: " << msg << "\n";

    std::vector<unsigned char> encoded;
    if (sign(pem_key, msg, encoded))
    {
        std::string signature = base64url_encode(encoded.data(), encoded.size());
        LOG(DEBUG, LOG_TAG) << "Signature: " << signature << "\n";
        auto token = msg + "." + signature;
        LOG(DEBUG, LOG_TAG) << "Token: " << token << "\n";
        return token;
    }

    LOG(ERROR, LOG_TAG) << "Failed to sign token\n";
    return std::nullopt;
}
