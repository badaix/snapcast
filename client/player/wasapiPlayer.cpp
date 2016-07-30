#include "wasapiPlayer.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <comdef.h>
#include <comip.h>
#include <avrt.h>
#include <ksmedia.h>
#include <chrono>
#include <assert.h>
#include <locale>
#include <codecvt>
#include "common/snapException.h"

using namespace std;
using namespace std::chrono_literals;

template<typename T>
struct COMMemDeleter
{
	void operator() (T* obj)
	{
		if (obj != NULL)
		{
			CoTaskMemFree(obj);
			obj = NULL;
		}
	}
};

template<typename T>
using com_mem_ptr = unique_ptr<T, COMMemDeleter<T> >;

using com_handle = unique_ptr<void, function<BOOL(HANDLE)> >;

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_IAudioClock = __uuidof(IAudioClock);

_COM_SMARTPTR_TYPEDEF(IMMDevice,__uuidof(IMMDevice));
_COM_SMARTPTR_TYPEDEF(IMMDeviceCollection,__uuidof(IMMDeviceCollection));
_COM_SMARTPTR_TYPEDEF(IMMDeviceEnumerator,__uuidof(IMMDeviceEnumerator));
_COM_SMARTPTR_TYPEDEF(IAudioClient,__uuidof(IAudioClient));

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define CHECK_HR(hres)\
	if (FAILED(hres))\
	{\
		stringstream ss;\
		ss << "HRESULT fault status: " << hex << (hres) << " line " << __LINE__ << endl;\
		throw SnapException(ss.str());\
	}

WASAPIPlayer::WASAPIPlayer(const PcmDevice& pcmDevice, Stream* stream)
	: Player(pcmDevice, stream)
{
	HRESULT hr = CoInitializeEx(
		NULL,
		COINIT_MULTITHREADED);
	CHECK_HR(hr);
}

WASAPIPlayer::~WASAPIPlayer()
{
	WASAPIPlayer::stop();
}

vector<PcmDevice> WASAPIPlayer::pcm_list()
{
	HRESULT hr;
	IMMDeviceCollectionPtr devices = nullptr;
	IMMDeviceEnumeratorPtr deviceEnumerator = nullptr;
	
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&deviceEnumerator);
	CHECK_HR(hr);

	hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &devices);
	CHECK_HR(hr);

	UINT deviceCount;
	devices->GetCount(&deviceCount);

	if (deviceCount == 0)
		throw SnapException("no valid devices");

	vector<PcmDevice> deviceList;
	
	for (UINT i = 0; i < deviceCount; ++i)
	{
		IMMDevicePtr device = nullptr;

		hr = devices->Item(i, &device);
		CHECK_HR(hr);

		PcmDevice desc;

		com_mem_ptr<LPWSTR> id(nullptr);
		hr = device->GetId(id.get());
		CHECK_HR(hr);
		
		DWORD state;
		hr = device->GetState(&state);
		CHECK_HR(hr);
		
		desc.name = wstring_convert<codecvt_utf8<wchar_t>, wchar_t>().to_bytes(*id);
		desc.description = to_string(state);
		deviceList.push_back(desc);
	}

	return deviceList;
}

