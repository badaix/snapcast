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

// prototype/interface header file
#include "pipewire_player.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"

// 3rd party headers
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>

// standard headers
#include <algorithm>
#include <cstdint>
#include <tuple>


using namespace std;

namespace player
{

static constexpr auto LOG_TAG = "PipeWirePlayer";

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


std::vector<PcmDevice> PipeWirePlayer::pcm_list(const std::string& parameter)
{
    std::ignore = parameter;

    pw_init(nullptr, nullptr);

    // Create a threaded loop for device enumeration
    auto* loop = pw_thread_loop_new("snapcast-enum", nullptr);
    if (!loop)
        throw SnapException("Failed to create PipeWire thread loop");

    auto* context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
    if (!context)
    {
        pw_thread_loop_destroy(loop);
        throw SnapException("Failed to create PipeWire context");
    }

    pw_thread_loop_lock(loop);

    if (pw_thread_loop_start(loop) < 0)
    {
        pw_thread_loop_unlock(loop);
        pw_context_destroy(context);
        pw_thread_loop_destroy(loop);
        throw SnapException("Failed to start thread loop");
    }

    auto* core = pw_context_connect(context, nullptr, 0);
    if (!core)
    {
        pw_thread_loop_unlock(loop);
        pw_thread_loop_stop(loop);
        pw_context_destroy(context);
        pw_thread_loop_destroy(loop);
        throw SnapException("Failed to connect to PipeWire core");
    }

    auto* registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    if (!registry)
    {
        pw_core_disconnect(core);
        pw_thread_loop_unlock(loop);
        pw_thread_loop_stop(loop);
        pw_context_destroy(context);
        pw_thread_loop_destroy(loop);
        throw SnapException("Failed to get PipeWire registry");
    }

    static std::vector<PcmDevice> g_devices;
    g_devices.clear();

    struct pw_registry_events events;
    spa_zero(events);
    events.version = PW_VERSION_REGISTRY_EVENTS;
    events.global = [](void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props)
    {
        std::ignore = data;
        std::ignore = permissions;
        std::ignore = version;

        // Only process Node interfaces
        if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
            return;

        const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        if (!media_class)
            return;

        // Only process Audio/Sink nodes
        if (strcmp(media_class, "Audio/Sink") != 0)
            return;

        const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
        const char* description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);

        if (name && description)
        {
            // std::lock_guard<std::mutex> lock(g_devices_mutex);
            g_devices.emplace_back(id, name, description);
            LOG(DEBUG, LOG_TAG) << "Found audio sink: " << name << " (" << description << ")\n";
        }
    };

    // Add registry listener
    struct spa_hook registry_hook;
    pw_registry_add_listener(registry, &registry_hook, &events, nullptr);

    // Let it run for a short time to enumerate devices
    pw_thread_loop_unlock(loop);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    pw_thread_loop_lock(loop);

    // Cleanup
    spa_hook_remove(&registry_hook);
    pw_proxy_destroy((struct pw_proxy*)registry);
    pw_core_disconnect(core);

    pw_thread_loop_unlock(loop);
    pw_thread_loop_stop(loop);
    pw_context_destroy(context);
    pw_thread_loop_destroy(loop);

    // Copy devices with mutex held
    std::vector<PcmDevice> devices;
    {
        // std::lock_guard<std::mutex> lock(g_devices_mutex);
        devices = g_devices;
    }

    // Add default device
    devices.emplace(devices.begin(), -1, DEFAULT_DEVICE, "Let PipeWire choose the device");

    LOG(INFO, LOG_TAG) << "Found " << devices.size() << " audio devices\n";

    return devices;
}


PipeWirePlayer::PipeWirePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, std::move(stream)), pw_main_loop_(nullptr), pw_stream_(nullptr), node_latency_(std::nullopt)
{
    LOG(DEBUG, LOG_TAG) << "Pipewire player\n";

    auto params = utils::string::split_pairs_to_container<std::vector<std::string>>(settings.parameter, ',', '=');

    if (params.find("buffer_time") != params.end())
        node_latency_ = std::chrono::milliseconds(std::max(cpt::stoi(params["buffer_time"].front()), 10));
}


