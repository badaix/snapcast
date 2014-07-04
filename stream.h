#ifndef STREAM_H
#define STREAM_H


#include <deque>
#include <mutex>
#include <condition_variable>
#include <vector>
#include "doubleBuffer.h"
#include "timeUtils.h"
#include "chunk.h"
#include "timeUtils.h"


class Stream
{
public:
	Stream() : lastUpdate(0), skip(0), idx(0)
	{
		pBuffer = new DoubleBuffer<int>(30000 / PLAYER_CHUNK_MS);
		pShortBuffer = new DoubleBuffer<int>(5000 / PLAYER_CHUNK_MS);
		pLock = new std::unique_lock<std::mutex>(mtx);
		bufferMs = 500;
	}

	void addChunk(Chunk* chunk)
	{
		Chunk* c = new Chunk(*chunk);
		mutex.lock();
		chunks.push_back(c);
		mutex.unlock();
		cv.notify_all();
	}

	std::vector<short> getChunk(double outputBufferDacTime, unsigned long framesPerBuffer)
	{
		Chunk* chunk = NULL;
		while (1)
		{
			if (chunks.empty())
				cv.wait(*pLock);

			int age(0);
			int chunkCount(0);
			mutex.lock();
			do
			{
				chunk = chunks.front();
				chunkCount = chunks.size();
				int age = getAge(*chunk) + outputBufferDacTime*1000 - bufferMs;
				if (age > WIRE_CHUNK_MS)
				{
					std::cerr << "age > WIRE_CHUNK_MS\n";
					chunks.pop_front();
					delete chunk;
					chunk = NULL;
				}
			} 
			while (chunk == NULL);
			mutex.unlock();
			
			if (chunk == NULL)
			{
				std::cerr << "no chunks available\n";
				continue;
			}

			std::cerr << "age: " << getAge(*chunk) << "\t" << age << "\t" << pBuffer->size() << "\t" << chunkCount << "\n";
			pBuffer->add(age);
			pShortBuffer->add(age);
			time_t now = time(NULL);
	
			if (skip == 0)
			{
				int age = 0;
				int median = 0;
				int shortMedian = 0;
	
				if (now != lastUpdate)
				{
					lastUpdate = now;
					median = pBuffer->median();
					shortMedian = pShortBuffer->median();
					std::cerr << "age: " << getAge(*chunk) << "\t" << age << "\t" << shortMedian << "\t" << median << "\t" << pBuffer->size() << "\t" << chunkCount << "\t" << outputBufferDacTime*1000 << "\n";
				}
				if ((age > 500) || (age < -500))
					skip = age / PLAYER_CHUNK_MS;
				else if (pShortBuffer->full() && ((shortMedian > 100) || (shortMedian < -100)))
					skip = shortMedian / PLAYER_CHUNK_MS;
				else if (pBuffer->full() && ((median > 10) || (median < -10)))
					skip = median / PLAYER_CHUNK_MS;
			}
		
			if (skip != 0)
			{
//				std::cerr << "age: " << getAge(*chunk) << "\t" << age << "\t" << shortMedian << "\t" << median << "\t" << buffer->size() << "\t" << outputBufferDacTime*1000 << "\n";
			}

	//		bool silence = (age < -500) || (shortBuffer.full() && (shortMedian < -100)) || (buffer.full() && (median < -15));
			if (skip > 0)
			{
				skip--;
				chunks.pop_front();
				delete chunk;
				std::cerr << "packe too old, dropping\n";
				pBuffer->clear();
				pShortBuffer->clear();
				usleep(100);
			}
			else if (skip < 0)
			{
				skip++;
				chunk = new Chunk();//PlayerChunk();
				memset(&(chunk->payload[0]), 0, sizeof(int16_t)*PLAYER_CHUNK_SIZE);
	//			std::cerr << "age < bufferMs (" << age << " < " << bufferMs << "), playing silence\n";
				pBuffer->clear();
				pShortBuffer->clear();
				usleep(100);
				break;
			}
			else
			{
				chunks.pop_front();
				break;
			}
		}
		
		std::vector<short> v;
		for (size_t n=0; n<framesPerBuffer; ++n)
		{
			v.push_back(0);
			v.push_back(0);
		}
		return v;
//		return chunk;
	}


private:
	std::deque<Chunk*> chunks;
	std::mutex mtx;
	std::mutex mutex;
	std::unique_lock<std::mutex>* pLock;
	std::condition_variable cv;
	DoubleBuffer<int>* pBuffer;
	DoubleBuffer<int>* pShortBuffer;

	time_t lastUpdate;
	int bufferMs;
	int skip;
	size_t idx;
};


//		std::cerr << chunk->tv_sec << "\t" << now.tv_sec << "\n";
/*		for (size_t n=0; n<WIRE_CHUNK_MS/PLAYER_CHUNK_MS; ++n)
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
*/



#endif


