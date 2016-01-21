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

using namespace std;

// source: https://github.com/hrydgard/native/blob/master/android/native-audio-so.cpp


// This callback handler is called every time a buffer finishes playing.
// The documentation available is very unclear about how to best manage buffers.
// I've chosen to this approach: Instantly enqueue a buffer that was rendered to the last time,
// and then render the next. Hopefully it's okay to spend time in this callback after having enqueued.
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	OpenslPlayer* player = (OpenslPlayer*)context;
	player->playerCallback(bq);
}



OpenslPlayer::OpenslPlayer(const PcmDevice& pcmDevice, Stream* stream) : Player(pcmDevice, stream), pubStream_(stream), bqPlayerObject(NULL), curBuffer(0)
{
}


OpenslPlayer::~OpenslPlayer()
{
	stop();
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

	chronos::usec delay(50 * 1000);
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


void OpenslPlayer::initOpensl()
{
	if (active_)
		return;

	const msg::SampleFormat& format = stream_->getFormat();

	frames_ = 960;//rate / 50; // => 50ms

	buff_size = frames_ * format.channels * 2 /* 2 -> sample size */;
	logO << "frames: " << frames_ << ", channels: " << format.channels << ", rate: " << format.rate << ", buff: " << buff_size << "\n";

	buffer[0] = new char[buff_size];
	buffer[1] = new char[buff_size];

	SLresult result;
	// create engine
	result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
	assert(SL_RESULT_SUCCESS == result);
	result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);
	result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
	assert(SL_RESULT_SUCCESS == result);
	result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, 0, 0);
	assert(SL_RESULT_SUCCESS == result);
	result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);

	SLuint32 sr = SL_SAMPLINGRATE_44_1;
	switch(format.rate)
	{
		case 8000:
 			sr = SL_SAMPLINGRATE_8;
 			break;
 		case 11025:
			sr = SL_SAMPLINGRATE_11_025;
			break;
		case 16000:
			sr = SL_SAMPLINGRATE_16;
			break;
		case 22050:
			sr = SL_SAMPLINGRATE_22_05;
			break;
		case 24000:
			sr = SL_SAMPLINGRATE_24;
			break;
		case 32000:
			sr = SL_SAMPLINGRATE_32;
			break;
		case 44100:
			sr = SL_SAMPLINGRATE_44_1;
			break;
		case 48000:
			sr = SL_SAMPLINGRATE_48;
			break;
		case 64000:
			sr = SL_SAMPLINGRATE_64;
			break;
		case 88200:
			sr = SL_SAMPLINGRATE_88_2;
			break;
		case 96000:
			sr = SL_SAMPLINGRATE_96;
			break;
		case 192000:
			sr = SL_SAMPLINGRATE_192;
			break;
		default:
			throw SnapException("Sample rate not supported");
	}

	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
	SLDataFormat_PCM format_pcm =
	{
		SL_DATAFORMAT_PCM,
		2,
		sr,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
		SL_BYTEORDER_LITTLEENDIAN
	};

	SLDataSource audioSrc = {&loc_bufq, &format_pcm};

	// configure audio sink
	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
	SLDataSink audioSnk = {&loc_outmix, NULL};

	// create audio player
	const SLInterfaceID ids[4] = {SL_IID_ANDROIDCONFIGURATION, SL_IID_PLAY, SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
	const SLboolean req[4] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
	result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 4, ids, req);
	assert(SL_RESULT_SUCCESS == result);

	SLAndroidConfigurationItf playerConfig;
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_ANDROIDCONFIGURATION, &playerConfig);
	SLint32 streamType = SL_ANDROID_STREAM_MEDIA;
//	SLint32 streamType = SL_ANDROID_STREAM_VOICE;
	result = (*playerConfig)->SetConfiguration(playerConfig, SL_ANDROID_KEY_STREAM_TYPE, &streamType, sizeof(SLint32));

	result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
	assert(SL_RESULT_SUCCESS == result);
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
	assert(SL_RESULT_SUCCESS == result);
	result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);
	assert(SL_RESULT_SUCCESS == result);
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
	assert(SL_RESULT_SUCCESS == result);
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
	assert(SL_RESULT_SUCCESS == result);

	// Render and enqueue a first buffer. (or should we just play the buffer empty?)
	curBuffer = 0;
////	audioCallback(buffer[curBuffer], framesPerBuffer);
	active_ = true;

	memset(buffer[curBuffer], 0, buff_size);
	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[curBuffer], sizeof(buffer[curBuffer]));
	if (SL_RESULT_SUCCESS != result)
		throw SnapException("SL_RESULT_SUCCESS != result");

	curBuffer ^= 1;
}


void OpenslPlayer::uninitOpensl()
{
	if (!active_)
		return;

	logE << "uninitOpensl\n";
	SLresult result;
	logO << "OpenSLWrap_Shutdown - stopping playback\n";
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
	if (SL_RESULT_SUCCESS != result)
	{
		logE << "SetPlayState failed\n";
	}

	logO << "OpenSLWrap_Shutdown - deleting player object\n";

	if (bqPlayerObject != NULL)
	{
		(*bqPlayerObject)->Destroy(bqPlayerObject);
		bqPlayerObject = NULL;
		bqPlayerPlay = NULL;
		bqPlayerBufferQueue = NULL;
		bqPlayerMuteSolo = NULL;
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
	delete [] buffer[1];
	logO << "OpenSLWrap_Shutdown - finished\n";
	active_ = false;
}


void OpenslPlayer::start()
{
	initOpensl();
}


void OpenslPlayer::stop()
{
	uninitOpensl();
}




void OpenslPlayer::worker()
{
}

