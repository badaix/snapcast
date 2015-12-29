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

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "openslPlayer.h"
#include "common/log.h"
#include "common/snapException.h"

using namespace std;


// This is kinda ugly, but for simplicity I've left these as globals just like in the sample,
// as there's not really any use case for this where we have multiple audio devices yet.

// engine interfaces
static SLObjectItf engineObject;
static SLEngineItf engineEngine;
static SLObjectItf outputMixObject;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
static SLMuteSoloItf bqPlayerMuteSolo;
static SLVolumeItf bqPlayerVolume;

// Double buffering.
static char *buffer[2];
static int curBuffer = 0;
static int framesPerBuffer;
int sampleRate;

static AndroidAudioCallback audioCallback;

// This callback handler is called every time a buffer finishes playing.
// The documentation available is very unclear about how to best manage buffers.
// I've chosen to this approach: Instantly enqueue a buffer that was rendered to the last time,
// and then render the next. Hopefully it's okay to spend time in this callback after having enqueued.
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	if (bq != bqPlayerBufferQueue)
	{
		logE << "Wrong bq!\n";
		return;
	}
		chronos::usec delay(130*1000);
		OpenslPlayer* player = (OpenslPlayer*)context;

		if (player->pubStream_->getPlayerChunk(buffer[curBuffer], delay, player->frames_))
		{

			SLresult result;
			do
			{
				result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[curBuffer], player->buff_size);
				if (result == SL_RESULT_BUFFER_INSUFFICIENT)
					usleep(1000);
			}
			while (result == SL_RESULT_BUFFER_INSUFFICIENT);
		}
	curBuffer ^= 1;	// Switch buffer

/*	int renderedFrames = 100;//audioCallback(buffer[curBuffer], framesPerBuffer);

	int sizeInBytes = framesPerBuffer * 2 * sizeof(short);
	int byteCount = (framesPerBuffer - renderedFrames) * 4;
	if (byteCount > 0) {
		memset(buffer[curBuffer] + renderedFrames * 2, 0, byteCount);
	}
	SLresult result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[curBuffer], sizeInBytes);

	// Comment from sample code:
	// the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
	// which for this code example would indicate a programming error
	if (result != SL_RESULT_SUCCESS)
	{
		logE << "OpenSL ES: Failed to enqueue! " << renderedFrames << " " << sizeInBytes << "\n";
	}

	curBuffer ^= 1;	// Switch buffer
*/
}



OpenslPlayer::OpenslPlayer(const PcmDevice& pcmDevice, Stream* stream) : Player(pcmDevice, stream), buff_(NULL), pubStream_(stream)
{
}


OpenslPlayer::~OpenslPlayer()
{
	stop();
}


void OpenslPlayer::initOpensl()
{
	unsigned int rate;
	int channels;
//	int buff_size;

	const msg::SampleFormat& format = stream_->getFormat();
	rate = format.rate;
	channels = format.channels;

	frames_ = 2048;

	buff_size = frames_ * channels * 2 /* 2 -> sample size */;
	logO << "frames: " << frames_ << ", channels: " << channels << ", rate: " << rate << ", buff: " << buff_size << "\n";
	buff_ = (char *) malloc(buff_size);
	if (buff_ == NULL)
		logO << "NULL\n";

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

logE << "1\n";
	buffer[0] = new char[buff_size];
	buffer[1] = new char[buff_size];
logE << "1\n";

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
logE << "1\n";

	SLuint32 sr = SL_SAMPLINGRATE_44_1;
	if (sampleRate == 48000)
	{
		sr = SL_SAMPLINGRATE_48;
	}

logE << "1\n";
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
logE << "1\n";

	SLDataSource audioSrc = {&loc_bufq, &format_pcm};

	// configure audio sink
	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
	SLDataSink audioSnk = {&loc_outmix, NULL};
logE << "1\n";

	// create audio player
	const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
	const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
	result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 2, ids, req);
	assert(SL_RESULT_SUCCESS == result);

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

	if (buff_ != NULL)
	{
		free(buff_);
		buff_ = NULL;
	}
}


void OpenslPlayer::start()
{
	initOpensl();
	Player::start();
}


void OpenslPlayer::stop()
{
	Player::stop();
	uninitOpensl();
}




void OpenslPlayer::worker()
{
	buff_ = (char *) malloc(buff_size);
	while (active_)
	{
//		const msg::SampleFormat& sampleFormat = stream_->getFormat();
//		logE << "worker\n";
		chronos::usec delay(100*1000);
		usleep(500*1000);
continue;
		if (stream_->getPlayerChunk(buff_, delay, frames_))
		{

			SLresult result;
			do
			{
				result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buff_, buff_size);
				if (result == SL_RESULT_BUFFER_INSUFFICIENT)
					usleep(1000);
			}
			while (result == SL_RESULT_BUFFER_INSUFFICIENT);
//			logO << "got chunk: " << result << "\n";
//			usleep(28500);
		}
		else
		{
			logO << "Failed to get chunk\n";
			while (active_ && !stream_->waitForChunk(100))
				logD << "Waiting for chunk\n";
		}
	}
}

