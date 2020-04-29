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

#ifndef PLAYER_H
#define PLAYER_H

#include "client_settings.hpp"
#include "common/aixlog.hpp"
#include "common/endian.hpp"
#include "stream.hpp"

#include <boost/asio.hpp>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>


/// Audio Player
/**
 * Abstract audio player implementation
 */
class Player
{
    using volume_callback = std::function<void(double volume, bool muted)>;

public:
    Player(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    virtual ~Player();

    /// Set audio volume in range [0..1]
    virtual void setVolume(double volume);
    virtual void setMute(bool mute);
    virtual void start();
    virtual void stop();
    void setVolumeCallback(const volume_callback& callback)
    {
        onVolumeChanged_ = callback;
    }

protected:
    virtual void worker();
    virtual bool needsThread() const = 0;

    void setVolume_poly(double volume, double exp);
    void setVolume_exp(double volume, double base);

    void adjustVolume(char* buffer, size_t frames);
    void notifyVolumeChange(double volume, bool muted) const
    {
        if (onVolumeChanged_)
            onVolumeChanged_(volume, muted);
    }

    boost::asio::io_context& io_context_;
    std::atomic<bool> active_;
    std::shared_ptr<Stream> stream_;
    std::thread playerThread_;
    ClientSettings::Player settings_;
    double volume_;
    bool muted_;
    double volCorrection_;
    volume_callback onVolumeChanged_;

private:
    template <typename T>
    void adjustVolume(char* buffer, size_t count, double volume)
    {
        T* bufferT = (T*)buffer;
        for (size_t n = 0; n < count; ++n)
            bufferT[n] = endian::swap<T>(static_cast<T>(endian::swap<T>(bufferT[n]) * volume));
    }
};


#endif
