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
#include <portaudio.h>
#include "chunk.h"
#include "timeUtils.h"
#include "stream.h"


std::deque<int> timeDiffs;
int bufferMs;


void player() 
{
}


void sleepMs(int ms)
{
	if (ms > 0)
		usleep(ms * 1000);
}


Stream* stream;

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
    Stream* stream = (Stream*)userData;
    short* out = (short*)outputBuffer;

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    
	std::vector<short> s = stream->getChunk(timeInfo->outputBufferDacTime, framesPerBuffer);
	
	for (size_t n=0; n<framesPerBuffer; n++)
	{
	    *out++ = s[2*n];
	    *out++ = s[2*n+1];
	}
//	delete chunk;
    
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
    PaStream *paStream;
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
              &paStream,
              NULL, /* no input */
              &outputParameters,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              patestCallback,
              stream );
    if( err != paNoError ) goto error;

    err = Pa_SetStreamFinishedCallback( paStream, &StreamFinished );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( paStream );
    if( err != paNoError ) goto error;

//    printf("Play for %d seconds.\n", NUM_SECONDS );
//    Pa_Sleep( NUM_SECONDS * 1000 );

//    err = Pa_StopStream( paStream );
//    if( err != paNoError ) goto error;

//    err = Pa_CloseStream( paStream );
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
	stream = new Stream();
	initAudio();
	Chunk* chunk;// = new Chunk();
    while (1)
    {
        zmq::message_t update;
        subscriber.recv(&update);

		timeval now;
		gettimeofday(&now, NULL);
		std::cerr << "New chunk: " << chunkTime(*chunk) << "\t" << timeToStr(now) << "\t" << getAge(*chunk) << "\n";
//        memcpy(chunk, update.data(), sizeof(Chunk));
		chunk = (Chunk*)(update.data());
		stream->addChunk(chunk);
    }
    return 0;
}


