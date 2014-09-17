#ifndef TIME_PROVIDER_H
#define TIME_PROVIDER_H

#include "doubleBuffer.h"

class TimeProvider
{
public:
    static TimeProvider& getInstance()
    {
        static TimeProvider instance;
        return instance;
    }

    void setDiffToServer(double ms);
    long getDiffToServer();
    long getDiffToServerMs();

private:
    TimeProvider();                   // Constructor? (the {} brackets) are needed here.
    // Dont forget to declare these two. You want to make sure they
    // are unaccessable otherwise you may accidently get copies of
    // your singleton appearing.
    TimeProvider(TimeProvider const&);              // Don't Implement
    void operator=(TimeProvider const&); // Don't implement

    DoubleBuffer<long> diffBuffer;
    long diffToServer;
};


#endif