PipeWirePlayer::~PipeWirePlayer()
{
    LOG(DEBUG, LOG_TAG) << "Destructor\n";
    stop(); // NOLINT
}


void PipeWirePlayer::worker()
{
    while (active_)
    {
        LOG(DEBUG, LOG_TAG) << "Starting main loop\n";
        int res = pw_main_loop_run(pw_main_loop_);
        const SEVERITY severity = active_ ? SEVERITY::ERROR : SEVERITY::DEBUG;
        LOG(severity, LOG_TAG) << "PipeWire main loop exited with result: " << res << "\n";
        if (active_)
        {
            // sleep and run the main loop again
            LOG(INFO, LOG_TAG) << "Still active, sleeping before running the main loop again\n";
            this_thread::sleep_for(100ms);
            try
            {
                uninitPipewire();
            }
            catch (const std::exception& e)
            {
                LOG(ERROR, LOG_TAG) << "Exception while uninitializing PipeWire: " << e.what() << "\n";
            }

            try
            {
                initPipewire();
            }
            catch (const std::exception& e)
            {
                LOG(ERROR, LOG_TAG) << "Exception while initializing PipeWire: " << e.what() << "\n";
            }
        }
    }
}


bool PipeWirePlayer::needsThread() const
{
    return true;
}


void PipeWirePlayer::start()
{
    LOG(DEBUG, LOG_TAG) << "Start\n";
    initPipewire();
    Player::start();
}


void PipeWirePlayer::stop()
{
    LOG(INFO, LOG_TAG) << "Stop\n";
    Player::stop();
    uninitPipewire();
}


void PipeWirePlayer::onProcess()
{
    if (!active_)
    {
        pw_main_loop_quit(pw_main_loop_);
        return;
    }

    struct pw_buffer* b;
    if ((b = pw_stream_dequeue_buffer(pw_stream_)) == nullptr)
    {
        LOG(WARNING, LOG_TAG) << "No buffer available: " << strerror(errno) << "\n";
        return;
    }

    spa_buffer* buf = b->buffer;
    int16_t* dst;
    if ((dst = reinterpret_cast<int16_t*>(buf->datas[0].data)) == nullptr)
    {
        LOG(WARNING, LOG_TAG) << "Failed to get buffer\n";
        return;
    }

    const auto& sampleformat = stream_->getFormat();
    int stride = sampleformat.frameSize();
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
    int64_t delay_us = 0;
#if PW_CHECK_VERSION(0, 3, 50)
    if (pw_stream_get_time_n(pw_stream_, &time, sizeof(struct pw_time)) == 0)
#else
    if (pw_stream_get_time(pw_stream_, &time) == 0)
#endif
    {
        delay_us = time.delay * time.rate.num * 1000 * 1000 / time.rate.denom;
    }
    else
    {
        // Fallback to buffer-based estimate if timing query fails
        delay_us = (n_frames * 1000000) / sampleformat.rate();
    }

    // LOG(TRACE, LOG_TAG) << "Delay: " << time.delay << ", rate: " << time.rate.num << "/" << time.rate.denom << ", ms: " << delay_us / 1000 << "\n";
    if (!stream_->getPlayerChunkOrSilence(dst, chronos::usec(delay_us), n_frames))
    {
        // LOG(DEBUG, LOG_TAG) << "Failed to get chunk. Playing silence.\n";
    }
    else
    {
        adjustVolume(reinterpret_cast<char*>(dst), n_frames);
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = n_frames * stride;

    pw_stream_queue_buffer(pw_stream_, b);
}


void PipeWirePlayer::on_process(void* userdata)
{
    auto* player = static_cast<PipeWirePlayer*>(userdata);
    player->onProcess();
}


void PipeWirePlayer::onParamChanged(uint32_t id, const struct spa_pod* param)
{
    LOG(DEBUG, LOG_TAG) << "Stream param changed, type: " << id << "\n";

    if (id == SPA_PARAM_Props)
    {
        if (settings_.mixer.mode != ClientSettings::Mixer::Mode::hardware)
            return;

        int csize = 0, ctype = 0, n_vals = 0;
        float* vals = nullptr;
        int res = spa_pod_parse_object(param, SPA_TYPE_OBJECT_Props, nullptr, SPA_PROP_channelVolumes, SPA_POD_Array(&csize, &ctype, &n_vals, &vals));
        LOG(DEBUG, LOG_TAG) << "get SPA_PROP_channelVolumes result: " << res << "\n";
        if (res == 1)
        {
            LOG(DEBUG, LOG_TAG) << "csize: " << csize << ", ctype: " << ctype << ", n: " << n_vals << ", vals: " << vals[0] << "\n";
            volume_.volume = vals[0];
        }

        bool mute = false;
        res = spa_pod_parse_object(param, SPA_TYPE_OBJECT_Props, nullptr, SPA_PROP_mute, SPA_POD_Bool(&mute));
        LOG(DEBUG, LOG_TAG) << "get SPA_PROP_mute result: " << res << "\n";
        if (res == 1)
        {
            volume_.mute = mute;
        }

        LOG(INFO, LOG_TAG) << "Volume changed: " << volume_.volume << ", mute: " << volume_.mute << "\n";
        notifyVolumeChange(volume_);
        return;
    }

    if (id == SPA_PARAM_Format)
    {

        struct spa_audio_info_raw info;
        spa_zero(info);

        if (spa_format_audio_raw_parse(param, &info) < 0)
            return;

        LOG(INFO, LOG_TAG) << "Format changed - rate: " << info.rate << ", channels: " << info.channels << "\n";
    }
}


void PipeWirePlayer::on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param)
{
    if (!userdata || !param)
        return;

    auto* player = static_cast<PipeWirePlayer*>(userdata);
    player->onParamChanged(id, param);
}


