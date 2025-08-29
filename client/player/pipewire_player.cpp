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
#include <pipewire/main-loop.h>
#include <tuple>


using namespace std;

namespace player
{

static constexpr auto LOG_TAG = "PipewirePlayer";

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-statement-expression"
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
    : Player(io_context, settings, std::move(stream)), pw_main_loop_(nullptr), pw_stream_(nullptr)
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
}


void PipewirePlayer::onProcess()
{
    if (!active_)
    {
        pw_main_loop_quit(pw_main_loop_);
        return;
    }

    struct pw_buffer* b;
    if ((b = pw_stream_dequeue_buffer(pw_stream_)) == nullptr)
    {
        pw_log_warn("out of buffers: %s", strerror(errno));
        return;
    }

    spa_buffer* buf = b->buffer;
    int16_t* dst;
    if ((dst = reinterpret_cast<int16_t*>(buf->datas[0].data)) == nullptr)
        return;

    const auto& sampleformat = stream_->getFormat();
    int stride = sizeof(int16_t) * sampleformat.channels();
    int n_frames = buf->datas[0].maxsize / stride;
#if PW_CHECK_VERSION(0, 3, 49)
    if (b->requested)
        n_frames = std::min<int>(static_cast<int>(b->requested), n_frames);
        // LOG(TRACE, LOG_TAG) << "on_process - frames: " << n_frames << ", requested: " << b->requested << "\n";
#else
    // LOG(TRACE, LOG_TAG) << "on_process - frames: " << n_frames << "\n";
#endif

    // if (delay.count() == 0)
    // {
    //     // Calc latency according to:
    //     // https://docs.pipewire.org/structpw__time.html
    //     pw_time time;
    //     pw_stream_get_time_n(pw_stream_, &time, sizeof(struct pw_time));
    //     uint64_t now = pw_stream_get_nsec(pw_stream_);
    //     int64_t diff = now - time.now;
    //     double elapsed = static_cast<double>(time.rate.denom * diff) / static_cast<double>(time.rate.num * SPA_NSEC_PER_SEC);

    //     double rate = sampleformat.rate();
    //     double latency_ms = (time.buffered * 1000. / rate) + (time.queued * 1000. / rate) +
    //                         ((time.delay - elapsed) * 1000. * static_cast<double>(time.rate.num) / static_cast<double>(time.rate.denom));
    //     LOG(DEBUG, LOG_TAG) << "time.buffered: " << time.buffered << ", time.queued: " << time.queued << ", time.delay: " << time.delay
    //                         << ", elapsed: " << elapsed << ", time.rate.num: " << time.rate.num << ", time.rate.denom: " << time.rate.denom << "\n";
    //     LOG(DEBUG, LOG_TAG) << "latency: " << latency_ms << "\n";
    //     delay = chronos::usec(static_cast<int>(latency_ms * 1000));
    // }

    pw_time time;
    pw_stream_get_time_n(pw_stream_, &time, sizeof(struct pw_time));
    auto delay = chronos::usec(static_cast<int>(time.delay * 1000. * 1000. / sampleformat.rate()));
    if (!stream_->getPlayerChunkOrSilence(dst, delay, n_frames))
    {
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = n_frames * stride;

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

    // Set up stream properties
    pw_stream_ = pw_stream_new_simple(pw_main_loop_get_loop(pw_main_loop_), "Snapcast",
                                      pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Music",
                                                        PW_KEY_APP_NAME, "Snapclient", /*PW_KEY_MEDIA_CLASS, "Audio/Sink",*/ PW_KEY_NODE_NAME, "TODO", nullptr),
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
