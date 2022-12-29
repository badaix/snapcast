/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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
#include "file_player.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"

// 3rd party headers

// standard headers
#include <cassert>
#include <iostream>

using namespace std;

namespace player
{

static constexpr auto LOG_TAG = "FilePlayer";
static constexpr auto kDefaultBuffer = 50ms;

static constexpr auto kDescription = "Raw PCM file output";

std::vector<PcmDevice> FilePlayer::pcm_list(const std::string& parameter)
{
    auto params = utils::string::split_pairs(parameter, ',', '=');
    string filename;
    if (params.find("filename") != params.end())
        filename = params["filename"];
    if (filename.empty())
        filename = "stdout";
    return {PcmDevice{0, filename, kDescription}};
}


FilePlayer::FilePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, stream), timer_(io_context), file_(nullptr)
{
    auto params = utils::string::split_pairs(settings.parameter, ',', '=');
    string filename;
    if (params.find("filename") != params.end())
        filename = params["filename"];

    if (filename.empty() || (filename == "stdout"))
    {
        file_.reset(stdout, [](auto p) { std::ignore = p; });
    }
    else if (filename == "stderr")
    {
        file_.reset(stderr, [](auto p) { std::ignore = p; });
    }
    else if (filename != "null")
    {
        std::string mode = "w";
        if (params.find("mode") != params.end())
            mode = params["mode"];
        if ((mode != "w") && (mode != "a"))
            throw SnapException("Mode must be w (write) or a (append)");
        mode += "b";
        file_.reset(fopen(filename.c_str(), mode.c_str()), [](auto p) { fclose(p); });
        if (!file_)
            throw SnapException("Error opening file: '" + filename + "', error: " + cpt::to_string(errno));
    }
}


FilePlayer::~FilePlayer()
{
    LOG(DEBUG, LOG_TAG) << "Destructor\n";
    stop();
}


bool FilePlayer::needsThread() const
{
    return false;
}


void FilePlayer::requestAudio()
{
    auto numFrames = static_cast<uint32_t>(stream_->getFormat().msRate() * kDefaultBuffer.count());
    auto needed = numFrames * stream_->getFormat().frameSize();
    if (buffer_.size() < needed)
        buffer_.resize(needed);

    if (!stream_->getPlayerChunkOrSilence(buffer_.data(), 10ms, numFrames))
    {
        // LOG(INFO, LOG_TAG) << "Failed to get chunk. Playing silence.\n";
    }
    else
    {
        adjustVolume(static_cast<char*>(buffer_.data()), numFrames);
    }

    if (file_)
    {
        fwrite(buffer_.data(), 1, needed, file_.get());
        fflush(file_.get());
    }

    loop();
}


void FilePlayer::loop()
{
    next_request_ += kDefaultBuffer;
    auto now = std::chrono::steady_clock::now();
    if (next_request_ < now)
        next_request_ = now + 1ms;

    timer_.expires_at(next_request_);
    timer_.async_wait(
        [this](boost::system::error_code ec)
        {
        if (ec)
            return;
        requestAudio();
    });
}


void FilePlayer::start()
{
    next_request_ = std::chrono::steady_clock::now();
    loop();
}


void FilePlayer::stop()
{
    LOG(INFO, LOG_TAG) << "Stop\n";
    timer_.cancel();
}

} // namespace player