void PipeWirePlayer::initPipewire()
{
    // Set up stream events
    spa_zero(stream_events_);
    stream_events_.version = PW_VERSION_STREAM_EVENTS;
    stream_events_.process = on_process;
    stream_events_.param_changed = on_param_changed;

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

    // clang-format off
    // Set up stream properties
    auto* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", 
        PW_KEY_MEDIA_CATEGORY, "Playback", 
        PW_KEY_MEDIA_ROLE, "Music", 
        // PW_KEY_MEDIA_CLASS, "Audio/Sink",
        PW_KEY_APP_NAME, "Snapclient",
        PW_KEY_APP_ID, "snapcast",
        PW_KEY_APP_ICON_NAME, "snapcast",
        PW_KEY_NODE_DESCRIPTION, "Snapcast Audio Stream",
        // PW_KEY_NODE_NAME, "TODO: Player name or instance id", 
        nullptr);
    // clang-format on

    if (node_latency_)
    {
        // Calculate latency in samples
        const SampleFormat& format = stream_->getFormat();
        uint32_t latency_samples = (node_latency_->count() * format.rate()) / 1000;
        std::string latency = std::to_string(latency_samples) + "/" + std::to_string(format.rate());
        LOG(INFO, LOG_TAG) << "Setting Node-latency to: " << node_latency_->count() << " ms, fraction: " << latency << "\n";
        pw_properties_set(props, PW_KEY_NODE_LATENCY, latency.c_str());
    }

    // Set target node if specified
    // Check if device exists (only for non-default devices)
    if (settings_.pcm_device.name != DEFAULT_DEVICE)
    {
        if (settings_.pcm_device.idx == -1)
        {
            LOG(WARNING, LOG_TAG) << "Device '" << settings_.pcm_device.name << "' not found, using default\n";
        }
        else
        {
            LOG(INFO, LOG_TAG) << "Using device '" << settings_.pcm_device.name << "'\n";
#if PW_CHECK_VERSION(0, 3, 64)
            pw_properties_set(props, PW_KEY_TARGET_OBJECT, settings_.pcm_device.name.c_str());
#else
            pw_properties_set(props, PW_KEY_NODE_TARGET, settings_.pcm_device.name.c_str());
#endif
        }
    }

    // Create stream, props ownership transferred to stream
    pw_stream_ = pw_stream_new_simple(pw_main_loop_get_loop(pw_main_loop_), "Snapcast", props, &stream_events_, this);

    if (!pw_stream_)
    {
        uninitPipewire();
        throw SnapException("Failed to create PipeWire stream");
    }

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
    if (res < 0)
    {
        uninitPipewire();
        throw SnapException("Failed to connect PipeWire stream: " + std::string(spa_strerror(res)));
    }
}

