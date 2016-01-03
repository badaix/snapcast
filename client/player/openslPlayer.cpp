/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

	chronos::usec delay(150 * 1000);
	if (!pubStream_->getPlayerChunk(buffer[curBuffer], delay, frames_))
	{
		logO << "Failed to get chunk. Playing silence.\n";
		memset(buffer[curBuffer], 0, buff_size);
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
	unsigned int rate;
	int channels;
//	int buff_size;

	const msg::SampleFormat& format = stream_->getFormat();
	rate = format.rate;
	channels = format.channels;

	frames_ = rate / 20; // => 50ms

	buff_size = frames_ * channels * 2 /* 2 -> sample size */;
	logO << "frames: " << frames_ << ", channels: " << channels << ", rate: " << rate << ", buff: " << buff_size << "\n";

////	audioCallback = cb;
	framesPerBuffer = frames_;//_FramesPerBuffer;
	if (framesPerBuffer == 0)
		framesPerBuffer = 256;
	if (framesPerBuffer < 32)
		framesPerBuffer = 32;
	sampleRate = rate;
	if (sampleRate != 44100 && sampleRate != 48000)
	{
		logE << "Invalid sample rate " << sampleRate << " - choosing 44100\n";
		sampleRate = 44100;
	}

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
	if (sampleRate == 48000)
	{
		sr = SL_SAMPLINGRATE_48;
	}

	logO << "SamplingRate: " << sr/1000 << "\n";

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

	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[curBuffer], sizeof(buffer[curBuffer]));
	if (SL_RESULT_SUCCESS != result)
		throw SnapException("SL_RESULT_SUCCESS != result");

	curBuffer ^= 1;
}


void OpenslPlayer::uninitOpensl()
{
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
}


void OpenslPlayer::start()
{
	active_ = true;
	initOpensl();
}


void OpenslPlayer::stop()
{
	active_ = false;
	uninitOpensl();
}




void OpenslPlayer::worker()
{
}

