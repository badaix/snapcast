/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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
#include "oboe_player.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/str_compat.hpp"

// 3rd party headers

// standard headers
#include <cstring>
#include <iostream>


using namespace std;

namespace player
{

static constexpr auto LOG_TAG = "OboePlayer";
static constexpr double kDefaultLatency = 50;


OboePlayer::OboePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, stream)
{
    LOG(DEBUG, LOG_TAG) << "Contructor\n";
    LOG(INFO, LOG_TAG) << "Init start\n";
    char* env = getenv("SAMPLE_RATE");
    if (env)
        oboe::DefaultStreamValues::SampleRate = cpt::stoi(env, oboe::DefaultStreamValues::SampleRate);
    env = getenv("FRAMES_PER_BUFFER");
    if (env)
        oboe::DefaultStreamValues::FramesPerBurst = cpt::stoi(env, oboe::DefaultStreamValues::FramesPerBurst);

    LOG(INFO, LOG_TAG) << "DefaultStreamValues::SampleRate: " << oboe::DefaultStreamValues::SampleRate
                       << ", DefaultStreamValues::FramesPerBurst: " << oboe::DefaultStreamValues::FramesPerBurst << "\n";


    auto result = openStream();
    LOG(INFO, LOG_TAG) << "BufferSizeInFrames: " << out_stream_->getBufferSizeInFrames() << ", FramesPerBurst: " << out_stream_->getFramesPerBurst() << "\n";
    if (result != oboe::Result::OK)
        LOG(ERROR, LOG_TAG) << "Error building AudioStream: " << oboe::convertToText(result) << "\n";
    LOG(INFO, LOG_TAG) << "Init done\n";
}


OboePlayer::~OboePlayer()
{
    LOG(DEBUG, LOG_TAG) << "Destructor\n";
    stop();
    auto result = out_stream_->stop(std::chrono::nanoseconds(100ms).count());
    if (result != oboe::Result::OK)
        LOG(ERROR, LOG_TAG) << "Error in AudioStream::stop: " << oboe::convertToText(result) << "\n";
    result = out_stream_->close();
    if (result != oboe::Result::OK)
        LOG(ERROR, LOG_TAG) << "Error in AudioStream::stop: " << oboe::convertToText(result) << "\n";
}


oboe::Result OboePlayer::openStream()
{
    oboe::SharingMode sharing_mode = oboe::SharingMode::Shared;
    if (settings_.sharing_mode == ClientSettings::SharingMode::exclusive)
        sharing_mode = oboe::SharingMode::Exclusive;

    oboe::AudioFormat audio_format;
    switch (stream_->getFormat().bits())
    {
        case 32:
            audio_format = oboe::AudioFormat::I32;
            break;
        case 24:
            audio_format = oboe::AudioFormat::I24;
            break;
        case 16:
        default:
            audio_format = oboe::AudioFormat::I16;
            break;
    }

    // The builder set methods can be chained for convenience.
    oboe::AudioStreamBuilder builder;
    auto result = builder.setSharingMode(sharing_mode)
                      ->setPerformanceMode(oboe::PerformanceMode::None)
                      ->setChannelCount(stream_->getFormat().channels())
                      ->setSampleRate(stream_->getFormat().rate())
                      ->setFormat(audio_format)
                      ->setDataCallback(this)
                      ->setErrorCallback(this)
                      ->setDirection(oboe::Direction::Output)
                      //->setFramesPerCallback((8 * stream->getFormat().rate) / 1000)
                      //->setFramesPerCallback(2 * oboe::DefaultStreamValues::FramesPerBurst)
                      //->setFramesPerCallback(960) // 2*192)
                      ->openStream(out_stream_);

    LOG(INFO, LOG_TAG) << "Hardware sample rate: " << out_stream_->getHardwareSampleRate()
                       << ", hardware format: " << oboe::convertToText(out_stream_->getHardwareFormat()) << "\n";
    if (out_stream_->getAudioApi() == oboe::AudioApi::AAudio)
    {
        LOG(INFO, LOG_TAG) << "AudioApi: AAudio\n";
        latency_tuner_ = nullptr;
    }
    else
    {
        LOG(INFO, LOG_TAG) << "AudioApi: OpenSL\n";
        out_stream_->setBufferSizeInFrames(4 * out_stream_->getFramesPerBurst());
    }

    return result;
}


bool OboePlayer::needsThread() const
{
    return false;
}


