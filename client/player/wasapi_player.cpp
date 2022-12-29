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

// prototype/interface header file
#include "wasapi_player.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"

// 3rd party headers
#include <initguid.h>
#include <mmdeviceapi.h>
//#include <functiondiscoverykeys_devpkey.h>

// standard headers
#include <audioclient.h>
#include <avrt.h>
#include <comdef.h>
#include <comip.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>

#include <cassert>
#include <chrono>
#include <codecvt>
#include <functional>
#include <locale>

using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace player
{

static constexpr auto LOG_TAG = "WASAPI";

template <typename T>
struct COMMemDeleter
{
    void operator()(T* obj)
    {
        if (obj != NULL)
        {
            CoTaskMemFree(obj);
            obj = NULL;
        }
    }
};

template <typename T>
using com_mem_ptr = unique_ptr<T, COMMemDeleter<T>>;

using com_handle = unique_ptr<void, function<BOOL(HANDLE)>>;

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_IAudioClock = __uuidof(IAudioClock);
const IID IID_IAudioEndpointVolume = _uuidof(IAudioEndpointVolume);

_COM_SMARTPTR_TYPEDEF(IMMDevice, __uuidof(IMMDevice));
_COM_SMARTPTR_TYPEDEF(IMMDeviceCollection, __uuidof(IMMDeviceCollection));
_COM_SMARTPTR_TYPEDEF(IMMDeviceEnumerator, __uuidof(IMMDeviceEnumerator));
_COM_SMARTPTR_TYPEDEF(IAudioClient, __uuidof(IAudioClient));
_COM_SMARTPTR_TYPEDEF(IPropertyStore, __uuidof(IPropertyStore));
_COM_SMARTPTR_TYPEDEF(IAudioSessionManager, __uuidof(IAudioSessionManager));
_COM_SMARTPTR_TYPEDEF(IAudioSessionControl, __uuidof(IAudioSessionControl));

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

EXTERN_C const PROPERTYKEY DECLSPEC_SELECTANY PKEY_Device_FriendlyName = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};

#define CHECK_HR(hres)                                                                                                                                         \
    if (FAILED(hres))                                                                                                                                          \
    {                                                                                                                                                          \
        stringstream ss;                                                                                                                                       \
        ss << "HRESULT fault status: " << hex << (hres) << " line " << dec << __LINE__ << endl;                                                                \
        LOG(FATAL, LOG_TAG) << ss.str();                                                                                                                       \
        throw SnapException(ss.str());                                                                                                                         \
    }

WASAPIPlayer::WASAPIPlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, stream)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CHECK_HR(hr);

    audioEventListener_ = new AudioSessionEventListener();
}

WASAPIPlayer::~WASAPIPlayer()
{
    audioEndpointListener_->UnregisterControlChangeNotify(&audioEndpointVolumeCallback_);
    WASAPIPlayer::stop();
}

inline PcmDevice convertToDevice(int idx, IMMDevicePtr& device)
{
    HRESULT hr;
    PcmDevice desc;

    LPWSTR id = NULL;
    hr = device->GetId(&id);
    CHECK_HR(hr);

    IPropertyStorePtr properties = nullptr;
    hr = device->OpenPropertyStore(STGM_READ, &properties);

    PROPVARIANT deviceName;
    PropVariantInit(&deviceName);

    hr = properties->GetValue(PKEY_Device_FriendlyName, &deviceName);
    CHECK_HR(hr);

    desc.idx = idx;

    // Convert a wide Unicode string to an UTF8 string
    auto utf8_encode = [](const std::wstring& wstr)
    {
        if (wstr.empty())
            return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    };

    desc.name = utf8_encode(id);
    desc.description = utf8_encode(deviceName.pwszVal);

    CoTaskMemFree(id);

    return desc;
}

vector<PcmDevice> WASAPIPlayer::pcm_list()
{
    HRESULT hr;
    IMMDeviceCollectionPtr devices = nullptr;
    IMMDeviceEnumeratorPtr deviceEnumerator = nullptr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hr != CO_E_ALREADYINITIALIZED)
        CHECK_HR(hr);

    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_SERVER, IID_IMMDeviceEnumerator, (void**)&deviceEnumerator);
    CHECK_HR(hr);

    hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);
    CHECK_HR(hr);

    UINT deviceCount;
    devices->GetCount(&deviceCount);

    if (deviceCount == 0)
        throw SnapException("no valid devices");

    vector<PcmDevice> deviceList;

    {
        IMMDevicePtr defaultDevice = nullptr;
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
        CHECK_HR(hr);

        auto dev = convertToDevice(0, defaultDevice);
        dev.name = DEFAULT_DEVICE;
        deviceList.push_back(dev);
    }

    for (UINT i = 0; i < deviceCount; ++i)
    {
        IMMDevicePtr device = nullptr;

        hr = devices->Item(i, &device);
        CHECK_HR(hr);
        deviceList.push_back(convertToDevice(i + 1, device));
    }

    return deviceList;
}

