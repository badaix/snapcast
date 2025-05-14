/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

#ifndef WASAPI_PLAYER_HPP
#define WASAPI_PLAYER_HPP

#pragma warning(push)
#pragma warning(disable : 4100)

// local headers
#include "player.hpp"

// 3rd party headers
#include <audiopolicy.h>
#include <endpointvolume.h>


namespace player
{

class AudioSessionEventListener : public IAudioSessionEvents
{
    LONG _cRef;

    float volume_ = 1.f;
    bool muted_ = false;

public:
    AudioSessionEventListener() : _cRef(1)
    {
    }

    float getVolume()
    {
        return volume_;
    }

    bool getMuted()
    {
        return muted_;
    }

    ~AudioSessionEventListener()
    {
    }

    // IUnknown methods -- AddRef, Release, and QueryInterface

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ulRef = InterlockedDecrement(&_cRef);
        if (0 == ulRef)
        {
            delete this;
        }
        return ulRef;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface);

    // Notification methods for audio session events

    HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext);

    HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD ChannelCount, float NewChannelVolumeArray[], DWORD ChangedChannel, LPCGUID EventContext)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState NewState);

    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason);
};


class AudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback
{
    LONG _cRef;
    float volume_ = 1.f;
    bool muted_ = false;

public:
    AudioEndpointVolumeCallback() : _cRef(1)
    {
    }

    ~AudioEndpointVolumeCallback()
    {
    }

    float getVolume()
    {
        return volume_;
    }

    bool getMuted()
    {
        return muted_;
    }

    // IUnknown methods -- AddRef, Release, and QueryInterface

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ulRef = InterlockedDecrement(&_cRef);
        if (0 == ulRef)
        {
            delete this;
        }
        return ulRef;
    }

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

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify);
};

static constexpr auto WASAPI = "wasapi";

class WASAPIPlayer : public Player
{
public:
    WASAPIPlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    virtual ~WASAPIPlayer();

    static std::vector<PcmDevice> pcm_list();

protected:
    virtual void worker();
    virtual bool needsThread() const override
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

#endif
