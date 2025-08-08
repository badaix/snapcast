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
#include "common/utils/string_utils.hpp"

// 3rd party headers

// standard headers



#include <math.h>
 
#include <spa/param/audio/format-utils.h>
 
#include <pipewire/pipewire.h>
 
#define M_PI_M2 ( M_PI + M_PI )
 
#define DEFAULT_RATE            44100
#define DEFAULT_CHANNELS        2
#define DEFAULT_VOLUME          0.7


using namespace std;

namespace player
{

static constexpr auto LOG_TAG = "PipewirePlayer";
static constexpr auto kDefaultBuffer = 50ms;

static constexpr auto kDescription = "Pipewire player";

std::vector<PcmDevice> PipewirePlayer::pcm_list(const std::string& parameter)
{
    auto params = utils::string::split_pairs(parameter, ',', '=');
    string filename;
    if (params.find("filename") != params.end())
        filename = params["filename"];
    if (filename.empty())
        filename = "stdout";
    return {PcmDevice{0, filename, kDescription}};
}


PipewirePlayer::PipewirePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, std::move(stream)), timer_(io_context), file_(nullptr)
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


PipewirePlayer::~PipewirePlayer()
{
    LOG(DEBUG, LOG_TAG) << "Destructor\n";
    stop(); // NOLINT
}


bool PipewirePlayer::needsThread() const
{
    return false;
}


void PipewirePlayer::requestAudio()
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


void PipewirePlayer::loop()
{
    next_request_ += kDefaultBuffer;
    auto now = std::chrono::steady_clock::now();
    if (next_request_ < now)
        next_request_ = now + 1ms;

    timer_.expires_at(next_request_);
    timer_.async_wait([this](boost::system::error_code ec)
    {
        if (ec)
            return;
        requestAudio();
    });
}


void PipewirePlayer::start()
{
    next_request_ = std::chrono::steady_clock::now();
    loop();
}


void PipewirePlayer::stop()
{
    LOG(INFO, LOG_TAG) << "Stop\n";
    timer_.cancel();
}



 
struct data {
        struct pw_main_loop *loop;
        struct pw_stream *stream;
        double accumulator;
};
 
/* [on_process] */
static void on_process(void *userdata)
{
        auto* data = reinterpret_cast<struct data*>(userdata);
        struct pw_buffer *b;
        struct spa_buffer *buf;
        int i, c, n_frames, stride;
        int16_t *dst, val;
 
        if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
                pw_log_warn("out of buffers: %m");
                return;
        }
 
        buf = b->buffer;
        if ((dst = reinterpret_cast<int16_t*>(buf->datas[0].data)) == nullptr)
                return;
 
        stride = sizeof(int16_t) * DEFAULT_CHANNELS;
        n_frames = buf->datas[0].maxsize / stride;
        if (b->requested)
                n_frames = SPA_MIN(b->requested, n_frames);
 
        for (i = 0; i < n_frames; i++) {
                data->accumulator += M_PI_M2 * 440 / DEFAULT_RATE;
                if (data->accumulator >= M_PI_M2)
                        data->accumulator -= M_PI_M2;
 
                /* sin() gives a value between -1.0 and 1.0, we first apply
                 * the volume and then scale with 32767.0 to get a 16 bits value
                 * between [-32767 32767].
                 * Another common method to convert a double to
                 * 16 bits is to multiple by 32768.0 and then clamp to
                 * [-32768 32767] to get the full 16 bits range. */
                val = sin(data->accumulator) * DEFAULT_VOLUME * 32767.0;
                for (c = 0; c < DEFAULT_CHANNELS; c++)
                        *dst++ = val;
        }
 
        buf->datas[0].chunk->offset = 0;
        buf->datas[0].chunk->stride = stride;
        buf->datas[0].chunk->size = n_frames * stride;
 
        pw_stream_queue_buffer(data->stream, b);
}
/* [on_process] */
 
static const struct pw_stream_events stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .process = on_process,
};
 