#pragma warning(push)
#pragma warning(disable : 4127)
void WASAPIPlayer::worker()
{
    assert(sizeof(char) == sizeof(BYTE));

    HRESULT hr;

    // Create the format specifier
    com_mem_ptr<WAVEFORMATEX> waveformat((WAVEFORMATEX*)(CoTaskMemAlloc(sizeof(WAVEFORMATEX))));
    waveformat->wFormatTag = WAVE_FORMAT_PCM;
    waveformat->nChannels = stream_->getFormat().channels();
    waveformat->nSamplesPerSec = stream_->getFormat().rate();
    waveformat->wBitsPerSample = stream_->getFormat().bits();

    waveformat->nBlockAlign = waveformat->nChannels * waveformat->wBitsPerSample / 8;
    waveformat->nAvgBytesPerSec = waveformat->nSamplesPerSec * waveformat->nBlockAlign;

    waveformat->cbSize = 0;

    com_mem_ptr<WAVEFORMATEXTENSIBLE> waveformatExtended((WAVEFORMATEXTENSIBLE*)(CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE))));
    waveformatExtended->Format = *waveformat;
    waveformatExtended->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    waveformatExtended->Format.cbSize = 22;
    waveformatExtended->Samples.wValidBitsPerSample = waveformat->wBitsPerSample;
    waveformatExtended->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    waveformatExtended->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;


    // Retrieve the device enumerator
    IMMDeviceEnumeratorPtr deviceEnumerator = nullptr;
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_SERVER, IID_IMMDeviceEnumerator, (void**)&deviceEnumerator);
    CHECK_HR(hr);

    // Register the default playback device (eRender for playback)
    IMMDevicePtr device = nullptr;
    if (settings_.pcm_device.idx == 0)
    {
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        CHECK_HR(hr);
    }
    else
    {
        IMMDeviceCollectionPtr devices = nullptr;
        hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);
        CHECK_HR(hr);

        devices->Item(settings_.pcm_device.idx - 1, &device);
    }

    IPropertyStorePtr properties = nullptr;
    hr = device->OpenPropertyStore(STGM_READ, &properties);
    CHECK_HR(hr);

    PROPVARIANT format;
    hr = properties->GetValue(PKEY_AudioEngine_DeviceFormat, &format);
    CHECK_HR(hr);

    PWAVEFORMATEX formatEx = (PWAVEFORMATEX)format.blob.pBlobData;
    LOG(INFO, LOG_TAG) << "Device accepts format: " << formatEx->nSamplesPerSec << ":" << formatEx->wBitsPerSample << ":" << formatEx->nChannels << "\n";
    // Activate the device
    IAudioClientPtr audioClient = nullptr;
    hr = device->Activate(IID_IAudioClient, CLSCTX_SERVER, NULL, (void**)&audioClient);
    CHECK_HR(hr);

    if (settings_.sharing_mode == ClientSettings::SharingMode::exclusive)
    {
        hr = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &(waveformatExtended->Format), NULL);
        CHECK_HR(hr);
    }

    IAudioSessionManagerPtr sessionManager = nullptr;
    // Get the session manager for the endpoint device.
    hr = device->Activate(__uuidof(IAudioSessionManager), CLSCTX_INPROC_SERVER, NULL, (void**)&sessionManager);
    CHECK_HR(hr);


    // Get the control interface for the process-specific audio
    // session with session GUID = GUID_NULL. This is the session
    // that an audio stream for a DirectSound, DirectShow, waveOut,
    // or PlaySound application stream belongs to by default.
    IAudioSessionControlPtr control = nullptr;
    hr = sessionManager->GetAudioSessionControl(NULL, 0, &control);
    CHECK_HR(hr);

    // register
    hr = control->RegisterAudioSessionNotification(audioEventListener_);
    CHECK_HR(hr);

    hr = device->Activate(IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&audioEndpointListener_);

    audioEndpointListener_->RegisterControlChangeNotify((IAudioEndpointVolumeCallback*)&audioEndpointVolumeCallback_);

    // Get the device period
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    hr = audioClient->GetDevicePeriod(NULL, &hnsRequestedDuration);
    CHECK_HR(hr);

    LOG(INFO, LOG_TAG) << "Initializing WASAPI in " << (settings_.sharing_mode == ClientSettings::SharingMode::shared ? "shared" : "exclusive") << " mode\n";

    _AUDCLNT_SHAREMODE share_mode = settings_.sharing_mode == ClientSettings::SharingMode::shared ? AUDCLNT_SHAREMODE_SHARED : AUDCLNT_SHAREMODE_EXCLUSIVE;
    DWORD stream_flags = settings_.sharing_mode == ClientSettings::SharingMode::shared
                             ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
                             : AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

    // Initialize the client at minimum latency
    hr = audioClient->Initialize(share_mode, stream_flags, hnsRequestedDuration, hnsRequestedDuration, &(waveformatExtended->Format), NULL);

    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
        UINT32 alignedBufferSize;
        hr = audioClient->GetBufferSize(&alignedBufferSize);
        CHECK_HR(hr);
        audioClient.Attach(NULL, false);
        hnsRequestedDuration = (REFERENCE_TIME)((10000.0 * 1000 / waveformatExtended->Format.nSamplesPerSec * alignedBufferSize) + 0.5);
        hr = device->Activate(IID_IAudioClient, CLSCTX_SERVER, NULL, (void**)&audioClient);
        CHECK_HR(hr);
        hr = audioClient->Initialize(share_mode, stream_flags, hnsRequestedDuration, hnsRequestedDuration, &(waveformatExtended->Format), NULL);
    }
    CHECK_HR(hr);

    // Register an event to refill the buffer
    com_handle eventHandle(CreateEvent(NULL, FALSE, FALSE, NULL), &::CloseHandle);
    if (eventHandle == NULL)
        CHECK_HR(E_FAIL);
    hr = audioClient->SetEventHandle(HANDLE(eventHandle.get()));
    CHECK_HR(hr);

    // Get size of buffer
    UINT32 bufferFrameCount;
    hr = audioClient->GetBufferSize(&bufferFrameCount);
    CHECK_HR(hr);

    // Get the rendering service
    IAudioRenderClient* renderClient = NULL;
    hr = audioClient->GetService(IID_IAudioRenderClient, (void**)&renderClient);
    CHECK_HR(hr);

    // Grab the clock service
    IAudioClock* clock = NULL;
    hr = audioClient->GetService(IID_IAudioClock, (void**)&clock);
    CHECK_HR(hr);

    // Boost our priority
    DWORD taskIndex = 0;
    com_handle taskHandle(AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex), &::AvRevertMmThreadCharacteristics);
    if (taskHandle == NULL)
        CHECK_HR(E_FAIL);

    // And, action!
    hr = audioClient->Start();
    CHECK_HR(hr);

    size_t bufferSize = bufferFrameCount * waveformatExtended->Format.nBlockAlign;
    BYTE* buffer;
    unique_ptr<char[]> queueBuffer(new char[bufferSize]);
    UINT64 position = 0, bufferPosition = 0, frequency;
    clock->GetFrequency(&frequency);

    while (active_)
    {
        DWORD returnVal = WaitForSingleObject(eventHandle.get(), 2000);
        if (returnVal != WAIT_OBJECT_0)
        {
            // stop();
            LOG(INFO, LOG_TAG) << "Got timeout waiting for audio device callback\n";
            CHECK_HR(ERROR_TIMEOUT);

            hr = audioClient->Stop();
            CHECK_HR(hr);
            hr = audioClient->Reset();
            CHECK_HR(hr);

            while (active_ && !stream_->waitForChunk(std::chrono::milliseconds(100)))
                LOG(INFO, LOG_TAG) << "Waiting for chunk\n";

            hr = audioClient->Start();
            CHECK_HR(hr);
            bufferPosition = 0;
            break;
        }

        // Thread was sleeping above, double check that we are still running
        if (!active_)
            break;

        // update our volume from IAudioControl
        if (mode_ == ClientSettings::SharingMode::exclusive)
        {
            volCorrection_ = audioEventListener_->getVolume();
            // muteOverride = audioEventListener_->getMuted(); // use this for also applying audio mixer mute state
        }

        // get audio device volume from IAudioEndpointVolume
        // float deviceVolume = audioEndpointVolumeCallback.getVolume(); // system volume (for this audio device)
        // bool deviceMuted = audioEndpointVolumeCallback.getMuted(); // system mute (for this audio device)

        clock->GetPosition(&position, NULL);

        UINT32 padding = 0;
        if (settings_.sharing_mode == ClientSettings::SharingMode::shared)
        {
            hr = audioClient->GetCurrentPadding(&padding);
            CHECK_HR(hr);
        }

        int available = bufferFrameCount - padding;

        if (stream_->getPlayerChunk(queueBuffer.get(),
                                    microseconds(((bufferPosition * 1000000) / waveformat->nSamplesPerSec) - ((position * 1000000) / frequency)), available))
        {
            if (available > 0)
            {
                adjustVolume(queueBuffer.get(), available);
                hr = renderClient->GetBuffer(available, &buffer);
                CHECK_HR(hr);
                memcpy(buffer, queueBuffer.get(), bufferSize);
                hr = renderClient->ReleaseBuffer(available, 0);
                CHECK_HR(hr);

                bufferPosition += available;
            }
        }
        else
        {
            LOG(INFO, LOG_TAG) << "Failed to get chunk\n";

            hr = audioClient->Stop();
            CHECK_HR(hr);
            hr = audioClient->Reset();
            CHECK_HR(hr);

            while (active_ && !stream_->waitForChunk(std::chrono::milliseconds(100)))
                LOG(INFO, LOG_TAG) << "Waiting for chunk\n";

            hr = audioClient->Start();
            CHECK_HR(hr);
            bufferPosition = 0;
        }
    }
}
#pragma warning(pop)

