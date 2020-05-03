/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#include "alsa_player.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"

//#define BUFFER_TIME 120000
#define PERIOD_TIME 30000
#define exp10(x) (exp((x)*log(10)))

using namespace std;

static constexpr auto LOG_TAG = "Alsa";
static constexpr auto DEFAULT_MIXER = "PCM";


AlsaPlayer::AlsaPlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, stream), handle_(nullptr), ctl_(nullptr), mixer_(nullptr), elem_(nullptr), sd_(io_context), timer_(io_context)
{
    if (settings_.mixer.mode == ClientSettings::Mixer::Mode::hardware)
    {
        string tmp;
        if (settings_.mixer.parameter.empty())
            mixer_name_ = DEFAULT_MIXER;
        else
            utils::string::split_left(settings_.mixer.parameter, ':', mixer_name_, tmp);

        // default:CARD=ALSA => default
        utils::string::split_left(settings_.pcm_device.name, ':', mixer_device_, tmp);
        LOG(DEBUG, LOG_TAG) << "Mixer: " << mixer_name_ << ", device: " << mixer_device_ << "\n";
    }
}


void AlsaPlayer::setMute(bool mute)
{
    if (settings_.mixer.mode != ClientSettings::Mixer::Mode::hardware)
    {
        Player::setMute(mute);
        return;
    }

    int val = mute ? 0 : 1;
    int err = snd_mixer_selem_set_playback_switch(elem_, SND_MIXER_SCHN_MONO, val);
    if (err < 0)
        LOG(ERROR, LOG_TAG) << "Failed to mute, error: " << snd_strerror(err) << "\n";
}


