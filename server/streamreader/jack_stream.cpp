/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl
    Copyright (C) 2024  Marcus Weseloh <marcus@weseloh.cc>

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
#include "jack_stream.hpp"

#include <jack/jack.h>

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"

// standard headers
#include <memory>

using namespace std;
using namespace std::chrono_literals;

namespace streamreader
{

static constexpr auto LOG_TAG = "JackStream";


void float_to_s32(char* dst, jack_default_audio_sample_t* src, unsigned long nsamples, unsigned long dst_skip)
{
    while (nsamples--)
    {
        // float to S32 conversion
        double clipped = fmin(1.0f, fmax((double)(*src), -1.0f));
        double scaled = clipped * 2147483647.0;
        *(int32_t*)dst = lrint(scaled);

        dst += dst_skip;
        src++;
    }
}

void float_to_s24(char* dst, jack_default_audio_sample_t* src, unsigned long nsamples, unsigned long dst_skip)
{
    int32_t tmp;

    while (nsamples--)
    {

        // float to S24 conversion
        if (*src <= -1.0f)
        {
            tmp = -8388607;
        }
        else if (*src >= 1.0f)
        {
            tmp = 8388607;
        }
        else
        {
            tmp = lrintf(*src * 8388607.0);
        }

#if __BYTE_ORDER == __LITTLE_ENDIAN
        memcpy(dst, &tmp, 3);
#elif __BYTE_ORDER == __BIG_ENDIAN
        memcpy(dst, (char*)&tmp + 1, 3);
#endif
        dst += dst_skip;
        src++;
    }
}

void float_to_s16(char* dst, jack_default_audio_sample_t* src, unsigned long nsamples, unsigned long dst_skip)
{
    while (nsamples--)
    {

        // float to S16 conversion
        if (*src <= -1.0f)
        {
            *((int16_t*)dst) = -32767;
        }
        else if (*src >= 1.0f)
        {
            *((int16_t*)dst) = 32767;
        }
        else
        {
            *((int16_t*)dst) = lrintf(*src * 32767.0);
        }

        dst += dst_skip;
        src++;
    }
}

namespace
{
template <typename Rep, typename Period>
void wait(boost::asio::steady_timer& timer, const std::chrono::duration<Rep, Period>& duration, std::function<void()> handler)
{
    timer.expires_after(duration);
    timer.async_wait(
        [handler = std::move(handler)](const boost::system::error_code& ec)
        {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Error during async wait: " << ec.message() << "\n";
        }
        else
        {
            handler();
        }
    });
}
} // namespace

JackStream::JackStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : PcmStream(pcmListener, ioc, server_settings, uri), read_timer_(strand_), silence_(0ms), first_(true)
{

    serverName_ = uri_.getQuery("server_name", "default");

    send_silence_ = (uri_.getQuery("send_silence", "false") == "true");
    idle_threshold_ = std::chrono::milliseconds(std::max(cpt::stoi(uri_.getQuery("idle_threshold", "100")), 10));

    doAutoConnect_ = (uri_.query.find("autoconnect") != uri_.query.end());
    autoConnectRegex_ = uri_.getQuery("autoconnect", "");
    autoConnectSkip_ = cpt::stoi(uri_.getQuery("autoconnect_skip", "0"));

    switch (sampleFormat_.bits())
    {
        case 16:
            interleave_func_ = float_to_s16;
            break;
        case 24:
            interleave_func_ = float_to_s24;
            break;
        case 32:
            interleave_func_ = float_to_s32;
            break;
        default:
            throw SnapException("JackStreams only support 16, 24 and 32 bit sample formats");
    }

    jack_set_error_function([](const char* msg) { LOG(ERROR, LOG_TAG) << "Jack Error: " << msg << "\n"; });
    jack_set_info_function([](const char* msg) { LOG(DEBUG, LOG_TAG) << msg << "\n"; });
}


void JackStream::start()
{
    LOG(TRACE, LOG_TAG) << "JackStream::start()\n";

    // Need to start immediately, otherwise client will fail
    // due to a zero-length pcm header message
    first_ = true;
    tvEncodedChunk_ = std::chrono::steady_clock::now();
    PcmStream::start();

    boost::asio::post(strand_, [this] { tryConnect(); });
}


void JackStream::stop()
{
    LOG(TRACE, LOG_TAG) << "JackStream::stop()\n";

    PcmStream::stop();
    closeJackConnection();
}


/**
 * Try to connect to the jack server. If it fails, wait
 * a little bit and try again.
 */
void JackStream::tryConnect()
{
    try
    {
        if (!openJackConnection())
        {
            LOG(WARNING, LOG_TAG) << "Jack connection failed, trying again in 5 seconds\n";
            wait(read_timer_, 5s, [this] { tryConnect(); });
            return;
        }

        if (!createJackPorts())
        {
            LOG(ERROR, LOG_TAG) << "Failed to create Jack ports, trying again in 5 seconds\n";
            closeJackConnection();
            wait(read_timer_, 5s, [this] { tryConnect(); });
            return;
        }

        if (doAutoConnect_)
        {
            autoConnectPorts();
        }
    }
    catch (exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Error during Jack connection: " << e.what() << "\n";
        stop();
    }
}


