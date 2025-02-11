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

#pragma once

// local headers
#include "client_settings.hpp"
#include "common/endian.hpp"
#include "stream.hpp"

// 3rd party headers
#include <boost/asio/io_context.hpp>

// standard headers
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>


#if !defined(WINDOWS)
#define SUPPORTS_VOLUME_SCRIPT
#endif

namespace player
{

/// Audio Player
/**
 * Abstract audio player implementation
 */
class Player
{
public:
    /// Volume
    struct Volume
    {
        double volume{1.0}; ///< volume [0..1]
        bool mute{false}; ///< muted?
    };

    using volume_callback = std::function<void(const Volume& volume)>;

    /// c'tor
    Player(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    /// d'tor
    virtual ~Player();

    /// Set audio volume in range [0..1]
    /// @param volume the volume on range [0..1], muted or not
    virtual void setVolume(const Volume& volume);

    /// Called on start, before the first audio sample is sent or any other function is called.
    /// In case of hardware mixer, it will call getVolume and notify the server about the current volume
    virtual void start();
    /// Called on stop
    virtual void stop();
    /// Sets the hardware volume change callback
    void setVolumeCallback(const volume_callback& callback)
    {
        onVolumeChanged_ = callback;
    }

protected:
    /// will be run in a thread if needsThread is true
    virtual void worker();
    /// @return true if the worker function should be started in a thread
    virtual bool needsThread() const = 0;

    /// get the hardware mixer volume
    /// @param[out] volume the volume on range [0..1], muted or not
    /// @return success or not
    virtual bool getHardwareVolume(Volume& volume);

    /// set the hardware mixer volume
    /// @param volume the volume on range [0..1], muted or not
    /// @param muted muted or not
    virtual void setHardwareVolume(const Volume& volume);

    /// set volume polynomial: volume^exp
    void setVolume_poly(double volume, double exp);
    /// set volume exponential: (base^volume - 1) / (base - 1)
    void setVolume_exp(double volume, double base);

    /// adjust volume of buffer by the current volume setting
    void adjustVolume(char* buffer, size_t frames);

    /// Notify the server about hardware volume changes
    /// @param volume the volume in range [0..1]
    /// @param muted if muted or not
    void notifyVolumeChange(const Volume& volume) const
    {
        if (onVolumeChanged_)
            onVolumeChanged_(volume);
    }

    /// asio IO context
    boost::asio::io_context& io_context_;
    /// is the player started?
    std::atomic<bool> active_;
    /// the played stream
    std::shared_ptr<Stream> stream_;
    /// runs the worker function if "needsThread" is true
    std::thread playerThread_;
    /// player settings
    ClientSettings::Player settings_;
    /// player volume
    Player::Volume volume_;
    /// current volume correction factor (according to "setVolume")
    double volCorrection_;
    /// callback for local volume changes that are reported back to the server
    volume_callback onVolumeChanged_;
    /// mutex
    mutable std::mutex mutex_;

private:
    template <typename T>
    void adjustVolume(char* buffer, size_t count, double volume)
    {
        auto* bufferT = reinterpret_cast<T*>(buffer);
        for (size_t n = 0; n < count; ++n)
            bufferT[n] = endian::swap<T>(static_cast<T>(endian::swap<T>(bufferT[n]) * volume));
    }
};

inline bool operator==(const Player::Volume& lhs, const Player::Volume& rhs)
{
    return ((lhs.volume == rhs.volume) && (lhs.mute == rhs.mute));
}

inline bool operator!=(const Player::Volume& lhs, const Player::Volume& rhs)
{
    return !(lhs == rhs);
}

} // namespace player