HRESULT STDMETHODCALLTYPE AudioSessionEventListener::QueryInterface(REFIID riid, VOID** ppvInterface)
{
    if (IID_IUnknown == riid)
    {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    }
    else if (__uuidof(IAudioSessionEvents) == riid)
    {
        AddRef();
        *ppvInterface = (IAudioSessionEvents*)this;
    }
    else
    {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AudioSessionEventListener::OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext)
{
    std::ignore = EventContext;
    volume_ = NewVolume;
    muted_ = NewMute;

    if (NewMute)
    {
        LOG(DEBUG, LOG_TAG) << ("MUTE\n");
    }
    else
    {
        LOG(DEBUG, LOG_TAG) << "Volume = " << (UINT32)(100 * NewVolume + 0.5) << " percent\n";
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE AudioSessionEventListener::OnStateChanged(AudioSessionState NewState)
{
    char* pszState = "?????";

    switch (NewState)
    {
        case AudioSessionStateActive:
            pszState = "active";
            break;
        case AudioSessionStateInactive:
            pszState = "inactive";
            break;
    }
    LOG(DEBUG, LOG_TAG) << "New session state = " << pszState << "\n";

    return S_OK;
}

HRESULT STDMETHODCALLTYPE AudioSessionEventListener::OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason)
{
    char* pszReason = "?????";

    switch (DisconnectReason)
    {
        case DisconnectReasonDeviceRemoval:
            pszReason = "device removed";
            break;
        case DisconnectReasonServerShutdown:
            pszReason = "server shut down";
            break;
        case DisconnectReasonFormatChanged:
            pszReason = "format changed";
            break;
        case DisconnectReasonSessionLogoff:
            pszReason = "user logged off";
            break;
        case DisconnectReasonSessionDisconnected:
            pszReason = "session disconnected";
            break;
        case DisconnectReasonExclusiveModeOverride:
            pszReason = "exclusive-mode override";
            break;
    }
    LOG(INFO, LOG_TAG) << "Audio session disconnected (reason: " << pszReason << ")";

    return S_OK;
}



HRESULT STDMETHODCALLTYPE AudioEndpointVolumeCallback::OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify)
{
    if (pNotify == NULL)
    {
        return E_INVALIDARG;
    }

    if (pNotify->bMuted)
    {
        LOG(DEBUG, LOG_TAG) << ("MASTER MUTE\n");
    }

    LOG(DEBUG, LOG_TAG) << "Volume = " << (UINT32)(100 * pNotify->fMasterVolume + 0.5) << " percent\n";

    volume_ = pNotify->fMasterVolume;
    muted_ = pNotify->bMuted;

    return S_OK;
}

} // namespace player
