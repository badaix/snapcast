/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#include <unistd.h>
#include <assert.h>
#include <iostream>

#include "openslPlayer.h"
#include "common/log.h"
#include "common/snapException.h"
#include "common/strCompat.h"

using namespace std;

// http://stackoverflow.com/questions/35730050/android-with-nexus-6-how-to-avoid-decreased-opensl-audio-thread-priority-rela?rq=1

// source: https://github.com/hrydgard/native/blob/master/android/native-audio-so.cpp
// https://android.googlesource.com/platform/development/+/c21a505/ndk/platforms/android-9/samples/native-audio/jni/native-audio-jni.c

// This callback handler is called every time a buffer finishes playing.
// The documentation available is very unclear about how to best manage buffers.
// I've chosen to this approach: Instantly enqueue a buffer that was rendered to the last time,
// and then render the next. Hopefully it's okay to spend time in this callback after having enqueued.
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	OpenslPlayer* player = static_cast<OpenslPlayer*>(context);
	player->playerCallback(bq);
}



OpenslPlayer::OpenslPlayer(const PcmDevice& pcmDevice, Stream* stream) :
	Player(pcmDevice, stream),
	engineObject(NULL),
	engineEngine(NULL),
	outputMixObject(NULL),
	bqPlayerObject(NULL),
	bqPlayerPlay(NULL),
	bqPlayerBufferQueue(NULL),
	bqPlayerVolume(NULL),
	curBuffer(0),
	ms_(50),
	buff_size(0),
	pubStream_(stream)
{
	initOpensl();
}


OpenslPlayer::~OpenslPlayer()
{
	uninitOpensl();
}


void OpenslPlayer::playerCallback(SLAndroidSimpleBufferQueueItf bq)
{
	if (bq != bqPlayerBufferQueue)
	{
		logE << "Wrong bq!\n";
		return;
	}

/*	static long lastTick = 0;
	long now = chronos::getTickCount();
	int diff = 0;
	if (lastTick != 0)
	{
		diff = now - lastTick;
//		logE << "diff: " << diff << ", frames: " << player->frames_  / 44.1 << "\n";
//		if (diff <= 50)
//		{
//			usleep(1000 * (50 - diff));
//			diff = 50;
//		}
	}
	lastTick = chronos::getTickCount();
*/

//	size_t d = player->frames_ / 0.48d;
//	logE << "Delay: " << d << "\n";
//	SLAndroidSimpleBufferQueueState state;
//	(*bq)->GetState(bq, &state);
//	cout << "bqPlayerCallback count: " << state.count << ", idx: " << state.index << "\n";

//	diff = 0;
//	chronos::usec delay((250 - diff) * 1000);

//	while (active_ && !stream_->waitForChunk(100))
//		logO << "Waiting for chunk\n";

	if (!active_)
		return;

	chronos::usec delay(ms_ * 1000);
	if (!pubStream_->getPlayerChunk(buffer[curBuffer], delay, frames_))
	{
//		logO << "Failed to get chunk. Playing silence.\n";
		memset(buffer[curBuffer], 0, buff_size);
	}
	else
	{
		adjustVolume(buffer[curBuffer], frames_);
	}

	while (active_)
	{
		SLresult result = (*bq)->Enqueue(bq, buffer[curBuffer], buff_size);
		if (result == SL_RESULT_BUFFER_INSUFFICIENT)
			usleep(1000);
		else
			break;
	}

	curBuffer ^= 1;	// Switch buffer
}


