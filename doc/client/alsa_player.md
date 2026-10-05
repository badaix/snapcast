# ALSA playback during stream interruptions

Snapclient's ALSA backend writes silence when no synchronized audio is available
for a short period. This keeps the PCM running while a source pauses or switches
between music and an announcement. Once usable audio returns, the normal Stream
synchronization determines which samples are played.

After five seconds without a usable player chunk, snapclient closes the PCM and
waits for a stream chunk before reopening it. This also applies when ALSA readiness
waits repeatedly time out. The hardware mixer is retained during normal idle close.

PCM writes are nonblocking. Partial writes retain their remaining frames;
`EAGAIN`, interrupted waits, and interrupted writes are retried. A write with no
progress for one second triggers a device reopen. Underruns and suspend errors
reset the PCM with `snd_pcm_prepare`; if that fails, the PCM is closed and reopened.
Audio is fetched again after a reset so playback can recover its synchronization.

## Regression tests

On Linux with ALSA development headers installed:

```sh
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --target alsa_player_test
./bin/alsa_player_test
```

The tests use ALSA's `null` PCM and GNU linker wrappers to inject readiness,
write, recovery, and initialization errors. No physical sound card is needed.
They do not verify kernel-driver behavior or audible synchronization on hardware.

For hardware validation, play music through Music Assistant, trigger repeated
announcements, and verify that music resumes. Repeat with gaps shorter and longer
than five seconds, announcements from idle, and different stream sample formats.
Capture `snapclient --logfilter debug` output and check that device reopen and
recovery messages are followed by successful playback without a service restart.