void AlsaPlayer::setVolume(double volume)
{
    if (settings_.mixer.mode != ClientSettings::Mixer::Mode::hardware)
    {
        Player::setVolume(volume);
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    // boost::system::error_code ec;
    // sd_.cancel(ec);
    // if (ctl_)
    //     snd_ctl_subscribe_events(ctl_, 0);

    last_change_ = std::chrono::steady_clock::now();
    try
    {
        int err = 0;
        long minv, maxv;
        if ((err = snd_mixer_selem_get_playback_dB_range(elem_, &minv, &maxv)) == 0)
        {
            double min_norm = exp10((minv - maxv) / 6000.0);
            volume = volume * (1 - min_norm) + min_norm;
            double mixer_volume = 6000.0 * log10(volume) + maxv;

            LOG(DEBUG, LOG_TAG) << "Mixer volume range [" << minv << ", " << maxv << "], volume: " << volume << ", mixer volume: " << mixer_volume << "\n";
            if ((err = snd_mixer_selem_set_playback_dB_all(elem_, mixer_volume, 0)) < 0)
                throw SnapException(std::string("Failed to set playback volume, error: ") + snd_strerror(err));
        }
        else
        {
            if ((err = snd_mixer_selem_get_playback_volume_range(elem_, &minv, &maxv)) < 0)
                throw SnapException(std::string("Failed to get playback volume range, error: ") + snd_strerror(err));

            auto mixer_volume = volume * (maxv - minv) + minv;
            LOG(DEBUG, LOG_TAG) << "Mixer volume range [" << minv << ", " << maxv << "], volume: " << volume << ", mixer volume: " << mixer_volume << "\n";
            if ((err = snd_mixer_selem_set_playback_volume_all(elem_, mixer_volume)) < 0)
                throw SnapException(std::string("Failed to set playback volume, error: ") + snd_strerror(err));
        }
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << "\n";
    }

    // if (ctl_)
    // {
    //     snd_ctl_subscribe_events(ctl_, 1);
    //     waitForEvent();
    // }
}


bool AlsaPlayer::getVolume(double& volume, bool& muted)
{
    long vol;
    int err = 0;

    try
    {
        while (snd_mixer_handle_events(mixer_) > 0)
            this_thread::sleep_for(1us);
        long minv, maxv;
        if ((err = snd_mixer_selem_get_playback_dB_range(elem_, &minv, &maxv)) == 0)
        {
            if ((err = snd_mixer_selem_get_playback_dB(elem_, SND_MIXER_SCHN_MONO, &vol)) < 0)
                throw SnapException(std::string("Failed to get playback volume, error: ") + snd_strerror(err));

            volume = pow(10, (vol - maxv) / 6000.0);
            if (minv != SND_CTL_TLV_DB_GAIN_MUTE)
            {
                double min_norm = pow(10, (minv - maxv) / 6000.0);
                volume = (volume - min_norm) / (1 - min_norm);
            }
        }
        else
        {
            if ((err = snd_mixer_selem_get_playback_volume_range(elem_, &minv, &maxv)) < 0)
                throw SnapException(std::string("Failed to get playback volume range, error: ") + snd_strerror(err));
            if ((err = snd_mixer_selem_get_playback_volume(elem_, SND_MIXER_SCHN_MONO, &vol)) < 0)
                throw SnapException(std::string("Failed to get playback volume, error: ") + snd_strerror(err));

            vol -= minv;
            maxv = maxv - minv;
            volume = static_cast<double>(vol) / static_cast<double>(maxv);
        }
        int val;
        if ((err = snd_mixer_selem_get_playback_switch(elem_, SND_MIXER_SCHN_MONO, &val)) < 0)
            return false;
        muted = (val == 0);
        LOG(DEBUG, LOG_TAG) << "Get volume, mixer volume range [" << minv << ", " << maxv << "], volume: " << volume << ", muted: " << muted << "\n";
        snd_mixer_handle_events(mixer_);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << "\n";
        return false;
    }
}


void AlsaPlayer::waitForEvent()
{
    sd_.async_wait(boost::asio::posix::stream_descriptor::wait_read, [this](const boost::system::error_code& ec) {
        if (ec)
        {
            LOG(DEBUG, LOG_TAG) << "waitForEvent error: " << ec.message() << "\n";
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        unsigned short revents;
        snd_ctl_poll_descriptors_revents(ctl_, fd_.get(), 1, &revents);
        if (revents & POLLIN || (revents == 0))
        {
            snd_ctl_event_t* event;
            snd_ctl_event_alloca(&event);

            if (((snd_ctl_read(ctl_, event) >= 0) && (snd_ctl_event_get_type(event) == SND_CTL_EVENT_ELEM)) || (revents == 0))
            {
                auto now = std::chrono::steady_clock::now();
                if (now - last_change_ < 1s)
                {
                    LOG(DEBUG, LOG_TAG) << "Last volume change by server: " << std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change_).count()
                                        << " ms => ignoring volume change\n";
                    waitForEvent();
                    return;
                }
                // Sometimes the old volume is reported after this event has been raised.
                // As workaround we defer getting the volume by 20ms.
                timer_.cancel();
                timer_.expires_after(20ms);
                timer_.async_wait([this](const boost::system::error_code& ec) {
                    if (!ec)
                    {
                        double volume;
                        bool muted;
                        if (getVolume(volume, muted))
                        {
                            LOG(DEBUG, LOG_TAG) << "Volume: " << volume << ", muted: " << muted << "\n";
                            notifyVolumeChange(volume, muted);
                        }
                    }
                });
            }
        }
        waitForEvent();
    });
}

void AlsaPlayer::initMixer()
{
    std::lock_guard<std::mutex> lock(mutex_);
    int err;
    if ((err = snd_ctl_open(&ctl_, mixer_device_.c_str(), SND_CTL_READONLY)) < 0)
        throw SnapException("Can't open control for " + mixer_device_ + ", error: " + snd_strerror(err));
    if ((err = snd_ctl_subscribe_events(ctl_, 1)) < 0)
        throw SnapException("Can't subscribe for events for " + mixer_device_ + ", error: " + snd_strerror(err));
    fd_ = std::make_unique<pollfd>();
    err = snd_ctl_poll_descriptors(ctl_, fd_.get(), 1);
    LOG(DEBUG, LOG_TAG) << "Filled " << err << " poll descriptors, poll descriptor count: " << snd_ctl_poll_descriptors_count(ctl_) << "\n";

    snd_mixer_selem_id_t* sid;
    snd_mixer_selem_id_alloca(&sid);
    // TODO: make configurable
    int mix_index = 0;
    // sets simple-mixer index and name
    snd_mixer_selem_id_set_index(sid, mix_index);
    snd_mixer_selem_id_set_name(sid, mixer_name_.c_str());

    if ((err = snd_mixer_open(&mixer_, 0)) < 0)
        throw SnapException(std::string("Failed to open mixer, error: ") + snd_strerror(err));
    if ((err = snd_mixer_attach(mixer_, mixer_device_.c_str())) < 0)
        throw SnapException("Failed to attach mixer to " + mixer_device_ + ", error: " + snd_strerror(err));
    if ((err = snd_mixer_selem_register(mixer_, NULL, NULL)) < 0)
        throw SnapException(std::string("Failed to register selem, error: ") + snd_strerror(err));
    if ((err = snd_mixer_load(mixer_)) < 0)
        throw SnapException(std::string("Failed to load mixer, error: ") + snd_strerror(err));
    elem_ = snd_mixer_find_selem(mixer_, sid);
    if (!elem_)
        throw SnapException("Failed to find mixer: " + mixer_name_);

    sd_ = boost::asio::posix::stream_descriptor(io_context_, fd_->fd);
    waitForEvent();
}


void AlsaPlayer::initAlsa()
{
    unsigned int tmp, rate;
    int err, channels;
    snd_pcm_hw_params_t* params;

    const SampleFormat& format = stream_->getFormat();
    rate = format.rate();
    channels = format.channels();

    /* Open the PCM device in playback mode */
    if ((err = snd_pcm_open(&handle_, settings_.pcm_device.name.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
        throw SnapException("Can't open " + settings_.pcm_device.name + ", error: " + snd_strerror(err));

    /*	struct snd_pcm_playback_info_t pinfo;
     if ( (pcm = snd_pcm_playback_info( pcm_handle, &pinfo )) < 0 )
     fprintf( stderr, "Error: playback info error: %s\n", snd_strerror( err ) );
     printf("buffer: '%d'\n", pinfo.buffer_size);
     */
    /* Allocate parameters object and fill it with default values*/
    snd_pcm_hw_params_alloca(&params);

    if ((err = snd_pcm_hw_params_any(handle_, params)) < 0)
        throw SnapException("Can't fill params: " + string(snd_strerror(err)));

    /* Set parameters */
    if ((err = snd_pcm_hw_params_set_access(handle_, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        throw SnapException("Can't set interleaved mode: " + string(snd_strerror(err)));

    snd_pcm_format_t snd_pcm_format;
    if (format.bits() == 8)
        snd_pcm_format = SND_PCM_FORMAT_S8;
    else if (format.bits() == 16)
        snd_pcm_format = SND_PCM_FORMAT_S16_LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 4))
        snd_pcm_format = SND_PCM_FORMAT_S24_LE;
    else if (format.bits() == 32)
        snd_pcm_format = SND_PCM_FORMAT_S32_LE;
    else
        throw SnapException("Unsupported sample format: " + cpt::to_string(format.bits()));

    err = snd_pcm_hw_params_set_format(handle_, params, snd_pcm_format);
    if (err == -EINVAL)
    {
        if (snd_pcm_format == SND_PCM_FORMAT_S24_LE)
        {
            snd_pcm_format = SND_PCM_FORMAT_S32_LE;
            volCorrection_ = 256;
        }
        if (snd_pcm_format == SND_PCM_FORMAT_S8)
        {
            snd_pcm_format = SND_PCM_FORMAT_U8;
        }
    }

    err = snd_pcm_hw_params_set_format(handle_, params, snd_pcm_format);
    if (err < 0)
    {
        stringstream ss;
        ss << "Can't set format: " << string(snd_strerror(err)) << ", supported: ";
        for (int format = 0; format <= (int)SND_PCM_FORMAT_LAST; format++)
        {
            snd_pcm_format_t snd_pcm_format = static_cast<snd_pcm_format_t>(format);
            if (snd_pcm_hw_params_test_format(handle_, params, snd_pcm_format) == 0)
                ss << snd_pcm_format_name(snd_pcm_format) << " ";
        }
        throw SnapException(ss.str());
    }

    if ((err = snd_pcm_hw_params_set_channels(handle_, params, channels)) < 0)
        throw SnapException("Can't set channels number: " + string(snd_strerror(err)));

    if ((err = snd_pcm_hw_params_set_rate_near(handle_, params, &rate, nullptr)) < 0)
        throw SnapException("Can't set rate: " + string(snd_strerror(err)));

    unsigned int period_time;
    snd_pcm_hw_params_get_period_time_max(params, &period_time, nullptr);
    if (period_time > PERIOD_TIME)
        period_time = PERIOD_TIME;

    unsigned int buffer_time = 4 * period_time;

    snd_pcm_hw_params_set_period_time_near(handle_, params, &period_time, nullptr);
    snd_pcm_hw_params_set_buffer_time_near(handle_, params, &buffer_time, nullptr);

    //	long unsigned int periodsize = stream_->format.msRate() * 50;//2*rate/50;
    //	if ((pcm = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, params, &periodsize)) < 0)
    //		LOG(ERROR, LOG_TAG) << "Unable to set buffer size " << (long int)periodsize << ": " <<  snd_strerror(pcm) << "\n";

    /* Write parameters */
    if ((err = snd_pcm_hw_params(handle_, params)) < 0)
        throw SnapException("Can't set hardware parameters: " + string(snd_strerror(err)));

    /* Resume information */
    LOG(DEBUG, LOG_TAG) << "PCM name: " << snd_pcm_name(handle_) << "\n";
    LOG(DEBUG, LOG_TAG) << "PCM state: " << snd_pcm_state_name(snd_pcm_state(handle_)) << "\n";
    snd_pcm_hw_params_get_channels(params, &tmp);
    LOG(DEBUG, LOG_TAG) << "channels: " << tmp << "\n";

    snd_pcm_hw_params_get_rate(params, &tmp, nullptr);
    LOG(DEBUG, LOG_TAG) << "rate: " << tmp << " bps\n";

    /* Allocate buffer to hold single period */
    snd_pcm_hw_params_get_period_size(params, &frames_, nullptr);
    LOG(INFO, LOG_TAG) << "frames: " << frames_ << "\n";

    snd_pcm_hw_params_get_period_time(params, &tmp, nullptr);
    LOG(DEBUG, LOG_TAG) << "period time: " << tmp << "\n";

    snd_pcm_sw_params_t* swparams;
    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_sw_params_current(handle_, swparams);

    snd_pcm_sw_params_set_avail_min(handle_, swparams, frames_);
    snd_pcm_sw_params_set_start_threshold(handle_, swparams, frames_);
    //	snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams, frames_);
    snd_pcm_sw_params(handle_, swparams);
}


void AlsaPlayer::uninitAlsa()
{
    if (handle_ != nullptr)
    {
        snd_pcm_drain(handle_);
        snd_pcm_close(handle_);
        handle_ = nullptr;
    }
}


void AlsaPlayer::uninitMixer()
{
    if (sd_.is_open())
    {
        boost::system::error_code ec;
        sd_.cancel(ec);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (ctl_ != nullptr)
    {
        snd_ctl_close(ctl_);
        ctl_ = nullptr;
    }
    if (mixer_ != nullptr)
    {
        snd_mixer_close(mixer_);
        mixer_ = nullptr;
    }
    elem_ = nullptr;
}


void AlsaPlayer::start()
{
    initAlsa();
    if (settings_.mixer.mode == ClientSettings::Mixer::Mode::hardware)
        initMixer();
    Player::start();
}


AlsaPlayer::~AlsaPlayer()
{
    stop();
}


void AlsaPlayer::stop()
{
    Player::stop();
    uninitMixer();
    uninitAlsa();
}


bool AlsaPlayer::needsThread() const
{
    return true;
}


void AlsaPlayer::worker()
{
    snd_pcm_sframes_t pcm;
    snd_pcm_sframes_t framesDelay;
    snd_pcm_sframes_t framesAvail;
    long lastChunkTick = chronos::getTickCount();
    const SampleFormat& format = stream_->getFormat();
    while (active_)
    {
        if (handle_ == nullptr)
        {
            try
            {
                initAlsa();
            }
            catch (const std::exception& e)
            {
                LOG(ERROR, LOG_TAG) << "Exception in initAlsa: " << e.what() << endl;
                chronos::sleep(100);
            }
            if (handle_ == nullptr)
                continue;
        }

        int wait_result = snd_pcm_wait(handle_, 100);
        if (wait_result == -EPIPE)
        {
            LOG(ERROR, LOG_TAG) << "XRUN: " << snd_strerror(wait_result) << "\n";
            snd_pcm_prepare(handle_);
        }
        else if (wait_result < 0)
        {
            LOG(ERROR, LOG_TAG) << "ERROR. Can't wait for PCM to become ready: " << snd_strerror(wait_result) << "\n";
            uninitAlsa();
        }
        else if (wait_result == 0)
        {
            continue;
        }

        int result = snd_pcm_avail_delay(handle_, &framesAvail, &framesDelay);
        if (result < 0)
        {
            // if (result == -EPIPE)
            //     snd_pcm_prepare(handle_);
            // else
            //     uninitAlsa();
            LOG(WARNING, LOG_TAG) << "snd_pcm_avail_delay failed: " << snd_strerror(result) << ", avail: " << framesAvail << ", delay: " << framesDelay
                                  << ", retrying.\n";
            this_thread::sleep_for(5ms);
            int result = snd_pcm_avail_delay(handle_, &framesAvail, &framesDelay);
            if (result < 0)
            {
                this_thread::sleep_for(5ms);
                LOG(WARNING, LOG_TAG) << "snd_pcm_avail_delay failed again: " << snd_strerror(result) << ", avail: " << framesAvail
                                      << ", delay: " << framesDelay << ", using snd_pcm_avail and snd_pcm_delay.\n";
                framesAvail = snd_pcm_avail(handle_);
                result = snd_pcm_delay(handle_, &framesDelay);
                if ((result < 0) || (framesAvail <= 0) || (framesDelay <= 0))
                {
                    LOG(WARNING, LOG_TAG) << "snd_pcm_avail and snd_pcm_delay failed: " << snd_strerror(result) << ", avail: " << framesAvail
                                          << ", delay: " << framesDelay << "\n";
                    this_thread::sleep_for(10ms);
                    snd_pcm_prepare(handle_);
                    continue;
                }
            }
        }

        chronos::usec delay(static_cast<chronos::usec::rep>(1000 * (double)framesDelay / format.msRate()));
        // LOG(TRACE, LOG_TAG) << "delay: " << framesDelay << ", delay[ms]: " << delay.count() / 1000 << ", avail: " << framesAvail << "\n";

        if (buffer_.size() < static_cast<size_t>(framesAvail * format.frameSize()))
        {
            LOG(DEBUG, LOG_TAG) << "Resizing buffer from " << buffer_.size() << " to " << framesAvail * format.frameSize() << "\n";
            buffer_.resize(framesAvail * format.frameSize());
        }
        if (stream_->getPlayerChunk(buffer_.data(), delay, framesAvail))
        {
            lastChunkTick = chronos::getTickCount();
            adjustVolume(buffer_.data(), framesAvail);
            if ((pcm = snd_pcm_writei(handle_, buffer_.data(), framesAvail)) == -EPIPE)
            {
                LOG(ERROR, LOG_TAG) << "XRUN: " << snd_strerror(pcm) << "\n";
                snd_pcm_prepare(handle_);
            }
            else if (pcm < 0)
            {
                LOG(ERROR, LOG_TAG) << "ERROR. Can't write to PCM device: " << snd_strerror(pcm) << "\n";
                uninitAlsa();
            }
        }
        else
        {
            LOG(INFO, LOG_TAG) << "Failed to get chunk\n";
            while (active_ && !stream_->waitForChunk(100ms))
            {
                LOG(DEBUG, LOG_TAG) << "Waiting for chunk\n";
                if ((handle_ != nullptr) && (chronos::getTickCount() - lastChunkTick > 5000))
                {
                    LOG(NOTICE, LOG_TAG) << "No chunk received for 5000ms. Closing ALSA.\n";
                    uninitAlsa();
                    stream_->clearChunks();
                }
            }
        }
    }
}



vector<PcmDevice> AlsaPlayer::pcm_list()
{
    void **hints, **n;
    char *name, *descr, *io;
    vector<PcmDevice> result;
    PcmDevice pcmDevice;

    if (snd_device_name_hint(-1, "pcm", &hints) < 0)
        return result;
    n = hints;
    size_t idx(0);
    while (*n != nullptr)
    {
        name = snd_device_name_get_hint(*n, "NAME");
        descr = snd_device_name_get_hint(*n, "DESC");
        io = snd_device_name_get_hint(*n, "IOID");
        if (io != nullptr && strcmp(io, "Output") != 0)
            goto __end;
        pcmDevice.name = name;
        if (descr == nullptr)
        {
            pcmDevice.description = "";
        }
        else
        {
            pcmDevice.description = descr;
        }
        pcmDevice.idx = idx++;
        result.push_back(pcmDevice);

    __end:
        if (name != nullptr)
            free(name);
        if (descr != nullptr)
            free(descr);
        if (io != nullptr)
            free(io);
        n++;
    }
    snd_device_name_free_hint(hints);
    return result;
}
