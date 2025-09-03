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
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>

// standard headers
#include <atomic>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

namespace player
{

static constexpr auto PIPEWIRE = "pipewire";

/// PipeWire Player
/// Audio player implementation using PipeWire's native API
class PipeWirePlayer : public Player
{
public:
    /// c'tor
    PipeWirePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    /// d'tor
    virtual ~PipeWirePlayer();

    void start() override;
    void stop() override;

    /// List the system's audio output devices
    static std::vector<PcmDevice> pcm_list(const std::string& parameter = "");

private:
    bool needsThread() const override;
    void worker() override;

    void connect();
    void disconnect();

    bool getHardwareVolume(Volume& volume) override;
    void setHardwareVolume(const Volume& volume) override;

    // PipeWire callbacks
    static void on_process(void* userdata);
    static void on_state_changed(void* userdata, enum pw_stream_state old, enum pw_stream_state state, const char* error);
    static void on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param);
    static void on_io_changed(void* userdata, uint32_t id, void* area, uint32_t size);
    static void on_drained(void* userdata);

    // PipeWire callback helper
    void onProcess();

    // Registry callbacks for device enumeration
    static void registry_event_global(void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props);
    static void registry_event_global_remove(void* data, uint32_t id);

    std::vector<char> buffer_;
    std::chrono::microseconds latency_;
    std::atomic<bool> stream_ready_;
    std::atomic<bool> connected_;
    std::atomic<long> last_chunk_tick_;
    std::atomic<bool> disconnect_requested_;

    struct pw_main_loop* main_loop_;
    struct pw_context* context_;
    struct pw_core* core_;
    struct pw_stream* pw_stream_;

    struct spa_hook stream_listener_;
    struct pw_stream_events stream_events_;

    std::map<std::string, std::string> properties_;

    // Stream parameters
    struct spa_audio_info_raw audio_info_;
    uint32_t frame_size_;

    // PipeWire stream events - C++11 compatible initialization
    static struct pw_stream_events get_stream_events();
    static struct pw_registry_events get_registry_events();
};

} // namespace player
