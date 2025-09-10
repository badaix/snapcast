/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl
    Copyright (C) 2025  aanno <aannoaanno@gmail.com>

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
#include "player.hpp"

// 3rd party headers
#include <pipewire/main-loop.h>
#include <pipewire/stream.h>

// standard headers
#include <chrono>
#include <cstdio>
#include <memory>
#include <optional>

namespace player
{

static constexpr auto PIPEWIRE = "pipewire";

/// Pipewire Player
class PipeWirePlayer : public Player
{
public:
    /// c'tor
    PipeWirePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    /// d'tor
    ~PipeWirePlayer() override;

    void start() override;
    void stop() override;

    /// List the dummy file PCM device
    static std::vector<PcmDevice> pcm_list(const std::string& parameter);

private:
    // PipeWire callbacks
    static void on_process(void* userdata);
    static void on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param);

    bool getHardwareVolume(Volume& volume) override;
    void setHardwareVolume(const Volume& volume) override;

    void onProcess();
    void onParamChanged(uint32_t id, const struct spa_pod* param);

    void initPipewire();
    void uninitPipewire();

    void worker() override;
    bool needsThread() const override;

    // PipeWire structures
    struct pw_main_loop* pw_main_loop_;
    struct pw_stream* pw_stream_;

    // PipeWire event handlers
    struct pw_stream_events stream_events_;

    std::optional<std::chrono::milliseconds> node_latency_;
};

} // namespace player
