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

#pragma once

#pragma warning(push)
#pragma warning(disable : 4100)

// local headers
#include "player.hpp"

// 3rd party headers
#include <audiopolicy.h>
#include <endpointvolume.h>


namespace player
{

/// Implementation of IAudioSessionEvents
class AudioSessionEventListener : public IAudioSessionEvents
{
    LONG _cRef;

    float volume_ = 1.f;
    bool muted_ = false;

public:
    /// c'tor
    AudioSessionEventListener() : _cRef(1)
    {
    }

    /// @return volume
    float getVolume()
    {
        return volume_;
    }

    /// @return if muted
    bool getMuted()
    {
        return muted_;
    }

    /// d'tor
    ~AudioSessionEventListener()
    {
    }

    // IUnknown methods -- AddRef, Release, and QueryInterface

    /// documentation missing
    ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    /// documentation missing
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ulRef = InterlockedDecrement(&_cRef);
        if (0 == ulRef)
        {
            delete this;
        }
        return ulRef;
    }

    /// documentation missing
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface);

    // Notification methods for audio session events

    /// OnDisplayNameChanged callback
    HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext)
    {
        return S_OK;
    }

    /// OnIconPathChanged callback
    HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext)
    {
        return S_OK;
    }

    /// OnSimpleVolumeChanged callback
    HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext);

    /// OnChannelVolumeChanged callback
    HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD ChannelCount, float NewChannelVolumeArray[], DWORD ChangedChannel, LPCGUID EventContext)
    {
        return S_OK;
    }

    /// OnGroupingParamChanged callback
    HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext)
    {
        return S_OK;
    }

    /// OnStateChanged callback
    HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState NewState);

    /// OnSessionDisconnected callback
    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason);
};


/// Implementation of IAudioEndpointVolumeCallback
class AudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback
{
    LONG _cRef;
    float volume_ = 1.f;
    bool muted_ = false;

public:
    /// c'tor
    AudioEndpointVolumeCallback() : _cRef(1)
    {
    }

    /// d'tor
    ~AudioEndpointVolumeCallback()
    {
    }

    /// @return volume
    float getVolume()
    {
        return volume_;
    }

    /// @return if muted
    bool getMuted()
    {
        return muted_;
    }

    // IUnknown methods -- AddRef, Release, and QueryInterface

    /// documentation missing
    ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    /// documentation missing
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ulRef = InterlockedDecrement(&_cRef);
        if (0 == ulRef)
        {
            delete this;
        }
        return ulRef;
    }

    /// documentation missing
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface)
    {
        if (IID_IUnknown == riid)
        {
            AddRef();
            *ppvInterface = (IUnknown*)this;
        }
        else if (__uuidof(IAudioEndpointVolumeCallback) == riid)
        {
            AddRef();
            *ppvInterface = (IAudioEndpointVolumeCallback*)this;
        }
        else
        {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }

        return S_OK;
    }

    // Callback method for endpoint-volume-change notifications.

    /// documentation missing
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify);
};

static constexpr auto WASAPI = "wasapi";

/// WASAPI player
class WASAPIPlayer : public Player
{
public:
    /// c'tor
    WASAPIPlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    /// d'tor
    virtual ~WASAPIPlayer();

    /// @return list of available PCM devices
    static std::vector<PcmDevice> pcm_list();

protected:
    void worker() override;
    bool needsThread() const override
    {
        return true;
    }

private:
    AudioSessionEventListener* audioEventListener_;
    IAudioEndpointVolume* audioEndpointListener_;
    AudioEndpointVolumeCallback audioEndpointVolumeCallback_;
    ClientSettings::SharingMode mode_;
};

#pragma warning(pop)

} // namespace player
