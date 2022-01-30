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

#ifndef PULSE_PLAYER_HPP
#define PULSE_PLAYER_HPP

// local headers
#include "player.hpp"

// 3rd party headers
#include <pulse/pulseaudio.h>

// standard headers
#include <atomic>
#include <cstdio>
#include <memory>
#include <optional>


namespace player
{

static constexpr auto PULSE = "pulse";

/// File Player
/// Used for testing and doesn't even write the received audio to file at the moment,
/// but just discards it
class PulsePlayer : public Player
{
public:
    PulsePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    virtual ~PulsePlayer();

    void start() override;
    void stop() override;

    /// List the system's audio output devices
    static std::vector<PcmDevice> pcm_list(const std::string& parameter);

protected:
    bool needsThread() const override;
    void worker() override;

    void connect();
    void disconnect();

    bool getHardwareVolume(double& volume, bool& muted) override;
    void setHardwareVolume(double volume, bool muted) override;

    void triggerVolumeUpdate();

    void underflowCallback(pa_stream* stream);
    void stateCallback(pa_context* ctx);
    void writeCallback(pa_stream* stream, size_t nbytes);
    void subscribeCallback(pa_context* ctx, pa_subscription_event_type_t event_type, uint32_t idx);

    std::vector<char> buffer_;

    std::chrono::microseconds latency_;
    int underflows_ = 0;
    std::atomic<int> pa_ready_;

    long last_chunk_tick_;

    pa_buffer_attr bufattr_;
    pa_sample_spec pa_ss_;
    pa_mainloop* pa_ml_;
    pa_context* pa_ctx_;
    pa_stream* playstream_;
    pa_proplist* proplist_;
    std::optional<std::string> server_;
    std::map<std::string, std::string> properties_;

    // cache of the last volume change
    std::chrono::time_point<std::chrono::steady_clock> last_change_;
};

} // namespace player

#endif
