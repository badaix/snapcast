//
//  Weather update client in C++
//  Connects SUB socket to tcp://localhost:5556
//  Collects weather updates and finds avg temp in zipcode
//
//  Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>
//
#include <zmq.hpp>
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <unistd.h>
#include <deque>
#include <vector>
#include <algorithm>
#include <thread> 
#include <mutex>
#include <condition_variable>
#include <portaudio.h>
#include "chunk.h"
#include "doubleBuffer.h"
#include "timeUtils.h"


DoubleBuffer<int> buffer(30000 / PLAYER_CHUNK_MS);
DoubleBuffer<int> shortBuffer(500 / PLAYER_CHUNK_MS);
std::deque<PlayerChunk*> chunks;
std::deque<int> timeDiffs;
std::mutex mtx;
std::mutex mutex;
std::condition_variable cv;
int bufferMs;


void player() 
{
}


void sleepMs(int ms)
{
	if (ms > 0)
		usleep(ms * 1000);
}


int skip(0);
time_t lastUpdate(0);


/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int patestCallback( const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData )
{
//	std::cerr << "outputBufferDacTime: " << timeInfo->outputBufferDacTime*1000 << "\n";
    std::deque<PlayerChunk*>* chunks = (std::deque<PlayerChunk*>*)userData;
    short* out = (short*)outputBuffer;
    unsigned long i;

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    
	std::unique_lock<std::mutex> lck(mtx);
	int age = 0;
	int median = 0;
	int shortMedian = 0;
	PlayerChunk* chunk = NULL;
	while (1)
	{
		if (chunks->empty())
			cv.wait(lck);
		mutex.lock();
		chunk = chunks->front();
		int chunkCount = chunks->size();
		mutex.unlock();
		age = getAge(*chunk) + timeInfo->outputBufferDacTime*1000 - bufferMs;
		buffer.add(age);
		shortBuffer.add(age);
		time_t now = time(NULL);
	
		if (skip == 0)
		{
			if (now != lastUpdate)
			{
				lastUpdate = now;
				median = buffer.median();
				shortMedian = shortBuffer.median();
				std::cerr << "age: " << getAge(*chunk) << "\t" << age << "\t" << shortMedian << "\t" << median << "\t" << buffer.size() << "\t" << chunkCount << "\t" << timeInfo->outputBufferDacTime*1000 << "\n";
			}
			if ((age > 500) || (age < -500))
				skip = age / PLAYER_CHUNK_MS;
			else if (shortBuffer.full() && ((shortMedian > 100) || (shortMedian < -100)))
				skip = shortMedian / PLAYER_CHUNK_MS;
			else if (buffer.full() && ((median > 10) || (median < -10)))
				skip = median / PLAYER_CHUNK_MS;
		}
		
		if (skip != 0)
		{
			std::cerr << "age: " << getAge(*chunk) << "\t" << age << "\t" << shortMedian << "\t" << median << "\t" << buffer.size() << "\t" << timeInfo->outputBufferDacTime*1000 << "\n";
		}

//		bool silence = (age < -500) || (shortBuffer.full() && (shortMedian < -100)) || (buffer.full() && (median < -15));
		if (skip > 0)
		{
			skip--;
			chunks->pop_front();
			delete chunk;
			std::cerr << "packe too old, dropping\n";
			buffer.clear();
			shortBuffer.clear();
			usleep(100);
		}
		else if (skip < 0)
		{
			skip++;
			chunk = new PlayerChunk();
			memset(&(chunk->payload[0]), 0, sizeof(int16_t)*PLAYER_CHUNK_SIZE);
//			std::cerr << "age < bufferMs (" << age << " < " << bufferMs << "), playing silence\n";
			buffer.clear();
			shortBuffer.clear();
			usleep(100);
			break;
		}
		else
		{
			chunks->pop_front();
			break;
		}
	}
	
    for( i=0; i<framesPerBuffer; i++)
    {
        *out++ = chunk->payload[2*i];
        *out++ = chunk->payload[2*i+1];
    }
	delete chunk;
    
    return paContinue;
}

