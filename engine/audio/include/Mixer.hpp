#pragma once

#include <Ogg.hpp>
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
        void StopAll(); // sound effects; music keeps playing

        // Music: one track at a time, looping. A new track fades in over `fadeSeconds` while the
        // old one fades out (a crossfade); nullptr fades the music out. Keeps playing while paused.
        // keepTime: the new track starts at the playing one's moment (a variant of the same music, Mixer.md).
        void PlayMusic(std::shared_ptr<const MusicTrack> track, float volume = 0.7F, float fadeSeconds = 1.5F, bool keepTime = false);
        [[nodiscard]] std::size_t MusicFrame() const; // the current track's frame (0 with none)
        [[nodiscard]] std::shared_ptr<const MusicTrack> Music() const;
        [[nodiscard]] std::size_t VoiceCount() const;

    private:
        struct Voice final
        {
            std::shared_ptr<const AudioClip> clip; // keeps the samples alive while playing
            std::size_t frame = 0;
            float volume = 1.0F;
            Bus bus = Bus::Game;
        };

        struct MusicStream final
        {
            std::shared_ptr<const MusicTrack> track;
            std::unique_ptr<MusicDecoder> decoder;
            float gain = 0.0F, target = 0.0F, step = 0.0F; // gain moves by `step` per frame toward `target`
        };

        mutable std::mutex mutex_;
        std::vector<Voice> voices_;
        std::vector<MusicStream> music_; // the current track last; older ones are fading out
        bool gamePaused_ = false;
    };
}