double OboePlayer::getCurrentOutputLatencyMillis() const
{
    // Get the time that a known audio frame was presented for playing
    auto result = out_stream_->getTimestamp(CLOCK_MONOTONIC);
    double outputLatencyMillis = kDefaultLatency;
    const int64_t kNanosPerMillisecond = 1000000;
    if (result == oboe::Result::OK)
    {
        oboe::FrameTimestamp playedFrame = result.value();
        // Get the write index for the next audio frame
        int64_t writeIndex = out_stream_->getFramesWritten();
        // Calculate the number of frames between our known frame and the write index
        int64_t frameIndexDelta = writeIndex - playedFrame.position;
        // Calculate the time which the next frame will be presented
        int64_t frameTimeDelta = (frameIndexDelta * oboe::kNanosPerSecond) / (out_stream_->getSampleRate());
        int64_t nextFramePresentationTime = playedFrame.timestamp + frameTimeDelta;
        // Assume that the next frame will be written at the current time
        using namespace std::chrono;
        int64_t nextFrameWriteTime = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
        // Calculate the latency
        outputLatencyMillis = static_cast<double>(nextFramePresentationTime - nextFrameWriteTime) / kNanosPerMillisecond;
    }
    else
    {
        // LOG(ERROR, LOG_TAG) << "Error calculating latency: " << oboe::convertToText(result.error()) << "\n";
    }
    return outputLatencyMillis;
}


oboe::DataCallbackResult OboePlayer::onAudioReady(oboe::AudioStream* /*oboeStream*/, void* audioData, int32_t numFrames)
{
    if (latency_tuner_)
        latency_tuner_->tune();

    double output_latency = getCurrentOutputLatencyMillis();
    // LOG(INFO, LOG_TAG) << "getCurrentOutputLatencyMillis: " << output_latency << ", frames: " << numFrames << "\n";
    chronos::usec delay(static_cast<int>(output_latency * 1000.));

    void* buffer = audioData;
    if (stream_->getFormat().bits() == 24)
    {
        // Oboe expects 24 bit audio in 3 bytes, while Snapcast stores 24 bit in 4 bytes.
        // Data must be converted before passing it to Oboe, but first we need to adabt the buffer size
        size_t needed = stream_->getFormat().frameSize() * numFrames;
        if (audio_data_.size() < needed)
        {
            LOG(INFO, LOG_TAG) << "Resizing audio buffer to " << numFrames << " frames, (" << needed << ") bytes\n";
            audio_data_.resize(needed);
        }
        buffer = audio_data_.data();
    }

    if (!stream_->getPlayerChunkOrSilence(buffer, delay, numFrames))
    {
        // LOG(INFO, LOG_TAG) << "Failed to get chunk. Playing silence.\n";
    }
    else
    {
        adjustVolume(static_cast<char*>(buffer), numFrames);
        if (stream_->getFormat().bits() == 24)
        {
            // Copy the 24 bit, 4 bytes data into Oboes 24 bit, 3 bytes buffer
            for (size_t n = 0; n < static_cast<size_t>(numFrames) * stream_->getFormat().channels(); ++n)
                memcpy(static_cast<char*>(audioData) + 3 * n, audio_data_.data() + 4 * n, 3);
        }
    }

    return oboe::DataCallbackResult::Continue;
}


void OboePlayer::onErrorBeforeClose(oboe::AudioStream* oboeStream, oboe::Result error)
{
    std::ignore = oboeStream;
    LOG(INFO, LOG_TAG) << "onErrorBeforeClose: " << oboe::convertToText(error) << "\n";
    stop();
}


void OboePlayer::onErrorAfterClose(oboe::AudioStream* oboeStream, oboe::Result error)
{
    // Tech Note: Disconnected Streams and Plugin Issues
    // https://github.com/google/oboe/blob/master/docs/notes/disconnect.md
    std::ignore = oboeStream;
    LOG(INFO, LOG_TAG) << "onErrorAfterClose: " << oboe::convertToText(error) << "\n";
    auto result = openStream();
    if (result != oboe::Result::OK)
        LOG(ERROR, LOG_TAG) << "Error building AudioStream: " << oboe::convertToText(result) << "\n";
    start();
}


void OboePlayer::start()
{
    // Typically, start the stream after querying some stream information, as well as some input from the user
    LOG(INFO, LOG_TAG) << "Start\n";
    auto result = out_stream_->requestStart();
    if (result != oboe::Result::OK)
        LOG(ERROR, LOG_TAG) << "Error in requestStart: " << oboe::convertToText(result) << "\n";
}


void OboePlayer::stop()
{
    LOG(INFO, LOG_TAG) << "Stop\n";
    auto result = out_stream_->requestStop();
    if (result != oboe::Result::OK)
        LOG(ERROR, LOG_TAG) << "Error in requestStop: " << oboe::convertToText(result) << "\n";
}

} // namespace player