void WASAPIPlayer::worker()
{
  assert(sizeof(char) == sizeof(BYTE));
	
	HRESULT hr;

	// Create the format specifier
	com_mem_ptr<WAVEFORMATEX> waveformat((WAVEFORMATEX*)(CoTaskMemAlloc(sizeof(WAVEFORMATEX))));
	waveformat->wFormatTag      = WAVE_FORMAT_PCM;
	waveformat->nChannels       = stream_->getFormat().channels;
	waveformat->nSamplesPerSec  = stream_->getFormat().rate;
	waveformat->wBitsPerSample  = stream_->getFormat().bits;

	waveformat->nBlockAlign     = waveformat->nChannels * waveformat->wBitsPerSample / 8;
	waveformat->nAvgBytesPerSec = waveformat->nSamplesPerSec * waveformat->nBlockAlign;

	waveformat->cbSize          = 0;

	com_mem_ptr<WAVEFORMATEXTENSIBLE> waveformatExtended((WAVEFORMATEXTENSIBLE*)(CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE))));
	waveformatExtended->Format                      = *waveformat;
	waveformatExtended->Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
	waveformatExtended->Format.cbSize               = 22;
	waveformatExtended->Samples.wValidBitsPerSample = waveformat->wBitsPerSample;
	waveformatExtended->dwChannelMask               = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	waveformatExtended->SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;

	// Retrieve the device enumerator
	IMMDeviceEnumeratorPtr deviceEnumerator = nullptr;
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&deviceEnumerator);
	CHECK_HR(hr);

	// Register the default playback device (eRender for playback)
	IMMDevicePtr device = nullptr;
	hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	CHECK_HR(hr);

	// Activate the device
	IAudioClientPtr audioClient = nullptr;
	hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audioClient);
	CHECK_HR(hr);

	hr = audioClient->IsFormatSupported(
		AUDCLNT_SHAREMODE_EXCLUSIVE,
		&(waveformatExtended->Format),
		NULL);
	CHECK_HR(hr);
	
	// Get the device period
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	hr = audioClient->GetDevicePeriod(NULL, &hnsRequestedDuration);
	CHECK_HR(hr);
	
	// Initialize the client at minimum latency
	hr = audioClient->Initialize(
		AUDCLNT_SHAREMODE_EXCLUSIVE,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		hnsRequestedDuration,
		hnsRequestedDuration,
		&(waveformatExtended->Format),
		NULL);
	if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
	{
		UINT32 alignedBufferSize;
		hr = audioClient->GetBufferSize(&alignedBufferSize);
		CHECK_HR(hr);
		hr = audioClient->Release();
		CHECK_HR(hr);
		hnsRequestedDuration = (REFERENCE_TIME)((10000.0 * 1000 / waveformatExtended->Format.nSamplesPerSec * alignedBufferSize) + 0.5);
		hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audioClient);
		CHECK_HR(hr);
		hr = audioClient->Initialize(
			AUDCLNT_SHAREMODE_EXCLUSIVE,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			hnsRequestedDuration,
			hnsRequestedDuration,
			&(waveformatExtended->Format),
			NULL);
	}
	CHECK_HR(hr);

	// Register an event to refill the buffer
	com_handle eventHandle(CreateEvent(NULL, FALSE, FALSE, NULL), &::CloseHandle);
	if (eventHandle == NULL)
	{
		CHECK_HR(E_FAIL);
	}
	hr = audioClient->SetEventHandle(HANDLE(eventHandle.get()));
	CHECK_HR(hr);

	// Get size of buffer
	UINT32 bufferFrameCount;
	hr = audioClient->GetBufferSize(&bufferFrameCount);
	CHECK_HR(hr);

	// Get the rendering service
	IAudioRenderClient* renderClient = NULL;
	hr = audioClient->GetService(
		IID_IAudioRenderClient,
		(void**)&renderClient);
	CHECK_HR(hr);

	// Grab the clock service
	IAudioClock* clock = NULL;
	hr = audioClient->GetService(
		IID_IAudioClock,
		(void**)&clock);
	CHECK_HR(hr);

	// Boost our priority
	DWORD taskIndex = 0;
	com_handle taskHandle(AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex),
		&::AvRevertMmThreadCharacteristics);
	if (taskHandle == NULL)
	{
		CHECK_HR(E_FAIL);
	}

	// And, action!
	hr = audioClient->Start();
	CHECK_HR(hr);
	
	size_t bufferSize = bufferFrameCount * waveformatExtended->Format.nBlockAlign;
	BYTE* buffer;
	char queueBuffer[bufferSize];
	UINT64 position = 0, bufferPosition = 0, frequency;
	clock->GetFrequency(&frequency);
	
	while (active_)
	{
	  DWORD returnVal = WaitForSingleObject(eventHandle.get(), 2000);
		if (returnVal != WAIT_OBJECT_0)
		{
			stop();
			CHECK_HR(ERROR_TIMEOUT);
		}

		// Thread was sleeping above, double check that we are still running
		if (!active_)
			break;

		clock->GetPosition(&position, NULL);

		if (stream_->getPlayerChunk(queueBuffer, std::chrono::microseconds(
	                                                                     ((bufferPosition * 1000000) / waveformat->nSamplesPerSec) -
																																			 ((position * 1000000) / frequency)),
		                            bufferFrameCount))
		{
			adjustVolume(queueBuffer, bufferFrameCount);
			hr = renderClient->GetBuffer(bufferFrameCount, &buffer);
			CHECK_HR(hr);
			memcpy(buffer, queueBuffer, bufferSize);
			hr = renderClient->ReleaseBuffer(bufferFrameCount, 0);
			CHECK_HR(hr);
			
			bufferPosition += bufferFrameCount;
		}
		else
		{
			logO << "Failed to get chunk\n";
			
			hr = audioClient->Stop();
			CHECK_HR(hr);
			hr = audioClient->Reset();
			CHECK_HR(hr);
			
			while (active_ && !stream_->waitForChunk(100))
				logD << "Waiting for chunk\n";
			
			hr = audioClient->Start();
			CHECK_HR(hr);
			bufferPosition = 0;
		}
	}
}
