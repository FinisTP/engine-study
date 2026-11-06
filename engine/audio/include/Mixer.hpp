#pragma once

#include <Wav.hpp>

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace umbra::audio
{
    // Game sounds stop while the game is paused; UI sounds (menu clicks) keep playing.
    enum class Bus
    {
        Game,
        Ui
    };

    // Adds playing clips together into one stereo stream. Thread-safe: the game thread
    // calls Play, the audio device thread calls Mix. See Audio.md.
    class Mixer final
    {
    public:
        static constexpr std::size_t maxVoices = 32;

        // Starts a clip from the beginning. False for an empty clip or when all voices are busy.
        bool Play(std::shared_ptr<const AudioClip> clip, float volume = 1.0F, Bus bus = Bus::Game);
        // Writes `frames` stereo frames (L R L R ...) into `out`. Finished voices are dropped.
        void Mix(float *out, std::size_t frames);
        void SetGamePaused(bool paused);
        void StopAll();
        [[nodiscard]] std::size_t VoiceCount() const;

    private:
        struct Voice final
        {
            std::shared_ptr<const AudioClip> clip; // keeps the samples alive while playing
            std::size_t frame = 0;
            float volume = 1.0F;
            Bus bus = Bus::Game;
        };

        mutable std::mutex mutex_;
        std::vector<Voice> voices_;
        bool gamePaused_ = false;
    };
}
