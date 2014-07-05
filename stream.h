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
	Stream() : lastPlayerChunk(NULL), median(0), shortMedian(0), lastUpdate(0), skip(0), idx(0)
	{
		silentPlayerChunk = new PlayerChunk();
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


	Chunk* getNextChunk()
	{
		Chunk* chunk = NULL;
		if (chunks.empty())
			cv.wait(*pLock);

		mutex.lock();
		chunk = chunks.front();
		mutex.unlock();
		return chunk;
	}


	PlayerChunk* getNextPlayerChunk(int correction = 0)
	{
		Chunk* chunk = getNextChunk();
		if (correction > PLAYER_CHUNK_MS / 2)
			correction = PLAYER_CHUNK_MS/2;
		else if (correction < -PLAYER_CHUNK_MS/2)
			correction = -PLAYER_CHUNK_MS/2;
	
//std::cerr << "GetNextPlayerChunk: " << correction << "\n";		
//		int age(0);
//		age = getAge(*chunk) + outputBufferDacTime*1000 - bufferMs;
//		std::cerr << "age: " << age << " \tidx: " << chunk->idx << "\n"; 
		PlayerChunk* playerChunk = new PlayerChunk();
		playerChunk->tv_sec = chunk->tv_sec;
		playerChunk->tv_usec = chunk->tv_usec;
		playerChunk->idx = 0;

		size_t missing = PLAYER_CHUNK_SIZE;// + correction*PLAYER_CHUNK_MS_SIZE;
/*		double factor = (double)PLAYER_CHUNK_MS / (double)(PLAYER_CHUNK_MS + correction);
		size_t idx(0);
		size_t idxCorrection(0);
		for (size_t n=0; n<PLAYER_CHUNK_SIZE/2; ++n)
		{
			idx = chunk->idx + 2*floor(n*factor) - idxCorrection;
//std::cerr << factor << "\t" << n << "\t" << idx << "\n";
			if (idx >= WIRE_CHUNK_SIZE)
			{
				idxCorrection = 2*floor(n*factor);
				idx = 0;
				chunks.pop_front();
				delete chunk;
				chunk = getNextChunk();
			}
			playerChunk->payload[2*n] = chunk->payload[idx];
			playerChunk->payload[2*n+1] = chunk->payload[idx + 1];
		}
		addMs(chunk, -PLAYER_CHUNK_MS - correction);
		chunk->idx = idx;
		if (idx >= WIRE_CHUNK_SIZE)
		{
			chunks.pop_front();
			delete chunk;
		}
*/
		if (chunk->idx + PLAYER_CHUNK_SIZE > WIRE_CHUNK_SIZE)
		{
//std::cerr << "chunk->idx + PLAYER_CHUNK_SIZE >= WIRE_CHUNK_SIZE: " << chunk->idx + PLAYER_CHUNK_SIZE << " >= " << WIRE_CHUNK_SIZE << "\n";
			memcpy(&(playerChunk->payload[0]), &chunk->payload[chunk->idx], sizeof(int16_t)*(WIRE_CHUNK_SIZE - chunk->idx));
			missing = chunk->idx + PLAYER_CHUNK_SIZE - WIRE_CHUNK_SIZE;
			chunks.pop_front();
			delete chunk;
			
			chunk = getNextChunk();
		}

		memcpy(&(playerChunk->payload[0]), &chunk->payload[chunk->idx], sizeof(int16_t)*missing);

		addMs(chunk, -PLAYER_CHUNK_MS);
		chunk->idx += missing;
		if (chunk->idx >= WIRE_CHUNK_SIZE)
		{
			chunks.pop_front();
			delete chunk;
		}
		return playerChunk;
	}


	PlayerChunk* getChunk(double outputBufferDacTime, unsigned long framesPerBuffer)
	{
		int correction(0);
		if (pBuffer->full() && (abs(median) <= PLAYER_CHUNK_MS))
			correction = median;
			
		PlayerChunk* playerChunk = getNextPlayerChunk(correction);
		int age = getAge(playerChunk) - bufferMs + outputBufferDacTime*1000;
		pBuffer->add(age);
		pShortBuffer->add(age);
//		std::cerr << "Chunk: " << age << "\t" << outputBufferDacTime*1000 << "\n";

		int sleep(0);
		time_t now = time(NULL);
		if (now != lastUpdate)
		{
			lastUpdate = now;
			median = pBuffer->median();
			shortMedian = pShortBuffer->median();
			if (abs(age) > 300)
				sleep = age;
			if (pShortBuffer->full() && (abs(shortMedian) > WIRE_CHUNK_MS))
				sleep = shortMedian;
			if (pBuffer->full() && (abs(median) > PLAYER_CHUNK_MS))
				sleep = median;
			std::cerr << "Chunk: " << age << "\t" << shortMedian << "\t" << median << "\t" << pBuffer->size() << "\t" << outputBufferDacTime*1000 << "\n";
		}

		if (sleep != 0)
		{
			std::cerr << "Sleep: " << sleep << "\n";
			pBuffer->clear();
			pShortBuffer->clear();
			if (sleep < 0)
			{
				sleepMs(100-sleep);
			}
			else
			{
				for (size_t n=0; n<(size_t)(sleep / PLAYER_CHUNK_MS); ++n)
				{
					delete playerChunk;
					playerChunk = getNextPlayerChunk();
				}
			}
		}


//		int age = getAge(*lastPlayerChunk) + outputBufferDacTime*1000 - bufferMs;
		return playerChunk;
	}

/*		if (age < -500)
		{
			std::vector<short> v;
			if (skip < 0)
			{
				++skip;
				std::cerr << "chunk too new, sleeping\n";//age > WIRE_CHUNK_MS (" << age << " ms)\n";
				for (size_t n=0; n<(size_t)PLAYER_CHUNK_SIZE; ++n)
					v.push_back(chunk->payload[n]);
				return v;
			}
		}

		pBuffer->add(age);
		pShortBuffer->add(age);

		time_t now = time(NULL);

		if (skip == 0)
		{
			if (now != lastUpdate)
			{
				lastUpdate = now;
				median = pBuffer->median();
				shortMedian = pShortBuffer->median();
				if (abs(age) > 500)
					skip = age / PLAYER_CHUNK_SIZE;
				if (pShortBuffer->full() && (abs(shortMedian) > WIRE_CHUNK_MS))
					skip = shortMedian / PLAYER_CHUNK_SIZE;
				if (pBuffer->full() && (abs(median) > WIRE_CHUNK_MS))
					skip = median / PLAYER_CHUNK_SIZE;
			}
				std::cerr << "Chunk: " << age << "\t" << shortMedian << "\t" << median << "\t" << pBuffer->size() << "\t" << chunkCount << "\t" << outputBufferDacTime*1000 << "\n";
		}
		
		std::vector<short> v;
		if (skip < 0)
		{
			++skip;
			std::cerr << "chunk too new, sleeping\n";//age > WIRE_CHUNK_MS (" << age << " ms)\n";
			for (size_t n=0; n<(size_t)PLAYER_CHUNK_SIZE; ++n)
				v.push_back(chunk->payload[n]);
			return v;
		}

		for (size_t n=chunk->idx; n<chunk->idx + (size_t)PLAYER_CHUNK_SIZE; ++n)
		{
			v.push_back(chunk->payload[n]);
		}
//std::cerr << "before: " << chunkTime(*chunk) << ", after: ";
		addMs(*chunk, -PLAYER_CHUNK_MS);
//std::cerr << chunkTime(*chunk) << "\n";
		chunk->idx += PLAYER_CHUNK_SIZE;
		if (chunk->idx >= WIRE_CHUNK_SIZE)
		{
//std::cerr << "Chunk played out, deleting\n";
			chunks.pop_front();
			delete chunk;
		}
		return v;
	}
*/






/*
	std::vector<short> getChunk(double outputBufferDacTime, unsigned long framesPerBuffer)
	{
		while (1)
		{
			Chunk* chunk = NULL;
			if (chunks.empty())
				cv.wait(*pLock);

			int age(0);
			int chunkCount(0);
			mutex.lock();
			chunk = chunks.front();

			while (!chunks.empty())
			{
				chunk = chunks.front();
				if (skip != 0)
				{
					pBuffer->clear();
					pShortBuffer->clear();
					shortMedian = 0;
					median = 0;
				}
				if (skip > 0)
				{
					--skip;
					std::cerr << "chunk too old, skipping\n";//age > WIRE_CHUNK_MS (" << age << " ms)\n";
					delete chunk;
					chunk = NULL;
					chunks.pop_front();
				}
				else
					break;
			} 
			chunkCount = chunks.size();
			mutex.unlock();
			
			if (chunk == NULL)
			{
				std::cerr << "no chunks available\n";
				continue;
			}

			age = getAge(*chunk) + outputBufferDacTime*1000 - bufferMs;
			pBuffer->add(age);
			pShortBuffer->add(age);

			time_t now = time(NULL);

			if (skip == 0)
			{
				if (now != lastUpdate)
				{
					lastUpdate = now;
					median = pBuffer->median();
					shortMedian = pShortBuffer->median();
					if (abs(age) > 500)
						skip = age / PLAYER_CHUNK_SIZE;
					if (pShortBuffer->full() && (abs(shortMedian) > WIRE_CHUNK_MS))
						skip = shortMedian / PLAYER_CHUNK_SIZE;
					if (pBuffer->full() && (abs(median) > WIRE_CHUNK_MS))
						skip = median / PLAYER_CHUNK_SIZE;
				}
					std::cerr << "Chunk: " << age << "\t" << shortMedian << "\t" << median << "\t" << pBuffer->size() << "\t" << chunkCount << "\t" << outputBufferDacTime*1000 << "\n";
			}
			
			std::vector<short> v;
			if (skip < 0)
			{
				++skip;
				std::cerr << "chunk too new, sleeping\n";//age > WIRE_CHUNK_MS (" << age << " ms)\n";
				for (size_t n=0; n<(size_t)PLAYER_CHUNK_SIZE; ++n)
					v.push_back(chunk->payload[n]);
				return v;
			}

			for (size_t n=chunk->idx; n<chunk->idx + (size_t)PLAYER_CHUNK_SIZE; ++n)
			{
				v.push_back(chunk->payload[n]);
			}
//std::cerr << "before: " << chunkTime(*chunk) << ", after: ";
			addMs(*chunk, -PLAYER_CHUNK_MS);
//std::cerr << chunkTime(*chunk) << "\n";
			chunk->idx += PLAYER_CHUNK_SIZE;
			if (chunk->idx >= WIRE_CHUNK_SIZE)
			{
//std::cerr << "Chunk played out, deleting\n";
				chunks.pop_front();
				delete chunk;
			}
			return v;
		}
	}
*/


/*
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
*/

private:
	void sleepMs(int ms)
	{
		if (ms > 0)
			usleep(ms * 1000);
	}


	std::deque<Chunk*> chunks;
	std::mutex mtx;
	std::mutex mutex;
	std::unique_lock<std::mutex>* pLock;
	std::condition_variable cv;
	DoubleBuffer<int>* pBuffer;
	DoubleBuffer<int>* pShortBuffer;

	PlayerChunk* lastPlayerChunk;
	PlayerChunk* silentPlayerChunk;

	int median;
	int shortMedian;
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


