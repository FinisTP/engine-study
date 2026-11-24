#pragma once

#include <AssetFolders.hpp>
#include <AudioDevice.hpp>
#include <Mixer.hpp>
#include <SoundBank.hpp>

#include <map>
#include <string>

// The night's sounds (Play.md "Sounds"): sounds.csv names a file and a volume for each cue Play
// gives (step, dice, door, item, hit, death, bell, turn, click). A missing file is silent.
class Sounds final
{
public:
    explicit Sounds(const umbra::assets::AssetFolders &assets);
    void Play(const std::string &cue);
    void SetVolume(float volume) { volume_ = volume; } // the player's effects volume, 0..1, times each cue's
    // The place's music (sounds.csv row "music:<place>"), crossfading over `fade` seconds; a place with
    // none, or a missing file, fades the music out. The player's music volume, 0..1, applies at once.
    void Music(const std::string &place, float fade = 1.5F);
    void SetMusicVolume(float volume);

private:
    struct Cue
    {
        std::string file;
        float volume = 1.0F;
    };
    umbra::audio::Mixer mixer_; // outlives the device (declared first)
    umbra::audio::SoundBank bank_;
    umbra::platform::AudioDevice device_;
    std::map<std::string, Cue> cues_;
    float volume_ = 1.0F, musicVolume_ = 0.7F;
    std::string place_ = "-"; // the place whose music plays ("" silence)
};
