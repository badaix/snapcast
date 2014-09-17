#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <thread>
#include <atomic>
#include <alsa/asoundlib.h>
#include "stream.h"


class Player
{
public:
    Player(Stream* stream);
    void start();
    void stop();

private:
    void worker();
    snd_pcm_t* pcm_handle;
    snd_pcm_uframes_t frames;
    char *buff;
    std::atomic<bool> active_;
    Stream* stream_;
    std::thread* playerThread;
};


#endif