std::string OpenslPlayer::resultToString(SLresult result) const
{
	switch(result)
	{
		case SL_RESULT_SUCCESS:
			return "SL_RESULT_SUCCESS";
		case SL_RESULT_PRECONDITIONS_VIOLATED:
			return "SL_RESULT_PRECONDITIONS_VIOLATED";
		case SL_RESULT_PARAMETER_INVALID:
			return "SL_RESULT_PARAMETER_INVALID";
		case SL_RESULT_MEMORY_FAILURE:
			return "SL_RESULT_MEMORY_FAILURE";
		case SL_RESULT_RESOURCE_ERROR:
			return "SL_RESULT_RESOURCE_ERROR";
		case SL_RESULT_RESOURCE_LOST:
			return "SL_RESULT_RESOURCE_LOST";
		case SL_RESULT_IO_ERROR:
			return "SL_RESULT_IO_ERROR";
		case SL_RESULT_BUFFER_INSUFFICIENT:
			return "SL_RESULT_BUFFER_INSUFFICIENT";
		case SL_RESULT_CONTENT_CORRUPTED:
			return "SL_RESULT_CONTENT_CORRUPTED";
		case SL_RESULT_CONTENT_UNSUPPORTED:
			return "SL_RESULT_CONTENT_UNSUPPORTED";
		case SL_RESULT_CONTENT_NOT_FOUND:
			return "SL_RESULT_CONTENT_NOT_FOUND";
		case SL_RESULT_PERMISSION_DENIED:
			return "SL_RESULT_PERMISSION_DENIED";
		case SL_RESULT_FEATURE_UNSUPPORTED:
			return "SL_RESULT_FEATURE_UNSUPPORTED";
		case SL_RESULT_INTERNAL_ERROR:
			return "SL_RESULT_INTERNAL_ERROR";
		case SL_RESULT_UNKNOWN_ERROR:
			return "SL_RESULT_UNKNOWN_ERROR";
		case SL_RESULT_OPERATION_ABORTED:
			return "SL_RESULT_OPERATION_ABORTED";
		case SL_RESULT_CONTROL_LOST:
			return "SL_RESULT_CONTROL_LOST";
		default:
			return "UNKNOWN";
	}
}



void OpenslPlayer::throwUnsuccess(const std::string& what, SLresult result)
{
	if (SL_RESULT_SUCCESS == result)
		return;
	stringstream ss;
	ss << what << " failed: " << resultToString(result) << "(" << result << ")";
	throw SnapException(ss.str());
}


void OpenslPlayer::initOpensl()
{
	if (active_)
		return;

	const SampleFormat& format = stream_->getFormat();


	frames_ = format.rate / (1000 / ms_);// * format.channels; // 1920; // 48000 * 2 / 50  // => 50ms

	buff_size = frames_ * format.frameSize /* 2 -> sample size */;
	logO << "frames: " << frames_ << ", channels: " << format.channels << ", rate: " << format.rate << ", buff: " << buff_size << "\n";

	SLresult result;
	// create engine
	SLEngineOption engineOption[] = 
	{
			{(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
	};
	result = slCreateEngine(&engineObject, 1, engineOption, 0, NULL, NULL);
	throwUnsuccess("slCreateEngine", result);
	result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
	throwUnsuccess("EngineObject::Realize", result);
	result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
	throwUnsuccess("EngineObject::GetInterface", result);
	result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, 0, 0);
	throwUnsuccess("EngineEngine::CreateOutputMix", result);
	result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
	throwUnsuccess("OutputMixObject::Realize", result);

	SLuint32 samplesPerSec = SL_SAMPLINGRATE_48;
	switch(format.rate)
	{
		case 8000:
 			samplesPerSec = SL_SAMPLINGRATE_8;
 			break;
 		case 11025:
			samplesPerSec = SL_SAMPLINGRATE_11_025;
			break;
		case 16000:
			samplesPerSec = SL_SAMPLINGRATE_16;
			break;
		case 22050:
			samplesPerSec = SL_SAMPLINGRATE_22_05;
			break;
		case 24000:
			samplesPerSec = SL_SAMPLINGRATE_24;
			break;
		case 32000:
			samplesPerSec = SL_SAMPLINGRATE_32;
			break;
		case 44100:
			samplesPerSec = SL_SAMPLINGRATE_44_1;
			break;
		case 48000:
			samplesPerSec = SL_SAMPLINGRATE_48;
			break;
		case 64000:
			samplesPerSec = SL_SAMPLINGRATE_64;
			break;
		case 88200:
			samplesPerSec = SL_SAMPLINGRATE_88_2;
			break;
		case 96000:
			samplesPerSec = SL_SAMPLINGRATE_96;
			break;
		case 192000:
			samplesPerSec = SL_SAMPLINGRATE_192;
			break;
		default:
			throw SnapException("Sample rate not supported");
	}

	SLuint32 bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	SLuint32 containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	switch(format.bits)
	{
		case 8:
			bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_8;
			containerSize = SL_PCMSAMPLEFORMAT_FIXED_8;
 			break;
		case 16:
			bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
			containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
 			break;
		case 24:
			bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_24;
			containerSize = SL_PCMSAMPLEFORMAT_FIXED_32;
 			break;
		case 32:
			bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_32;
			containerSize = SL_PCMSAMPLEFORMAT_FIXED_32;
 			break;
		default:
			throw SnapException("Unsupported sample format: "  + cpt::to_string(format.bits));
	}
	
	
	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
	SLDataFormat_PCM format_pcm =
	{
		SL_DATAFORMAT_PCM,
		format.channels,
		samplesPerSec,
		bitsPerSample,
		containerSize,
		SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
		SL_BYTEORDER_LITTLEENDIAN
	};

	SLDataSource audioSrc = {&loc_bufq, &format_pcm};

	// configure audio sink
	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
	SLDataSink audioSnk = {&loc_outmix, NULL};

	// create audio player
	const SLInterfaceID ids[3] = {SL_IID_ANDROIDCONFIGURATION, SL_IID_PLAY, SL_IID_BUFFERQUEUE};//, SL_IID_VOLUME};
	const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};//, SL_BOOLEAN_TRUE};
	result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 3, ids, req);
	throwUnsuccess("Engine::CreateAudioPlayer", result);

	SLAndroidConfigurationItf playerConfig;
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_ANDROIDCONFIGURATION, &playerConfig);
	throwUnsuccess("PlayerObject::GetInterface", result);
	SLint32 streamType = SL_ANDROID_STREAM_MEDIA;
