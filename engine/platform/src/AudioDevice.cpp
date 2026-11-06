#include <AudioDevice.hpp>

#include <Log.hpp>
#include <Mixer.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <string>

namespace umbra::platform
{
    AudioDevice::~AudioDevice() { Close(); }

    bool AudioDevice::Open(audio::Mixer &mixer)
    {
        if (stream_ != nullptr)
            return true;
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            diagnostics::Error(std::string{"Audio unavailable (game runs silently): "} + SDL_GetError());
            return false;
        }

        // 32-bit float stereo at the mixer's rate; SDL converts to whatever the hardware wants.
        const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, audio::mixRate};
        mixer_ = &mixer;
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioDevice::Feed, this);
        if (stream_ == nullptr)
        {
            diagnostics::Error(std::string{"No audio device (game runs silently): "} + SDL_GetError());
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            mixer_ = nullptr;
            return false;
        }
        SDL_ResumeAudioStreamDevice(stream_); // devices opened this way start paused
        diagnostics::Info("Audio device opened.");
        return true;
    }

    void AudioDevice::Close() noexcept
    {
        if (stream_ == nullptr)
            return;
        SDL_DestroyAudioStream(stream_); // stops the callback before returning
        stream_ = nullptr;
        mixer_ = nullptr;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    void AudioDevice::Feed(void *self, SDL_AudioStream *stream, int bytesNeeded, int)
    {
        // Runs on SDL's audio thread: mix in small blocks until SDL has what it asked for.
        auto &device = *static_cast<AudioDevice *>(self);
        std::array<float, 512 * 2> block{};
        while (bytesNeeded > 0)
        {
            const int frames = std::min(bytesNeeded / static_cast<int>(sizeof(float) * 2) + 1, 512);
            device.mixer_->Mix(block.data(), static_cast<std::size_t>(frames));
            const int bytes = frames * static_cast<int>(sizeof(float) * 2);
            SDL_PutAudioStreamData(stream, block.data(), bytes);
            bytesNeeded -= bytes;
        }
    }
}
