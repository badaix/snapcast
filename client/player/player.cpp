/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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
#include "player.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"

// 3rd party headers
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#if BOOST_VERSION >= 108000
#if defined(__clang__) && (__clang_major__ >= 13) && !((__clang_major__ == 13) && (__clang_minor__ == 0) && (__clang_patchlevel__ == 0))
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#pragma GCC diagnostic ignored "-Wpedantic"
#include <boost/process/v2.hpp>
#else
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wmissing-braces"
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#include <boost/process/args.hpp>
#include <boost/process/child.hpp>
#include <boost/process/exe.hpp>
#endif
#pragma GCC diagnostic pop

// standard headers
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>


using namespace std;

namespace player
{

static constexpr auto LOG_TAG = "Player";

Player::Player(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : io_context_(io_context), active_(false), stream_(stream), settings_(settings), volCorrection_(1.0)
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

    auto not_empty = [](const std::string& value) -> std::string
    {
        if (!value.empty())
            return value;
        else
            return "<none>";
    };
    LOG(INFO, LOG_TAG) << "Player name: " << not_empty(settings_.player_name) << ", device: " << not_empty(settings_.pcm_device.name)
                       << ", description: " << not_empty(settings_.pcm_device.description) << ", idx: " << settings_.pcm_device.idx
                       << ", sharing mode: " << sharing_mode << ", parameters: " << not_empty(settings.parameter) << "\n";

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
    // if (settings_.mixer.mode == ClientSettings::Mixer::Mode::hardware)
    // {
    //     if (getHardwareVolume(volume_, muted_))
    //     {
    //         LOG(DEBUG, LOG_TAG) << "Volume: " << volume_ << ", muted: " << muted_ << "\n";
    //         notifyVolumeChange(volume_, muted_);
    //     }
    // }
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


void Player::setHardwareVolume(const Volume& volume)
{
    std::ignore = volume;
    throw SnapException("Failed to set hardware mixer volume: not supported");
}


bool Player::getHardwareVolume(Volume& volume)
{
    std::ignore = volume;
    throw SnapException("Failed to get hardware mixer volume: not supported");
}


void Player::adjustVolume(char* buffer, size_t frames)
{
    std::lock_guard<std::mutex> lock(mutex_);
    double volume = volCorrection_;
    // apply volume changes only for software mixer
    // for any other mixer, we might still have to apply the volCorrection_
    if (settings_.mixer.mode == ClientSettings::Mixer::Mode::software)
    {
        volume = volume_.mute ? 0. : volume_.volume;
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
    volume_.volume = std::pow(volume, exp);
    LOG(DEBUG, LOG_TAG) << "setVolume poly with exp " << exp << ": " << volume << " => " << volume_.volume << "\n";
}


// http://stackoverflow.com/questions/1165026/what-algorithms-could-i-use-for-audio-volume-level
void Player::setVolume_exp(double volume, double base)
{
    //	double base = M_E;
    //	double base = 10.;
    volume_.volume = (pow(base, volume) - 1) / (base - 1);
    LOG(DEBUG, LOG_TAG) << "setVolume exp with base " << base << ": " << volume << " => " << volume_.volume << "\n";
}


void Player::setVolume(const Volume& volume)
{
    std::lock_guard<std::mutex> lock(mutex_);
    volume_ = volume;
    if (settings_.mixer.mode == ClientSettings::Mixer::Mode::hardware)
    {
        setHardwareVolume(volume);
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
            setVolume_poly(volume.volume, (dparam < 0) ? 3. : dparam);
        else
            setVolume_exp(volume.volume, (dparam < 0) ? 10. : dparam);
    }
    else if (settings_.mixer.mode == ClientSettings::Mixer::Mode::script)
    {
#if BOOST_VERSION >= 108000
        // Use Boost process v2 if Boost >= 1.80.0 is available
        static std::optional<Volume> pending_volume_setting;
        static bool script_running = false;
        if (script_running)
        {
            pending_volume_setting = volume;
            LOG(DEBUG, LOG_TAG) << "Volume script still running, deferring this volume setting\n";
        }
        else
        {
            try
            {
                namespace procv2 = boost::process::v2;
                script_running = true;
                procv2::async_execute(procv2::process(io_context_, settings_.mixer.parameter,
                                                      {"--volume", cpt::to_string(volume.volume), "--mute", volume.mute ? "true" : "false"}),
                                      [&](boost::system::error_code ec, int ret_val)
                                      {
                    std::unique_lock<std::mutex> lock(mutex_);
                    LOG(DEBUG, LOG_TAG) << "Error code: " << ec.message() << ", i: " << ret_val << "\n";
                    script_running = false;
                    if (pending_volume_setting.has_value())
                    {
                        Volume v = pending_volume_setting.value();
                        pending_volume_setting = std::nullopt;
                        lock.unlock();
                        setVolume(v);
                    }
                });
            }
            catch (const std::exception& e)
            {
                script_running = false;
                LOG(ERROR, LOG_TAG) << "Failed to run script '" + settings_.mixer.parameter + "', error: " << e.what() << "\n";
            }
        }
#else
        // Use Boost process v1
        try
        {
            using namespace boost::process;
            child c(exe = settings_.mixer.parameter, args = {"--volume", cpt::to_string(volume), "--mute", mute ? "true" : "false"});
            c.detach();
        }
        catch (const std::exception& e)
        {
            LOG(ERROR, LOG_TAG) << "Failed to run script '" + settings_.mixer.parameter + "', error: " << e.what() << "\n";
        }
#endif
    }
}

} // namespace player
