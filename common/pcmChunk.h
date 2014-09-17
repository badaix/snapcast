#ifndef PCM_CHUNK_H
#define PCM_CHUNK_H

#include "message.h"
#include "wireChunk.h"
#include "sampleFormat.h"


typedef std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds> time_point_ms;



class PcmChunk : public WireChunk
{
public:
    PcmChunk(const SampleFormat& sampleFormat, size_t ms);
    PcmChunk();
    ~PcmChunk();

    int readFrames(void* outputBuffer, size_t frameCount);
    bool isEndOfChunk() const;

    inline time_point_ms timePoint() const
    {
        time_point_ms tp;
        std::chrono::milliseconds::rep relativeIdxTp = ((double)idx / ((double)format.rate/1000.));
        return
            tp +
            std::chrono::seconds(timestamp.sec) +
            std::chrono::milliseconds(timestamp.usec / 1000) +
            std::chrono::milliseconds(relativeIdxTp);
    }

    template<typename T>
    inline T getAge() const
    {
        return getAge<T>(timePoint());
    }

    inline long getAge() const
    {
        return getAge<std::chrono::milliseconds>().count();
    }

    inline static long getAge(const time_point_ms& time_point)
    {
        return getAge<std::chrono::milliseconds>(time_point).count();
    }

    template<typename T, typename U>
    static inline T getAge(const std::chrono::time_point<U>& time_point)
    {
        return std::chrono::duration_cast<T>(std::chrono::high_resolution_clock::now() - time_point);
    }

    int seek(int frames);
    double getDuration() const;
    double getDurationUs() const;
    double getTimeLeft() const;
    double getFrameCount() const;

    SampleFormat format;

private:
//	SampleFormat format_;
    uint32_t idx;
};



#endif


