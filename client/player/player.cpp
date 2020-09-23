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

#include <cmath>
#include <iostream>

#ifdef WINDOWS
#include <cstdlib>
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-braces"
#include <boost/process/args.hpp>
#include <boost/process/child.hpp>
#include <boost/process/exe.hpp>
#pragma GCC diagnostic pop
#endif

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"
#include "player.hpp"


using namespace std;

static constexpr auto LOG_TAG = "Player";

Player::Player(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : io_context_(io_context), active_(false), stream_(stream), settings_(settings), volume_(1.0), muted_(false), volCorrection_(1.0)
{
    string sharing_mode;
    switch (settings_.sharing_mode)
    {
        case ClientSettings::SharingMode::unspecified:
            sharing_mode = "unspecified";
            break;
        case ClientSettings::SharingMode::exclusive:
            sharing_mode = "exclusive";
            break;
        case ClientSettings::SharingMode::shared:
            sharing_mode = "shared";
            break;
    }

    auto not_empty = [](const std::string& value) -> std::string {
        if (!value.empty())
            return value;
        else
            return "<none>";
    };
    LOG(INFO, LOG_TAG) << "Player name: " << not_empty(settings_.player_name) << ", device: " << not_empty(settings_.pcm_device.name)
                       << ", description: " << not_empty(settings_.pcm_device.description) << ", idx: " << settings_.pcm_device.idx
                       << ", sharing mode: " << sharing_mode << "\n";

    string mixer;
    switch (settings_.mixer.mode)
    {
        case ClientSettings::Mixer::Mode::hardware:
            mixer = "hardware";
            break;
        case ClientSettings::Mixer::Mode::software:
            mixer = "software";
            break;
        case ClientSettings::Mixer::Mode::script:
            mixer = "script";
            break;
        case ClientSettings::Mixer::Mode::none:
            mixer = "none";
            break;
    }
    LOG(INFO, LOG_TAG) << "Mixer mode: " << mixer << ", parameters: " << not_empty(settings_.mixer.parameter) << "\n";
    LOG(INFO, LOG_TAG) << "Sampleformat: " << (settings_.sample_format.isInitialized() ? settings_.sample_format.toString() : stream->getFormat().toString())
                       << ", stream: " << stream->getFormat().toString() << "\n";
}


Player::~Player()
{
    stop();
}


void Player::start()
{
    active_ = true;
    if (needsThread())
        playerThread_ = thread(&Player::worker, this);

    // If hardware mixer is used, send the initial volume to the server, because this is
    // the volume that is configured by the user on his local device, so we shouldn't change it
    // on client start up
    if (settings_.mixer.mode == ClientSettings::Mixer::Mode::hardware)
    {
        double volume;
        bool muted;
        if (settings_.mixer.hwmute == ClientSettings::Mixer::HWmute::bidir || settings_.mixer.hwmute == ClientSettings::Mixer::HWmute::client2server)
        {
            if (getHardwareVolume(volume, muted))
            {
                LOG(DEBUG, LOG_TAG) << "Volume: " << volume << ", muted: " << muted << "\n";
                notifyVolumeChange(volume, muted);
            }
        }
        else
        {
            if (getHardwareVolume(volume))
            {
                LOG(DEBUG, LOG_TAG) << "Volume: " << volume << ", muted: ignored "<< "\n";
                notifyVolumeChange(volume, false);
            }
        }
        
    }
}


void Player::stop()
{
    if (active_)
    {
        active_ = false;
        if (playerThread_.joinable())
            playerThread_.join();
    }
}


void Player::worker()
{
}


void Player::setHardwareVolume(double volume, bool muted)
{
    std::ignore = volume;
    std::ignore = muted;
    throw SnapException("Failed to set hardware mixer volume: not supported");
}


bool Player::getHardwareVolume(double& volume, bool& muted)
{
    std::ignore = volume;
    std::ignore = muted;
    throw SnapException("Failed to get hardware mixer volume: not supported");
    return false;
}

void Player::setHardwareVolume(double volume)
{
    std::ignore = volume;
    throw SnapException("Failed to set hardware mixer volume: not supported");
}


bool Player::getHardwareVolume(double& volume)
{
    std::ignore = volume;
    throw SnapException("Failed to get hardware mixer volume: not supported");
    return false;
}


void Player::adjustVolume(char* buffer, size_t frames)
{
    double volume = volCorrection_;
    // apply volume changes only for software mixer
    // for any other mixer, we might still have to apply the volCorrection_
    if (settings_.mixer.mode == ClientSettings::Mixer::Mode::software)
    {
        volume = muted_ ? 0. : volume_;
        volume *= volCorrection_;
    }

    if (volume != 1.0)
    {
        const SampleFormat& sampleFormat = stream_->getFormat();
        if (sampleFormat.sampleSize() == 1)
            adjustVolume<int8_t>(buffer, frames * sampleFormat.channels(), volume);
        else if (sampleFormat.sampleSize() == 2)
            adjustVolume<int16_t>(buffer, frames * sampleFormat.channels(), volume);
        else if (sampleFormat.sampleSize() == 4)
            adjustVolume<int32_t>(buffer, frames * sampleFormat.channels(), volume);
    }
}


// https://cgit.freedesktop.org/pulseaudio/pulseaudio/tree/src/pulse/volume.c#n260
// http://www.robotplanet.dk/audio/audio_gui_design/
// https://lists.linuxaudio.org/pipermail/linux-audio-dev/2009-May/thread.html#22198
void Player::setVolume_poly(double volume, double exp)
{
    volume_ = std::pow(volume, exp);
    LOG(DEBUG, LOG_TAG) << "setVolume poly with exp " << exp << ": " << volume << " => " << volume_ << "\n";
}


// http://stackoverflow.com/questions/1165026/what-algorithms-could-i-use-for-audio-volume-level
void Player::setVolume_exp(double volume, double base)
{
    //	double base = M_E;
    //	double base = 10.;
    volume_ = (pow(base, volume) - 1) / (base - 1);
    LOG(DEBUG, LOG_TAG) << "setVolume exp with base " << base << ": " << volume << " => " << volume_ << "\n";
}


void Player::setVolume(double volume, bool mute)
{
    volume_ = volume;
    muted_ = mute;
    if (settings_.mixer.mode == ClientSettings::Mixer::Mode::hardware)
    {
        if (settings_.mixer.hwmute == ClientSettings::Mixer::HWmute::bidir || settings_.mixer.hwmute == ClientSettings::Mixer::HWmute::server2client)
        {
            setHardwareVolume(volume, muted_);
        }
        else
        {
            setHardwareVolume(volume);
        }
    }
    else if (settings_.mixer.mode == ClientSettings::Mixer::Mode::software)
    {
        string param;
        string mode = utils::string::split_left(settings_.mixer.parameter, ':', param);
        double dparam = -1.;
        if (!param.empty())
        {
            try
            {
                dparam = cpt::stod(param);
                if (dparam < 0)
                    throw SnapException("must be a positive number");
            }
            catch (const std::exception& e)
            {
                throw SnapException("Invalid mixer param: " + param + ", error: " + string(e.what()));
            }
        }
        if (mode == "poly")
            setVolume_poly(volume, (dparam < 0) ? 3. : dparam);
        else
            setVolume_exp(volume, (dparam < 0) ? 10. : dparam);
    }
    else if (settings_.mixer.mode == ClientSettings::Mixer::Mode::script)
    {
        try
        {
#ifdef WINDOWS
            string cmd = settings_.mixer.parameter + " --volume " + cpt::to_string(volume) + " --mute " + (mute ? "true" : "false");
            std::system(cmd.c_str());
#else
            using namespace boost::process;
            child c(exe = settings_.mixer.parameter, args = {"--volume", cpt::to_string(volume), "--mute", mute ? "true" : "false"});
            c.detach();
#endif
        }
        catch (const std::exception& e)
        {
            LOG(ERROR, LOG_TAG) << "Failed to run script '" + settings_.mixer.parameter + "', error: " << e.what() << "\n";
        }
    }
}