bool JackStream::openJackConnection()
{
    char* serverName = serverName_.data();
    jack_options_t options = (jack_options_t)(JackNoStartServer | JackServerName);

    client_ = jack_client_open(name_.c_str(), options, nullptr, serverName);
    if (client_ == nullptr)
    {
        return false;
    }

    LOG(INFO, LOG_TAG) << "Connected Jack client " << jack_get_client_name(client_) << "\n";

    jack_nframes_t jack_sample_rate = jack_get_sample_rate(client_);
    if (jack_sample_rate != sampleFormat_.rate())
    {
        throw SnapException("Jack streams must match the sample rate of the Jack server. "
                            "The server sample rate is " +
                            cpt::to_string(jack_sample_rate) + ".");
    }

    jack_set_process_callback(
        client_, [](jack_nframes_t nframes, void* arg) { return static_cast<JackStream*>(arg)->readJackBuffers(nframes); }, this);
    jack_on_shutdown(
        client_, [](void* arg) { static_cast<JackStream*>(arg)->onJackShutdown(); }, this);
    jack_set_port_registration_callback(
        client_, [](jack_port_id_t port_id, int registered, void* arg) { return static_cast<JackStream*>(arg)->onJackPortRegistration(port_id, registered); },
        this);

    int err = jack_activate(client_);
    if (err)
    {
        LOG(ERROR, LOG_TAG) << "Failed to activate Jack client " << name_ << ": " << err << "\n";
        closeJackConnection();
        return false;
    }

    return true;
}

bool JackStream::createJackPorts()
{
    if (ports_.size() > 0)
    {
        throw SnapException("Jack ports already created!");
    }

    int channelCount = sampleFormat_.channels();
    ports_.reserve(channelCount);

    // Register input ports
    for (int i = 0; i < channelCount; ++i)
    {
        std::string portName = "input_" + std::to_string(i);
        jack_port_t* port = jack_port_register(client_, portName.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        if (port == nullptr)
        {
            LOG(ERROR, LOG_TAG) << name_ << ": failed to register port " << portName << "\n";
            return false;
        }
        ports_.push_back(port);
    }

    return true;
}

/**
 * Reads the Jack port buffers, interlaces and converts the samples
 * to the stream format and adds the resulting data to the
 * ringbuffer, along with the jack timing information.
 */
int JackStream::readJackBuffers(jack_nframes_t nframes)
{
    int bytes_per_frame = sampleFormat_.sampleSize();
    int num_channels = sampleFormat_.channels();
    int payload_skip = bytes_per_frame * num_channels;

    // resize chunk payload buffer to match the Jack buffer size, if required
    if (chunk_->getFrameCount() != nframes)
    {
        LOG(TRACE, LOG_TAG) << "Resizing chunk to " << nframes << " frames\n";
        chunk_->setFrameCount(nframes);
        silent_chunk_ = std::vector<char>(chunk_->payloadSize, 0);
    }

    int connectedPorts = 0;

    for (size_t i = 0; i < ports_.size(); i++)
    {
        int payload_offset = bytes_per_frame * i;
        jack_port_t* port = ports_[i];

        if (jack_port_connected(port))
        {
            connectedPorts++;
        }

        jack_default_audio_sample_t* buf = static_cast<jack_default_audio_sample_t*>(jack_port_get_buffer(port, nframes));

        if (buf == nullptr)
        {
            LOG(ERROR, LOG_TAG) << "Unable to get Jack port buffer!\n";
            return -1;
        }

        interleave_func_(chunk_->payload + payload_offset, buf, nframes, payload_skip);
    }

    if (connectedPorts == 0 || isSilent(*chunk_))
    {
        silence_ += chunk_->duration<std::chrono::microseconds>();
        if (silence_ > idle_threshold_)
        {
            setState(ReaderState::kIdle);
            first_ = true;
        }
    }
    else
    {
        silence_ = 0ms;
        setState(ReaderState::kPlaying);
        if (first_)
        {
            first_ = false;
            tvEncodedChunk_ = std::chrono::steady_clock::now() - chunk_->duration<chrono::nanoseconds>();
        }
    }

    if ((state_ == ReaderState::kPlaying) || ((state_ == ReaderState::kIdle) && send_silence_))
    {
        chunkRead(*chunk_);
    }

    return 0;
}

void JackStream::closeJackConnection()
{
    if (client_ == nullptr)
    {
        return;
    }

    LOG(INFO, LOG_TAG) << "Closing Jack connection for " << name_ << "\n";

    ports_.clear();

    jack_client_close(client_);
    client_ = nullptr;
}

void JackStream::onJackPortRegistration(jack_port_id_t port_id, int registered)
{
    if (!doAutoConnect_ || !registered)
    {
        return;
    }

    jack_port_t* port = jack_port_by_id(client_, port_id);
    if (port == nullptr)
    {
        return;
    }

    if (jack_port_is_mine(client_, port))
    {
        return;
    }

    boost::asio::post(strand_, [this] { autoConnectPorts(); });
}

void JackStream::autoConnectPorts()
{
    const char** portNames = jack_get_ports(client_, autoConnectRegex_.c_str(), nullptr, JackPortIsOutput);

    if (portNames == nullptr)
    {
        return;
    }

    size_t portIdx = 0;
    int nameIdx = 0;

    while (portIdx < ports_.size() && portNames[nameIdx] != nullptr)
    {
        if (nameIdx < autoConnectSkip_)
        {
            nameIdx++;
            continue;
        }

        if (!jack_port_connected_to(ports_[portIdx], portNames[nameIdx]))
        {

            const char* localPortName = jack_port_name(ports_[portIdx]);
            int err = jack_connect(client_, portNames[nameIdx], localPortName);
            if (err != 0)
            {
                LOG(ERROR, LOG_TAG) << "Unable to autoconnect " << localPortName << " to " << portNames[nameIdx] << "(Error: " << err << ")\n";
            }
        }
        portIdx++;
        nameIdx++;
    }

    jack_free(portNames);
}

void JackStream::onJackShutdown()
{
    LOG(ERROR, LOG_TAG) << "Jack has shut down, trying to connect again!\n";
    ports_.clear();
    client_ = nullptr;
    wait(read_timer_, 1000ms, [this] { tryConnect(); });
}

} // namespace streamreader
