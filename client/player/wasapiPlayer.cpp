#include "wasapiPlayer.h"

#include <avrt.h>
#include <ksmedia.h>
#include <chrono>

using namespace std;
using namespace std::chrono_literals;

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_IAudioClock = __uuidof(IAudioClock);

WASAPIPlayer::WASAPIPlayer(const PcmDevice& pcmDevice, Stream* stream)
	: Player(pcmDevice, stream)
{
}

WASAPIPlayer::~WASAPIPlayer()
{
	stop();
}

void WASAPIPlayer::start()
{
	initWasapi();
	Player::start();
}

void WASAPIPlayer::stop()
{
	Player::stop();
	uninitWasapi();
}

#define CHECK_HR(hres) if (FAILED(hres)) { cout << "HRESULT fault status: " << hres << " line " << __LINE__ << endl; uninitWasapi(); return; }
#define SAFE_FREE(obj) if ((obj) != NULL) { (obj)->Release(); (obj) = NULL; }

void WASAPIPlayer::initWasapi()
{
	HRESULT hr;

	// Initialize COM
	hr = CoInitializeEx(
		NULL,
		COINIT_MULTITHREADED);
	CHECK_HR(hr);

	// Create the format specifier
	waveformat = (WAVEFORMATEX*)(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
	waveformat->wFormatTag      = WAVE_FORMAT_PCM;
	waveformat->nChannels       = stream_->getFormat().channels;
	waveformat->nSamplesPerSec  = stream_->getFormat().rate;
	waveformat->wBitsPerSample  = stream_->getFormat().bits;

	waveformat->nBlockAlign     = waveformat->nChannels * waveformat->wBitsPerSample / 8;
	waveformat->nAvgBytesPerSec = waveformat->nSamplesPerSec * waveformat->nBlockAlign;

	waveformat->cbSize          = 0;

	waveformatExtended = (WAVEFORMATEXTENSIBLE*)(CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE)));
	waveformatExtended->Format                      = *waveformat;
	waveformatExtended->Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
	waveformatExtended->Format.cbSize               = 22;
	waveformatExtended->Samples.wValidBitsPerSample = waveformat->wBitsPerSample;
	waveformatExtended->dwChannelMask               = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	waveformatExtended->SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;

	cout
		<< "wave format correct: " << to_string(waveformat->wFormatTag == WAVE_FORMAT_PCM)
		<< "wave format correct: " << to_string(waveformat->wFormatTag)
		<< "\r\nchannels: " << to_string(waveformat->nChannels)
		<< "\r\nsample rate: " << to_string(waveformat->nSamplesPerSec)
		<< "\r\naverage data-transfer rate correct: " << to_string(waveformat->nAvgBytesPerSec == (waveformat->nSamplesPerSec * waveformat->nBlockAlign))
		<< "\r\nblock alignment correct: " << to_string(waveformat->nBlockAlign == (waveformat->nChannels * waveformat->wBitsPerSample) / 8)
		<< "\r\nbits per sample: " << to_string(waveformat->wBitsPerSample)
		<< "\r\nbits per sample valid: " << to_string(waveformat->wBitsPerSample == 8 || waveformat->wBitsPerSample == 16) << endl;

	// Retrieve the device enumerator
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&deviceEnumerator);
	CHECK_HR(hr);

	// Register the default playback device (eRender for playback)
	hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	CHECK_HR(hr);

	// Activate the device
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
	eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (eventHandle == NULL)
	{
		CHECK_HR(E_FAIL);
	}
	hr = audioClient->SetEventHandle(eventHandle);
	CHECK_HR(hr);

	// Get size of buffer
	hr = audioClient->GetBufferSize(&bufferFrameCount);
	CHECK_HR(hr);

	// Get the rendering service
	hr = audioClient->GetService(
		IID_IAudioRenderClient,
		(void**)&renderClient);
	CHECK_HR(hr);

	// Grab the clock service
	hr = audioClient->GetService(
		IID_IAudioClock,
		(void**)&clock);
	CHECK_HR(hr);

	// Boost our priority
	DWORD taskIndex = 0;
	taskHandle = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
	if (taskHandle == NULL)
	{
		CHECK_HR(E_FAIL);
	}

	// And, action!
	hr = audioClient->Start();
	CHECK_HR(hr);

	wasapiActive = true;
}

void WASAPIPlayer::uninitWasapi()
{
	if (eventHandle != NULL)
	{
		CloseHandle(eventHandle);
	}
	if (taskHandle != NULL)
	{
		AvRevertMmThreadCharacteristics(taskHandle);
	}
	CoTaskMemFree(waveformat);
	CoTaskMemFree(waveformatExtended);
	SAFE_FREE(deviceEnumerator);
	SAFE_FREE(device);
	SAFE_FREE(audioClient);
	SAFE_FREE(renderClient);
	SAFE_FREE(clock);
	
	wasapiActive = false;
}

void WASAPIPlayer::worker()
{
	size_t bufferSize = bufferFrameCount * waveformatExtended->Format.nBlockAlign;
//	BYTE* bufferTemp = (BYTE*)malloc(bufferSize);
	BYTE* buffer;
	HRESULT hr;
	while (active_)
	{
		if (!wasapiActive)
		{
			try
			{
				initWasapi();
			}
			catch (const std::exception& e)
			{
				logE << "Exception in initWasapi: " << e.what() << endl;
			}
		}

		
		DWORD returnVal = WaitForSingleObject(eventHandle, 5000);
		if (returnVal != WAIT_OBJECT_0)
		{
			audioClient->Stop();
			CHECK_HR(ERROR_TIMEOUT);
		}
		
		hr = renderClient->GetBuffer(bufferFrameCount, &buffer);
		CHECK_HR(hr);

		stream_->getPlayerChunk(buffer, 999000us, bufferFrameCount);

		hr = renderClient->ReleaseBuffer(bufferFrameCount, 0);
		CHECK_HR(hr);
	}
}