void PipeWirePlayer::uninitPipewire()
{
    if (pw_stream_)
    {
        pw_stream_disconnect(pw_stream_);
        pw_stream_destroy(pw_stream_);
        pw_stream_ = nullptr;
    }

    if (pw_main_loop_)
    {
        pw_main_loop_destroy(pw_main_loop_);
        pw_main_loop_ = nullptr;
    }
}

void PipeWirePlayer::setHardwareVolume(const Volume& volume)
{
    // https://franks-reich.net/posts/sending_messages_to_pipewire/
    if (!pw_stream_ || !pw_main_loop_)
        return;

    pw_loop_invoke(pw_main_loop_get_loop(pw_main_loop_),
                   []([[maybe_unused]] struct spa_loop* loop, [[maybe_unused]] bool async, [[maybe_unused]] uint32_t seq, [[maybe_unused]] const void* data,
                      [[maybe_unused]] size_t size, [[maybe_unused]] void* user_data) -> int
    {
        auto* self = static_cast<PipeWirePlayer*>(user_data);
        const auto* volume = static_cast<const Volume*>(data);
        LOG(TRACE, LOG_TAG) << "pw_loop_invoke - volume: " << volume->volume << ", mute: " << volume->mute << "\n";
        auto vol = static_cast<float>(volume->volume);
        std::array<float, 2> values = {vol, vol}; // Same volume for both channels

        int ret = pw_stream_set_control(self->pw_stream_, SPA_PROP_channelVolumes, 2, values.data(), 0);
        if (ret >= 0)
        {
            float muted = volume->mute ? 1.0f : 0.0f;
            ret = pw_stream_set_control(self->pw_stream_, SPA_PROP_mute, 1, &muted, 0);
        }

        if (ret >= 0)
            LOG(DEBUG, LOG_TAG) << "Set hardware volume to " << (volume->volume * 100.0) << "%, mute: " << volume->mute << "\n";
        else
            LOG(ERROR, LOG_TAG) << "Failed to set hardware volume: " << ret << "\n";

        return 0;
    },
                   0, &volume, sizeof(volume), true, this);
}

// Seems unused
bool PipeWirePlayer::getHardwareVolume(Volume& volume)
{
    std::ignore = volume;
    return false;
}

#if 0
bool PipeWirePlayer::getHardwareVolume(Volume& volume)
{
    if (!pw_stream_)
        return false;

    if (settings_.mixer.mode != ClientSettings::Mixer::Mode::hardware)
        return false;

    const pw_stream_control* ret = pw_stream_get_control(pw_stream_, SPA_PROP_channelVolumes);
    if (!ret)
    {
        LOG(ERROR, LOG_TAG) << "Failed to query 'SPA_PROP_channelVolumes': " << ret << "\n";
        return false;
    }

    // Take the volume of the first channel
    if (ret->n_values >= 1)
        volume.volume = ret->values[0];

    ret = pw_stream_get_control(pw_stream_, SPA_PROP_mute);
    if (!ret)
    {
        LOG(ERROR, LOG_TAG) << "Failed to query 'SPA_PROP_mute': " << ret << "\n";
        return false;
    }

    if (ret->n_values >= 1)
        volume.mute = (ret->values[0] == 1);

    LOG(DEBUG, LOG_TAG) << "getHardwareVolume: " << volume.volume << ", mute: " << volume.mute << "\n";

    return true;
}
#endif

#ifdef __clang__
#pragma GCC diagnostic pop
#endif


} // namespace player
