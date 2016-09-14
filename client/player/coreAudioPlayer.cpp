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
#include "coreAudioPlayer.h"

#define NUM_BUFFERS 2


//http://stackoverflow.com/questions/4863811/how-to-use-audioqueue-to-play-a-sound-for-mac-osx-in-c
//https://gist.github.com/andormade/1360885

void callback(void *custom_data, AudioQueueRef queue, AudioQueueBufferRef buffer)
{
	CoreAudioPlayer* player = static_cast<CoreAudioPlayer*>(custom_data);
	player->playerCallback(queue, buffer);
}


CoreAudioPlayer::CoreAudioPlayer(const PcmDevice& pcmDevice, Stream* stream) : 
    Player(pcmDevice, stream),
	ms_(50),
	pubStream_(stream)
{
}


CoreAudioPlayer::~CoreAudioPlayer()
{
}


void CoreAudioPlayer::playerCallback(AudioQueueRef queue, AudioQueueBufferRef bufferRef)
{
    /// Estimate the playout delay by checking the number of frames left in the buffer
    /// and add ms_ (= complete buffer size). Based on trying.
    AudioTimeStamp timestamp;
    AudioQueueGetCurrentTime(queue, timeLine, &timestamp, NULL);
    size_t bufferedFrames = (frames_ - ((uint64_t)timestamp.mSampleTime % frames_)) % frames_;
    size_t bufferedMs = bufferedFrames * 1000 / pubStream_->getFormat().rate + (ms_ * (NUM_BUFFERS - 1));
//    logO << "buffered: " << bufferedFrames << ", ms: " << bufferedMs << ", mSampleTime: " << timestamp.mSampleTime << "\n";

	chronos::usec delay(bufferedMs * 1000);
    char *buffer = (char*)bufferRef->mAudioData;
	if (!pubStream_->getPlayerChunk(buffer, delay, frames_))
	{
		logO << "Failed to get chunk. Playing silence.\n";
		memset(buffer, 0, buff_size_);
	}
	else
	{
		adjustVolume(buffer, frames_);
	}

//    OSStatus status = 
    AudioQueueEnqueueBuffer(queue, bufferRef, 0, NULL);

    if (!active_)
    {
        AudioQueueStop(queue, false);
        AudioQueueDispose(queue, false);
        CFRunLoopStop(CFRunLoopGetCurrent());
    }
}


void CoreAudioPlayer::worker()
{
    const SampleFormat& sampleFormat = pubStream_->getFormat();

    AudioStreamBasicDescription format;
    format.mSampleRate       = sampleFormat.rate;
    format.mFormatID         = kAudioFormatLinearPCM;
    format.mFormatFlags      = kLinearPCMFormatFlagIsSignedInteger;// | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel   = sampleFormat.bits;
    format.mChannelsPerFrame = sampleFormat.channels;
    format.mBytesPerFrame    = sampleFormat.frameSize;
    format.mFramesPerPacket  = 1;
    format.mBytesPerPacket   = format.mBytesPerFrame * format.mFramesPerPacket;
    format.mReserved         = 0;

    AudioQueueRef queue;
    AudioQueueNewOutput(&format, callback, this, CFRunLoopGetCurrent(), kCFRunLoopCommonModes, 0, &queue);
    AudioQueueCreateTimeline(queue, &timeLine);
    
	frames_ = (sampleFormat.rate * ms_) / 1000;
    ms_ = frames_ * 1000 / sampleFormat.rate;
    logO << "frames: " << frames_ << ", ms: " << ms_ << "\n";
	buff_size_ = frames_ * sampleFormat.frameSize;
    
    AudioQueueBufferRef buffers[NUM_BUFFERS];
    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        AudioQueueAllocateBuffer(queue, buff_size_, &buffers[i]);
        buffers[i]->mAudioDataByteSize = buff_size_;
        callback(this, queue, buffers[i]);
    }

    logE << "CoreAudioPlayer::worker\n";
    AudioQueueCreateTimeline(queue, &timeLine);
    AudioQueueStart(queue, NULL);
    CFRunLoopRun();    
}


