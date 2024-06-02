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
#include <sys/types.h>

// 3rd party headers
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <vector>

// standard headers


static constexpr auto LOG_TAG = "JWT";



namespace
{

EVP_PKEY* createPrivate(const std::string& key)
{
    // Reads PEM information and retrieves some details
    BIO* keybio = BIO_new_mem_buf((void*)key.c_str(), -1);
    if (keybio == nullptr)
    {
        LOG(ERROR, LOG_TAG) << "BIO_new_mem_buf failed\n";
        return nullptr;
    }

    char* name = nullptr;
    char* header = nullptr;
    uint8_t* data = nullptr;
    long datalen = 0;
    if (PEM_read_bio(keybio, &name, &header, &data, &datalen) == 1)
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


bool Sign(const std::string& pem_key, const std::string& msg, std::vector<unsigned char>& encoded)
{
    auto* key = createPrivate(pem_key);
    EVP_MD_CTX* ctx = EVP_MD_CTX_create();
    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha384(), nullptr, key) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestSignInit failed\n";
        return false;
    }
    if (EVP_DigestSignUpdate(ctx, msg.c_str(), msg.size()) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestSignUpdate failed\n";
        return false;
    }
    size_t siglen;
    if (EVP_DigestSignFinal(ctx, nullptr, &siglen) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestSignFinal failed\n";
        return false;
    }

    encoded.resize(siglen);
    if (EVP_DigestSignFinal(ctx, encoded.data(), &siglen) <= 0)
    {
        LOG(ERROR, LOG_TAG) << "EVP_DigestSignFinal failed\n";
        return false;
    }
    EVP_MD_CTX_free(ctx);
    return true;
}

} // namespace