/*
 * This routine is called by portaudio when playback is done.
 */
static void StreamFinished( void* userData )
{
//   paTestData *data = (paTestData *) userData;
//   printf( "Stream Completed: %s\n", data->message );
}


int initAudio()
{
    PaStreamParameters outputParameters;
    PaStream *stream;
    PaError err;
    
    printf("PortAudio Test: output sine wave. SR = %d, BufSize = %d\n", SAMPLE_RATE, FRAMES_PER_BUFFER);
    
    err = Pa_Initialize();
    if( err != paNoError ) goto error;

	int numDevices;
	numDevices = Pa_GetDeviceCount();
	if( numDevices < 0 )
	{
		printf( "ERROR: Pa_CountDevices returned 0x%x\n", numDevices );
		err = numDevices;
		goto error;
	}
	const PaDeviceInfo *deviceInfo;
	for(int i=0; i<numDevices; i++)
	{
	    deviceInfo = Pa_GetDeviceInfo(i);
		std::cerr << "Device " << i << ": " << deviceInfo->name << "\n";
	}

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) 
	{
      fprintf(stderr,"Error: No default output device.\n");
      goto error;
    }
    outputParameters.channelCount = CHANNELS;       /* stereo output */
    outputParameters.sampleFormat = paInt16; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
              &stream,
              NULL, /* no input */
              &outputParameters,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              patestCallback,
              &chunks );
    if( err != paNoError ) goto error;

    err = Pa_SetStreamFinishedCallback( stream, &StreamFinished );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

//    printf("Play for %d seconds.\n", NUM_SECONDS );
//    Pa_Sleep( NUM_SECONDS * 1000 );

//    err = Pa_StopStream( stream );
//    if( err != paNoError ) goto error;

//    err = Pa_CloseStream( stream );
//    if( err != paNoError ) goto error;

//    Pa_Terminate();
//    printf("Test finished.\n");
    
    return err;
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return err;
}


int main (int argc, char *argv[])
{
	bufferMs = 300;	
	if (argc > 1)
		bufferMs = atoi(argv[1]);
    zmq::context_t context (1);
    zmq::socket_t subscriber (context, ZMQ_SUB);
//    subscriber.connect("tcp://127.0.0.1:123458");
    subscriber.connect("tcp://192.168.0.2:123458");

    const char* filter = "";
    subscriber.setsockopt(ZMQ_SUBSCRIBE, filter, strlen(filter));
/*	std::thread playerThread(player);
	struct sched_param params;
	params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	int ret = pthread_setschedparam(playerThread.native_handle(), SCHED_FIFO, &params);
	if (ret != 0) 
	    std::cerr << "Unsuccessful in setting thread realtime prio" << std::endl;
*/
	initAudio();
	Chunk* chunk;// = new Chunk();
    while (1)
    {
        zmq::message_t update;
        subscriber.recv(&update);
//        memcpy(chunk, update.data(), sizeof(Chunk));
		chunk = (Chunk*)(update.data());
//		timeval now;
//		gettimeofday(&now, NULL);
//		std::cerr << "New chunk: " << chunkTime(*chunk) << "\t" << timeToStr(now) << "\t" << getAge(*chunk) << "\n";
//		std::cerr << chunk->tv_sec << "\t" << now.tv_sec << "\n";
		for (size_t n=0; n<WIRE_CHUNK_MS/PLAYER_CHUNK_MS; ++n)
		{
			PlayerChunk* playerChunk = new PlayerChunk();
			playerChunk->tv_sec = chunk->tv_sec;
			playerChunk->tv_usec = chunk->tv_usec;
			addMs(*playerChunk, n*PLAYER_CHUNK_MS);
			memcpy(&(playerChunk->payload[0]), &chunk->payload[n*PLAYER_CHUNK_SIZE], sizeof(int16_t)*PLAYER_CHUNK_SIZE);
			mutex.lock();
			chunks.push_back(playerChunk);
			mutex.unlock();
			cv.notify_all();
		}
    }
    return 0;
}


