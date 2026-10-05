// Exercise the real ALSA player and Stream with ALSA's null PCM. Linker wrappers
// inject device failures without requiring a sound card or replacing Stream.
#include "client/player/alsa_player.hpp"
#include "client/time_provider.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>

using namespace std::chrono_literals;

namespace
{
std::atomic<int> opens{0}, closes{0}, writes{0}, audio_writes{0}, prepares{0};
std::atomic<int> wait_error{0}, write_error{0}, prepare_error{0};
std::atomic<bool> wait_timeout{false}, partial_write{false}, partial_ok{false};
bool force_unsigned = false;
size_t frame_size = 4;
unsigned char silence = 0;
std::atomic<int> params_error{0};
const char* pending_data = nullptr;
snd_pcm_uframes_t remaining = 0;
int partial_stage = 0;

bool eventually(const std::function<bool()>& condition, std::chrono::milliseconds timeout = 2s)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (!condition() && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(5ms);
    return condition();
}

struct Fixture
{
    boost::asio::io_context context;
    SampleFormat format;
    std::shared_ptr<Stream> stream;
    std::unique_ptr<player::AlsaPlayer> player;

    explicit Fixture(uint16_t bits = 16, uint32_t rate = 48000, uint16_t channels = 2)
        : format(rate, bits, channels), stream(std::make_shared<Stream>(format, format))
    {
        force_unsigned = bits == 8;
        frame_size = format.frameSize();
        silence = force_unsigned ? 0x80 : 0;
        params_error = 0;
        opens = closes = writes = audio_writes = prepares = 0;
        wait_error = write_error = prepare_error = 0;
        wait_timeout = partial_write = partial_ok = false;
        partial_stage = 0;
        ClientSettings::Player settings;
        settings.player_name = "alsa";
        settings.pcm_device.name = "null";
        settings.mixer.mode = ClientSettings::Mixer::Mode::none;
        stream->setBufferLen(100);
        player = std::make_unique<player::AlsaPlayer>(context, settings, stream);
        player->start();
    }

    ~Fixture()
    {
        player->stop();
    }

    void feed()
    {
        auto chunk = std::make_unique<msg::PcmChunk>(format, 200);
        std::memset(chunk->payload, 1, chunk->payloadSize);
        const auto timestamp = TimeProvider::sinceEpoche<chronos::usec>(TimeProvider::serverNow() - 50ms).count();
        chunk->timestamp = tv(timestamp / 1000000, timestamp % 1000000);
        stream->addChunk(std::move(chunk));
    }
};
} // namespace

extern "C"
{
    int __real_snd_pcm_open(snd_pcm_t**, const char*, snd_pcm_stream_t, int);
    int __wrap_snd_pcm_open(snd_pcm_t** pcm, const char* name, snd_pcm_stream_t stream, int mode)
    {
        // The production player must use nonblocking writes for stop() to stay bounded.
        if (!(mode & SND_PCM_NONBLOCK))
            return -EINVAL;
        ++opens;
        return __real_snd_pcm_open(pcm, name, stream, mode);
    }
    int __real_snd_pcm_close(snd_pcm_t*);
    int __wrap_snd_pcm_close(snd_pcm_t* pcm)
    {
        ++closes;
        return __real_snd_pcm_close(pcm);
    }
    int __real_snd_pcm_hw_params_set_format(snd_pcm_t*, snd_pcm_hw_params_t*, snd_pcm_format_t);
    int __wrap_snd_pcm_hw_params_set_format(snd_pcm_t* pcm, snd_pcm_hw_params_t* params, snd_pcm_format_t format)
    {
        if (force_unsigned && format == SND_PCM_FORMAT_S8)
            return -EINVAL;
        return __real_snd_pcm_hw_params_set_format(pcm, params, format);
    }
    int __real_snd_pcm_hw_params(snd_pcm_t*, snd_pcm_hw_params_t*);
    int __wrap_snd_pcm_hw_params(snd_pcm_t* pcm, snd_pcm_hw_params_t* params)
    {
        const int error = params_error.exchange(0);
        return error ? error : __real_snd_pcm_hw_params(pcm, params);
    }
    int __real_snd_pcm_prepare(snd_pcm_t*);
    int __wrap_snd_pcm_prepare(snd_pcm_t* pcm)
    {
        ++prepares;
        const int error = prepare_error.exchange(0);
        return error ? error : __real_snd_pcm_prepare(pcm);
    }
    int __wrap_snd_pcm_wait(snd_pcm_t*, int timeout)
    {
        std::this_thread::sleep_for(wait_timeout ? std::chrono::milliseconds(timeout) : 2ms);
        const int error = wait_error.exchange(0);
        return error ? error : (wait_timeout ? 0 : 1);
    }
    snd_pcm_sframes_t __real_snd_pcm_writei(snd_pcm_t*, const void*, snd_pcm_uframes_t);
    snd_pcm_sframes_t __wrap_snd_pcm_writei(snd_pcm_t* pcm, const void* data, snd_pcm_uframes_t frames)
    {
        const int error = write_error.load();
        if (error)
        {
            if (error != -EAGAIN)
                write_error = 0;
            return error;
        }
        if (partial_write && partial_stage == 0 && frames > 1)
        {
            const auto count = frames / 2;
            pending_data = static_cast<const char*>(data) + count * frame_size;
            remaining = frames - count;
            partial_stage = 1;
            return __real_snd_pcm_writei(pcm, data, count);
        }
        if (partial_write && partial_stage == 1)
        {
            partial_stage = 2;
            return -EAGAIN;
        }
        if (partial_write && partial_stage == 2)
        {
            partial_ok = (data == pending_data && frames == remaining);
            partial_write = false;
        }
        ++writes;
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < frames * frame_size; ++i)
        {
            if (bytes[i] != silence)
            {
                ++audio_writes;
                break;
            }
        }
        return __real_snd_pcm_writei(pcm, data, frames);
    }
}