Jwt::Jwt()
{
    // {
    //   "alg": "RS256",
    //   "typ": "JWT"
    // }
    json header = {{"alg", "RS384"}, {"typ", "JWT"}};

    // {
    //   "sub": "1234567890",
    //   "name": "Johannes",
    //   "admin": true,
    //   "iat": 1516239022
    // }
    json payload = {{"sub", "1234567890"}, {"name", "Johannes"}, {"admin", true}, {"iat", 1516239022}};

    LOG(INFO, LOG_TAG) << "Header: " << header << "\n";
    LOG(INFO, LOG_TAG) << "Payload: " << payload << "\n";
    std::string msg = base64url_encode(header.dump()) + "." + base64url_encode(payload.dump());
    LOG(INFO, LOG_TAG) << "Encoded: " << msg << "\n";

    auto key = "-----BEGIN PRIVATE KEY-----\n"
               "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCwFxHHKvV6THj1\n"
               "VjvZQJIW+OjIKF9MPfIN8wJTXn+4EkBJCoBy0NfKHG+FCb90YCPzvLrjL8jKcfhT\n"
               "/jpaStrYYjABXA1sJS9P+9cwiV9RwTxTrfOiss8F7eZdyZI9UogrPmQa7YbevmwB\n"
               "XeXIqKzdWQbtQsOJQgoL3vIR7qjUpVQrXPwAYQ6pc7AxS5RHFy8V2Y3sh/+i0aS9\n"
               "bH/276OAKZNwVIfUIcSVVbBAPvrheR2ezUVoKSumUzek/2uq3cLb5YNF4XSe1Ikv\n"
               "16lRSGTIQ2KflSYn3mJldFjEKL3sgADTmMhKes+TVveNr7eCvynyDCtHdiLanOKY\n"
               "PyyOXFTdAgMBAAECggEAAjunU6UBZsBhrUzKEVZ5XnYKxP+xZlmyat0Iy8QFmcY4\n"
               "z076iNo0o6u/efVWb/RIfcQILlmHXJKG7BEWg4Qc/oRPkwjW47xHULvtw0AOt85G\n"
               "mfy5UPgfBMvQRs1c/87piqYt0KNFTqhQCCa9GKcTmsf7p5ZtPTLw8Sxt2e6H8LsK\n"
               "60Jzc2Yw2t3unEb1NnjsTgshjPwFcdrppyRAa2B0Wk3f4ADA1i4vDmTt2+jTq/Hp\n"
               "yFWup58Djs+lyn4RLnp7jFD2KS/q+qVQsTfGcPeXLMIWHHQDwfjfkzdA74zNi+Pn\n"
               "C4e/iXIsC7VH4BJ8qrVH20WqTXRyuZ1uEF+32XbcSQKBgQDAtBXbnuGhu2llnfP2\n"
               "dxJ5qtRjxTHedZs9UMEuy5pkLMi4JsIdjf072lqizpG7DE9kfiERwXJ/Spe4OMMh\n"
               "MvWBpnJieTHnouAMpDVootVbSCpikOSGzClHZwpl6KU7pv9Q+Hv2xkSnTJLsakSt\n"
               "qlOsG6cwK56kXiwYG0RsAn6lhQKBgQDp7gNE4J6wf9SZHErpyh+65VqqcF5+chNq\n"
               "DTwFlb7U31WcDOA3hUHaQfrlwfblMAgnlVMofopdP4KIIgSWE31cCziBp8ZRy/25\n"
               "2/aNkDoPEN59Ibk1RWLYsCzcQIAQrTjvfDMn3An1E9B3qYFzfdLKZItb8p3cxpO/\n"
               "sMUwQRqFeQKBgDb7av0lwQUPXwwiXDhnUvsp9b2dxxPNBIUjJGuApkWMzZxVWq9q\n"
               "EuXf8FphjA0Nfx2SK0dQpaWSF+X1NB+l1YyvfBWCtO19eGXC+IYpZ6zK02UaKEoZ\n"
               "uHFqAfp/vZ1ekZx9uYj4myAM5iLUU1Iltgf2P+arm3EUeYpLRWN39sCtAoGBALZ0\n"
               "egBC4gLv8TXqp1Np3w26zdiaBFnDR/kzkVkZztnhx7gLIuaq/Q3q4HJLsvJXYETf\n"
               "ZxjyeaD5ZCohvkn/sYsVBWG7JieuX5uTQN5xW5dcpOwcXYR7Nfmkj5jKhhh7wyin\n"
               "So8QRIPujG6IuvsFbF+HxFpXBWGpUJv2mBZm8PShAoGBALU/KNl1El0HpxTr3wgj\n"
               "YY6eU3snn0Oa5Ci3k/FMWd1QTtVsf4Mym0demxshdC75L+qzCS0m5jAo9tm0keY9\n"
               "A4F3VRlsuUyuAja+qDU2xMo3jnFKOIyMfN4mVSiFkqnq3eQ4xHgViyIEyr+8AbA4\n"
               "ajjiCZsv+OITxQ+TTHeGDsdD\n"
               "-----END PRIVATE KEY-----\n";

    std::vector<unsigned char> encoded;
    if (Sign(key, msg, encoded))
    {
        std::string signature = base64url_encode(encoded.data(), encoded.size());
        LOG(INFO, LOG_TAG) << "Signature: " << signature << "\n";
    }
    else
    {
        LOG(ERROR, LOG_TAG) << "Failed to sign\n";
    }
}


bool Jwt::decode(const std::string& token)
{
    std::vector<std::string> parts = utils::string::split(token, '.');
    if (parts.size() != 3)
    {
        return false;
    }

    LOG(INFO, LOG_TAG) << "Header: " << base64url_decode(parts[0]) << "\n";
    LOG(INFO, LOG_TAG) << "Payload: " << base64url_decode(parts[1]) << "\n";

    header_ = json::parse(base64url_decode(parts[0]));
    payload_ = json::parse(base64url_decode(parts[1]));
    std::string signature = parts[2];

    LOG(INFO, LOG_TAG) << "Header: " << header_ << "\n";
    LOG(INFO, LOG_TAG) << "Payload: " << payload_ << "\n";
    LOG(INFO, LOG_TAG) << "Signature: " << signature << "\n";

    return true;
}
