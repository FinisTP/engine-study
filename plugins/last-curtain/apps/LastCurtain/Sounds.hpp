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
};