int main(int argc, char *argv[])
{
        struct data data = { 0, };
        const struct spa_pod *params[1];
        uint8_t buffer[1024];
        struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
 
        pw_init(&argc, &argv);
 
        data.loop = pw_main_loop_new(NULL);
 
        data.stream = pw_stream_new_simple(
                        pw_main_loop_get_loop(data.loop),
                        "audio-src",
                        pw_properties_new(
                                PW_KEY_MEDIA_TYPE, "Audio",
                                PW_KEY_MEDIA_CATEGORY, "Playback",
                                PW_KEY_MEDIA_ROLE, "Music",
                                NULL),
                        &stream_events,
                        &data);
 
        params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
                        &SPA_AUDIO_INFO_RAW_INIT(
                                .format = SPA_AUDIO_FORMAT_S16,
                                .channels = DEFAULT_CHANNELS,
                                .rate = DEFAULT_RATE ));
 
        pw_stream_connect(data.stream,
                          PW_DIRECTION_OUTPUT,
                          PW_ID_ANY,
                          PW_STREAM_FLAG_AUTOCONNECT |
                          PW_STREAM_FLAG_MAP_BUFFERS |
                          PW_STREAM_FLAG_RT_PROCESS,
                          params, 1);
 
        pw_main_loop_run(data.loop);
 
        pw_stream_destroy(data.stream);
        pw_main_loop_destroy(data.loop);
 
        return 0;
}

 
struct data {
        struct pw_main_loop *loop;
        struct pw_stream *stream;
        double accumulator;
};
 
/* [on_process] */
static void on_process(void *userdata)
{
        struct data *data = userdata;
        struct pw_buffer *b;
        struct spa_buffer *buf;
        int i, c, n_frames, stride;
        int16_t *dst, val;
 
        if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
                pw_log_warn("out of buffers: %m");
                return;
        }
 
        buf = b->buffer;
        if ((dst = buf->datas[0].data) == NULL)
                return;
 
        stride = sizeof(int16_t) * DEFAULT_CHANNELS;
        n_frames = buf->datas[0].maxsize / stride;
        if (b->requested)
                n_frames = SPA_MIN(b->requested, n_frames);
 
        for (i = 0; i < n_frames; i++) {
                data->accumulator += M_PI_M2 * 440 / DEFAULT_RATE;
                if (data->accumulator >= M_PI_M2)
                        data->accumulator -= M_PI_M2;
 
                /* sin() gives a value between -1.0 and 1.0, we first apply
                 * the volume and then scale with 32767.0 to get a 16 bits value
                 * between [-32767 32767].
                 * Another common method to convert a double to
                 * 16 bits is to multiple by 32768.0 and then clamp to
                 * [-32768 32767] to get the full 16 bits range. */
                val = sin(data->accumulator) * DEFAULT_VOLUME * 32767.0;
                for (c = 0; c < DEFAULT_CHANNELS; c++)
                        *dst++ = val;
        }
 
        buf->datas[0].chunk->offset = 0;
        buf->datas[0].chunk->stride = stride;
        buf->datas[0].chunk->size = n_frames * stride;
 
        pw_stream_queue_buffer(data->stream, b);
}
/* [on_process] */
 
static const struct pw_stream_events stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .process = on_process,
};
 
int main(int argc, char *argv[])
{
        struct data data = { 0, };
        const struct spa_pod *params[1];
        uint8_t buffer[1024];
        struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
 
        pw_init(&argc, &argv);
 
        data.loop = pw_main_loop_new(NULL);
 
        data.stream = pw_stream_new_simple(
                        pw_main_loop_get_loop(data.loop),
                        "audio-src",
                        pw_properties_new(
                                PW_KEY_MEDIA_TYPE, "Audio",
                                PW_KEY_MEDIA_CATEGORY, "Playback",
                                PW_KEY_MEDIA_ROLE, "Music",
                                NULL),
                        &stream_events,
                        &data);
 
        params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
                        &SPA_AUDIO_INFO_RAW_INIT(
                                .format = SPA_AUDIO_FORMAT_S16,
                                .channels = DEFAULT_CHANNELS,
                                .rate = DEFAULT_RATE ));
 
        pw_stream_connect(data.stream,
                          PW_DIRECTION_OUTPUT,
                          PW_ID_ANY,
                          PW_STREAM_FLAG_AUTOCONNECT |
                          PW_STREAM_FLAG_MAP_BUFFERS |
                          PW_STREAM_FLAG_RT_PROCESS,
                          params, 1);
 
        pw_main_loop_run(data.loop);
 
        pw_stream_destroy(data.stream);
        pw_main_loop_destroy(data.loop);
 
        return 0;
}


} // namespace player
