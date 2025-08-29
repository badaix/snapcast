/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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
#include "pipewire_player.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"

// 3rd party headers

// standard headers
#include <algorithm>
#include <cmath>
#include <pipewire/main-loop.h>
#include <tuple>


#define M_PI_M2 (M_PI + M_PI)

#define DEFAULT_RATE 44100
#define DEFAULT_CHANNELS 2
#define DEFAULT_VOLUME 0.7


using namespace std;

namespace player
{

static constexpr auto LOG_TAG = "PipewirePlayer";
// static constexpr auto kDefaultBuffer = 50ms;

// static constexpr auto kDescription = "Pipewire player";

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-statement-expression"
#pragma GCC diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#endif

namespace
{
spa_audio_format sampleFormatToPipeWire(const SampleFormat& format)
{
    if (format.bits() == 8)
        return SPA_AUDIO_FORMAT_S8;
    else if (format.bits() == 16)
        return SPA_AUDIO_FORMAT_S16_LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 4))
        return SPA_AUDIO_FORMAT_S24_LE;
    else if (format.bits() == 32)
        return SPA_AUDIO_FORMAT_S32_LE;
    else
        throw SnapException("Unsupported sample format: " + cpt::to_string(format.bits()));
}
} // namespace


std::vector<PcmDevice> PipewirePlayer::pcm_list(const std::string& parameter)
{
    std::ignore = parameter;
    return {};
}


PipewirePlayer::PipewirePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, std::move(stream)), timer_(io_context)
{
    LOG(DEBUG, LOG_TAG) << "Pipewire player\n";
}


PipewirePlayer::~PipewirePlayer()
{
    LOG(DEBUG, LOG_TAG) << "Destructor\n";
    stop(); // NOLINT
}


void PipewirePlayer::worker()
{
    pw_main_loop_run(pw_main_loop_);
    pw_stream_destroy(pw_stream_);
    pw_main_loop_destroy(pw_main_loop_);
}


bool PipewirePlayer::needsThread() const
{
    return true;
}


void PipewirePlayer::stop()
{
    LOG(INFO, LOG_TAG) << "Stop\n";
    timer_.cancel();
}


void PipewirePlayer::onProcess()
{
    if (!active_)
    {
        pw_main_loop_quit(pw_main_loop_);
        return;
    }

    struct pw_buffer* b;
    int i, c, n_frames, stride;
    int16_t *dst, val;

    if ((b = pw_stream_dequeue_buffer(pw_stream_)) == nullptr)
    {
        pw_log_warn("out of buffers: %s", strerror(errno));
        return;
    }

    spa_buffer* buf = b->buffer;
    if ((dst = reinterpret_cast<int16_t*>(buf->datas[0].data)) == nullptr)
        return;

    stride = sizeof(int16_t) * DEFAULT_CHANNELS;
    n_frames = buf->datas[0].maxsize / stride;
    if (b->requested)
        n_frames = std::min<int>(static_cast<int>(b->requested), n_frames);
    LOG(DEBUG, LOG_TAG) << "on_process: " << accumulator << ", frames: " << n_frames << ", requested: " << b->requested << "\n";

    for (i = 0; i < n_frames; i++)
    {
        accumulator += M_PI_M2 * 440 / DEFAULT_RATE;
        if (accumulator >= M_PI_M2)
            accumulator -= M_PI_M2;

        /* sin() gives a value between -1.0 and 1.0, we first apply
         * the volume and then scale with 32767.0 to get a 16 bits value
         * between [-32767 32767].
         * Another common method to convert a double to
         * 16 bits is to multiple by 32768.0 and then clamp to
         * [-32768 32767] to get the full 16 bits range. */
        val = sin(accumulator) * DEFAULT_VOLUME * 32767.0;
        for (c = 0; c < DEFAULT_CHANNELS; c++)
            dst[i * DEFAULT_CHANNELS + c] = val;
        // *dst++ = val;
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = n_frames * stride;

    LOG(DEBUG, LOG_TAG) << "queue: " << dst[0] << "\n";
    pw_stream_queue_buffer(pw_stream_, b);
}


void PipewirePlayer::on_process(void* userdata)
{
    auto* player = static_cast<PipewirePlayer*>(userdata);
    player->onProcess();
}


void PipewirePlayer::start()
{
    LOG(DEBUG, LOG_TAG) << "Start\n";

    // Set up stream events
    spa_zero(stream_events_);
    stream_events_.version = PW_VERSION_STREAM_EVENTS;
    stream_events_.process = on_process;
    // stream_events_.state_changed = on_state_changed;
    // stream_events_.param_changed = on_param_changed;

    // next_request_ = std::chrono::steady_clock::now();
    // loop();

    std::array<uint8_t, 1024> buffer;
    struct spa_pod_builder b;

#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    spa_pod_builder_init(&b, buffer.data(), buffer.size());
#pragma GCC diagnostic pop

    pw_init(nullptr, nullptr);

    // Create main loop
    pw_main_loop_ = pw_main_loop_new(nullptr);
    if (!pw_main_loop_)
        throw SnapException("Failed to create PipeWire main loop");

    // // Set up stream properties
    // auto* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Music", PW_KEY_APP_NAME, "Snapcast",
    //                            PW_KEY_MEDIA_CLASS, "Audio/Source", PW_KEY_NODE_NAME, "TODO", nullptr);
    // // Create stream, "props" ownership transferred to stream
    // pw_stream_ = pw_stream_new(pw_core_, stream_name_.c_str(), props);

    pw_stream_ = pw_stream_new_simple(pw_main_loop_get_loop(pw_main_loop_), "audio-src",
                                      pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Music", nullptr),
                                      // PW_KEY_APP_NAME, "Snapcast", PW_KEY_MEDIA_CLASS, "Audio/Source",,
                                      &stream_events_, this);

    // Set up audio format
    struct spa_audio_info_raw spa_audio_info = {};
    spa_audio_info.flags = SPA_AUDIO_FLAG_NONE;
    const auto& sampleformat = stream_->getFormat();
    spa_audio_info.format = sampleFormatToPipeWire(sampleformat);
    spa_audio_info.rate = sampleformat.rate();
    spa_audio_info.channels = sampleformat.channels();

    // Set channel positions (stereo by default)
    if (sampleformat.channels() == 2)
    {
        spa_audio_info.position[0] = SPA_AUDIO_CHANNEL_FL;
        spa_audio_info.position[1] = SPA_AUDIO_CHANNEL_FR;
    }
    else if (sampleformat.channels() == 1)
    {
        spa_audio_info.position[0] = SPA_AUDIO_CHANNEL_MONO;
    }

    // Build format parameters
    std::array<const struct spa_pod*, 1> params;
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &spa_audio_info);

    // Connect stream
    // NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
    auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);
    // NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)
    int res = pw_stream_connect(pw_stream_, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params.data(), params.size());
    std::ignore = res;
    LOG(DEBUG, LOG_TAG) << "pw_stream_connect: " << res << "\n";

    // Run PipeWire main loop in a separate thread
    Player::start();
}


#ifdef __clang__
#pragma GCC diagnostic pop
#endif


} // namespace player
