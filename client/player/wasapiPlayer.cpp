#include "wasapiPlayer.h"

#include <audioclient.h>
#include <mmdeviceapi.h>

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

using namespace std;

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

void WASAPIPlayer::initWasapi()
{
	HRESULT hr;

	// Initialize COM
	hr = CoInitializeEx(
		NULL,
		COINIT_MULTITHREADED);
	CHECK_HR(hr);

	// Retrieve the device enumerator
	IMMDeviceEnumerator* deviceEnumerator = NULL;
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&deviceEnumerator);
	CHECK_HR(hr);

	// Register the default playback device (eRender for playback)
	IMMDevice* device = NULL;
	hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	CHECK_HR(hr);

	// Activate the device
	IAudioClient* audioClient = NULL;
	hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audioClient);
	CHECK_HR(hr);
}

void WASAPIPlayer::uninitWasapi()
{
}

void WASAPIPlayer::worker()
{
	initWasapi();
}