TEST_CASE("ALSA bridges a brief stream gap with silence and resumes audio", "[alsa]")
{
    Fixture f;
    REQUIRE(eventually([] { return writes > 3; }));
    REQUIRE(audio_writes == 0);
    f.feed();
    REQUIRE(eventually([] { return audio_writes > 0; }));
    const auto before = writes.load();
    std::this_thread::sleep_for(250ms);
    REQUIRE(writes > before);
    const auto audio_before = audio_writes.load();
    f.feed();
    REQUIRE(eventually([&] { return audio_writes > audio_before; }));
    REQUIRE(opens == 1);
    REQUIRE(closes == 0);
}

TEST_CASE("ALSA releases an idle device even when readiness keeps timing out", "[alsa]")
{
    Fixture f;
    wait_timeout = true;
    REQUIRE(eventually([] { return closes > 0; }, 6s));
    REQUIRE(opens == 1);
    wait_timeout = false;
    f.feed();
    REQUIRE(eventually([] { return opens > 1 && audio_writes > 0; }));
}

TEST_CASE("ALSA checks recovery and reopens a device that cannot be prepared", "[alsa]")
{
    Fixture f;
    REQUIRE(eventually([] { return writes > 0; }));
    SECTION("underrun during wait")
    {
        wait_error = -EPIPE;
    }
    SECTION("underrun during write")
    {
        write_error = -EPIPE;
    }
    SECTION("suspend during wait")
    {
        wait_error = -ESTRPIPE;
    }
    SECTION("suspend during write")
    {
        write_error = -ESTRPIPE;
    }
    REQUIRE(eventually([] { return prepares > 0; }));
    const auto previous_opens = opens.load();
    prepare_error = -EIO;
    wait_error = -EPIPE;
    REQUIRE(eventually([&] { return opens > previous_opens; }));
    f.feed();
    REQUIRE(eventually([] { return audio_writes > 0; }));
}

TEST_CASE("ALSA preserves the pending_data of partial nonblocking writes", "[alsa]")
{
    Fixture f;
    partial_write = true;
    REQUIRE(eventually([] { return partial_ok.load(); }));
}

TEST_CASE("ALSA reopens stalled writes and stops promptly", "[alsa]")
{
    Fixture f;
    REQUIRE(eventually([] { return writes > 0; }));
    write_error = -EAGAIN;
    REQUIRE(eventually([] { return opens > 1; }));
    const auto start = std::chrono::steady_clock::now();
    f.player->stop();
    REQUIRE(std::chrono::steady_clock::now() - start < 500ms);
}


TEST_CASE("ALSA uses the unsigned midpoint for 8-bit silence", "[alsa]")
{
    Fixture f(8);
    REQUIRE(eventually([] { return writes > 3; }));
    REQUIRE(audio_writes == 0);
    f.feed();
    REQUIRE(eventually([] { return audio_writes > 0; }));
}

TEST_CASE("ALSA closes a partially initialized device before retrying", "[alsa]")
{
    Fixture f;
    REQUIRE(eventually([] { return writes > 0; }));
    params_error = -EIO;
    write_error = -ENODEV;
    REQUIRE(eventually([] { return opens >= 3 && closes >= 2; }));
    f.feed();
    REQUIRE(eventually([] { return audio_writes > 0; }));
}

TEST_CASE("ALSA stops promptly while waiting for a stream after idle close", "[alsa]")
{
    Fixture f;
    REQUIRE(eventually([] { return closes > 0; }, 6s));
    const auto start = std::chrono::steady_clock::now();
    f.player->stop();
    REQUIRE(std::chrono::steady_clock::now() - start < 500ms);
}


TEST_CASE("ALSA can stop and restart with announcement sample formats", "[alsa]")
{
    {
        Fixture music(16, 48000, 2);
        music.feed();
        REQUIRE(eventually([] { return audio_writes > 0; }));
    }
    {
        Fixture announcement(16, 22050, 1);
        announcement.feed();
        REQUIRE(eventually([] { return audio_writes > 0; }));
    }
    {
        Fixture music(24, 44100, 2);
        music.feed();
        REQUIRE(eventually([] { return audio_writes > 0; }));
    }
}
