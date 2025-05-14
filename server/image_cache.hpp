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


// 3rd party headers
#include <boost/algorithm/hex.hpp>
#include <boost/uuid/detail/md5.hpp>

// standard headers
#include <map>
#include <mutex>
#include <optional>
#include <string>


/// Image cache, used to store current album art per stream
class ImageCache
{
public:
    /// @return singleton to the image cache
    static ImageCache& instance()
    {
        static ImageCache instance_;
        return instance_;
    }

    /// Store the base64 encoded @p image for @p key (the session that stores the image) in the cache
    /// @return url of the cached image (md5 of key + image data) appended with @p extension
    std::string setImage(const std::string& key, std::string image, const std::string& extension)
    {
        if (image.empty())
        {
            clear(key);
            return "";
        }

        using boost::uuids::detail::md5;
        md5 hash;
        md5::digest_type digest;
        hash.process_bytes(key.data(), key.size());
        hash.process_bytes(image.data(), image.size());
        hash.get_digest(digest);
        std::string filename;
        const auto* intDigest = reinterpret_cast<const int*>(&digest);
        boost::algorithm::hex_lower(intDigest, intDigest + (sizeof(md5::digest_type) / sizeof(int)), std::back_inserter(filename));
        auto ext = extension;
        if (ext.find('.') == 0)
            ext = ext.substr(1);
        filename += "." + ext;
        std::lock_guard<std::mutex> lock(mutex_);
        key_to_url_[key] = filename;
        url_to_data_[filename] = std::move(image);
        return filename;
    };

    /// Clear image for @p key (the stream session's name)
    void clear(const std::string& key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = key_to_url_.find(key);
        if (iter != key_to_url_.end())
        {
            auto url = *iter;
            auto url_iter = url_to_data_.find(url.second);
            if (url_iter != url_to_data_.end())
                url_to_data_.erase(url_iter);
            key_to_url_.erase(iter);
        }
    }

    /// @return base64 encoded image for url (the one returned by "setImage")
    std::optional<std::string> getImage(const std::string& url)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = url_to_data_.find(url);
        if (iter == url_to_data_.end())
            return std::nullopt;
        else
            return iter->second;
    }

private:
    ImageCache() = default;
    ~ImageCache() = default;

    std::map<std::string, std::string> key_to_url_;
    std::map<std::string, std::string> url_to_data_;
    std::mutex mutex_;
};
