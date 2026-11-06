#pragma once

struct SDL_AudioStream;

namespace umbra::audio
{
    class Mixer;
}

namespace umbra::platform
{
    // The default sound output. SDL calls back on its audio thread for more samples,
    // which come from the mixer (see engine/audio/include/Audio.md).
    // If there is no sound device, Open returns false and the game runs silently.
    class AudioDevice final
    {
    public:
        AudioDevice() = default;
        ~AudioDevice();

        AudioDevice(const AudioDevice &) = delete;
        AudioDevice &operator=(const AudioDevice &) = delete;

        // The mixer must outlive the device (or Close must be called first).
        [[nodiscard]] bool Open(audio::Mixer &mixer);
        void Close() noexcept;
        [[nodiscard]] bool IsOpen() const noexcept { return stream_ != nullptr; }

    private:
        static void Feed(void *self, SDL_AudioStream *stream, int bytesNeeded, int totalBytes);

        SDL_AudioStream *stream_ = nullptr;
        audio::Mixer *mixer_ = nullptr;
    };
}