////	SLint32 streamType = SL_ANDROID_STREAM_VOICE;
	result = (*playerConfig)->SetConfiguration(playerConfig, SL_ANDROID_KEY_STREAM_TYPE, &streamType, sizeof(SLint32));
	throwUnsuccess("PlayerConfig::SetConfiguration", result);

	result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
	throwUnsuccess("PlayerObject::Realize", result);
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
	throwUnsuccess("PlayerObject::GetInterface", result);
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
	throwUnsuccess("PlayerObject::GetInterface", result);
	result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);
	throwUnsuccess("PlayerBufferQueue::RegisterCallback", result);
//	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
//	throwUnsuccess("PlayerObject::GetInterface", result);
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PAUSED);
	throwUnsuccess("PlayerPlay::SetPlayState", result);

	// Render and enqueue a first buffer. (or should we just play the buffer empty?)
	curBuffer = 0;
	buffer[0] = new char[buff_size];
	buffer[1] = new char[buff_size];

	active_ = true;

	memset(buffer[curBuffer], 0, buff_size);
	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[curBuffer], sizeof(buffer[curBuffer]));
	throwUnsuccess("PlayerBufferQueue::Enqueue", result);
	curBuffer ^= 1;
}


void OpenslPlayer::uninitOpensl()
{
//	if (!active_)
//		return;

	logO << "uninitOpensl\n";
	SLresult result;
	logO << "OpenSLWrap_Shutdown - stopping playback\n";
	if (bqPlayerPlay != NULL)
	{
		result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
		if (SL_RESULT_SUCCESS != result)
			logE << "SetPlayState failed\n";
	}

	logO << "OpenSLWrap_Shutdown - deleting player object\n";

	if (bqPlayerObject != NULL)
	{
		(*bqPlayerObject)->Destroy(bqPlayerObject);
		bqPlayerObject = NULL;
		bqPlayerPlay = NULL;
		bqPlayerBufferQueue = NULL;
		bqPlayerVolume = NULL;
	}

	logO << "OpenSLWrap_Shutdown - deleting mix object\n";

	if (outputMixObject != NULL)
	{
		(*outputMixObject)->Destroy(outputMixObject);
		outputMixObject = NULL;
	}

	logO << "OpenSLWrap_Shutdown - deleting engine object\n";

	if (engineObject != NULL)
	{
		(*engineObject)->Destroy(engineObject);
		engineObject = NULL;
		engineEngine = NULL;
	}

	delete [] buffer[0];
	buffer[0] = NULL;

	delete [] buffer[1];
	buffer[1] = NULL;

	logO << "OpenSLWrap_Shutdown - finished\n";
	active_ = false;
}


void OpenslPlayer::start()
{
	SLresult result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
	throwUnsuccess("PlayerPlay::SetPlayState", result);
}


void OpenslPlayer::stop()
{
	SLresult result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
	throwUnsuccess("PlayerPlay::SetPlayState", result);
}


void OpenslPlayer::worker()
{
}

