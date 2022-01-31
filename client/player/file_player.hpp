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

#ifndef FILE_PLAYER_HPP
#define FILE_PLAYER_HPP

// local headers
#include "player.hpp"

// 3rd party headers
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <cstdio>
#include <memory>

namespace player
{

static constexpr auto FILE = "file";

/// File Player
/// Used for testing and doesn't even write the received audio to file at the moment,
/// but just discards it
class FilePlayer : public Player
{
public:
    FilePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    virtual ~FilePlayer();

    void start() override;
    void stop() override;

    /// List the dummy file PCM device
    static std::vector<PcmDevice> pcm_list(const std::string& parameter);

protected:
    void requestAudio();
    void loop();
    bool needsThread() const override;
    boost::asio::steady_timer timer_;
    std::vector<char> buffer_;
    std::chrono::time_point<std::chrono::steady_clock> next_request_;
    std::shared_ptr<::FILE> file_;
};

} // namespace player

#endif
