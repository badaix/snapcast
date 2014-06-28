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


#define MS (50)
//44100 / 20 = 2205
#define SAMPLE_RATE (44100)
#define SIZE (SAMPLE_RATE*4*MS/1000)
int bufferMs;
#define FRAMES_PER_BUFFER  (SIZE/4)


struct Chunk
{
	int32_t tv_sec;
	int32_t tv_usec;
	char payload[SIZE];
};

std::deque<Chunk*> chunks;
std::deque<int> timeDiffs;
std::mutex mtx;
std::mutex mutex;
std::condition_variable cv;

std::string timeToStr(const timeval& timestamp)
{
	char tmbuf[64], buf[64];
	struct tm *nowtm;
	time_t nowtime;
	nowtime = timestamp.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
	snprintf(buf, sizeof buf, "%s.%06d", tmbuf, (int)timestamp.tv_usec);
	return buf;
}


std::string chunkTime(const Chunk& chunk)
{
	timeval ts;
	ts.tv_sec = chunk.tv_sec;
	ts.tv_usec = chunk.tv_usec;
	return timeToStr(ts);
}


int diff_ms(const timeval& t1, const timeval& t2)
{
    return (((t1.tv_sec - t2.tv_sec) * 1000000) + 
            (t1.tv_usec - t2.tv_usec))/1000;
}


int getAge(const Chunk& chunk)
{
	timeval now;
	gettimeofday(&now, NULL);
	timeval ts;
	ts.tv_sec = chunk.tv_sec;
	ts.tv_usec = chunk.tv_usec;
	return diff_ms(now, ts);
}
 
long getTickCount()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec*1000 + now.tv_nsec / 1000000;
}

void player() 
{
	std::unique_lock<std::mutex> lck(mtx);
	bool playing = true;
//	struct timespec last;
	while (1)
	{
		if (chunks.empty())
			cv.wait(lck);
		mutex.lock();
		std::cerr << "Chunks: " << chunks.size() << "\t" << getAge(*(chunks.front())) << "\n";
		if (getAge(*(chunks.front())) > bufferMs)
		{
			mutex.unlock();
			break;
		}
		mutex.unlock();
	}

	while (1)
	{
		if (chunks.empty())
			cv.wait(lck);
		mutex.lock();
		Chunk* chunk = chunks.front();
//		std::cerr << "Chunks: " << chunks.size() << "\n";
		chunks.pop_front();
		mutex.unlock();
	
//		playing = playing || (getAge(*chunks.front()) > 200);

		if (playing)
		{
			long now = getTickCount();
//			std::cerr << "Before: " << now << "\n";
	        for (size_t n=0; n<SIZE; ++n)
           		std::cout << chunk->payload[n];// << std::flush;
			std::cout << std::flush;
			long after = getTickCount();
//			std::cerr << "After: " << after << " (" << after - now << ")\n";
			if (after - now > MS / 2)
				usleep(((after - now) / 2) * 1000);
				
//			int age = getAge(*chunk);
//			std::cerr << "Age: " << age << "\n";
/*			if (age < 0)
			{
				std::cerr << "Sleeping, age: " << age / 2 << "\n";
				usleep((-age / 2) * 1000 - 100);
			}
			else
				std::cerr << "Dropping Chunk, age: " << age << "\n";
*/
/*			int age = getAge(*chunk) - bufferMs;
			if (age < 10)
			{
				if (age < 0)
				{
					usleep((-age) * 1000 - 100);
					age = getAge(*chunk) - bufferMs;
				}
				if (abs(age) > 10)
					std::cerr << "Buffer out of sync: " << age << "\n";
			
		        for (size_t n=0; n<size; ++n)
				{
	           		std::cout << chunk->payload[n];// << std::flush;
//				if (size % 100 == 0)
//					std::cout << std::flush;
				}
				std::cout << std::flush;
			}
			else
				std::cerr << "Dropping Chunk, age: " << age << "\n";
*/		}
		delete chunk;
	}
}



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
	std::cerr << "outputBufferDacTime: " << timeInfo->outputBufferDacTime*1000 << "\n";
    std::deque<Chunk*>* chunks = (std::deque<Chunk*>*)userData;
    short* out = (short*)outputBuffer;
    unsigned long i;

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    
	std::unique_lock<std::mutex> lck(mtx);
	int age = 0;
	Chunk* chunk = NULL;
	while (1)
	{
		if (chunks->empty())
			cv.wait(lck);
		mutex.lock();
		chunk = chunks->front();
		std::cerr << "Chunks: " << chunks->size() << "\n";
		chunks->pop_front();
		mutex.unlock();
		age = getAge(*chunk) + timeInfo->outputBufferDacTime*1000;
		std::cerr << "age: " << getAge(*chunk) << "\t" << age << "\n";
		if (age > bufferMs)
			delete chunk;
		else
			break;
	}
	
    for( i=0; i<framesPerBuffer; i++)
    {
//std::cerr << (int)chunk->payload[i] << "\t" << (int)chunk->payload[i+1] << "\t" << (int)chunk->payload[i+2] << "\t" << (int)chunk->payload[i+3] << "\n";
//std::cerr << i << "\t" << 4*i+1 << "\t" << 4*i << "\n";
        *out++ = (int)chunk->payload[4*i+1]*256 + (int)chunk->payload[4*i];
        *out++ = (int)chunk->payload[4*i+3]*256 + (int)chunk->payload[4*i+2];
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

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default output device.\n");
      goto error;
    }
    outputParameters.channelCount = 2;       /* stereo output */
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
    while (1)
    {
        zmq::message_t update;
        subscriber.recv(&update);
		Chunk* chunk = new Chunk();
        memcpy(chunk, update.data(), sizeof(Chunk));
		timeval now;
		gettimeofday(&now, NULL);
//		std::cerr << "New chunk: " << chunkTime(*chunk) << "\t" << timeToStr(now) << "\t" << getAge(*chunk) << "\n";

		mutex.lock();
		chunks.push_back(chunk);
		mutex.unlock();
		cv.notify_all();
    }
    return 0;
}

