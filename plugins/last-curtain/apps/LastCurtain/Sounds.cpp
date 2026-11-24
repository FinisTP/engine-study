#include "Sounds.hpp"

#include <Log.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>

Sounds::Sounds(const umbra::assets::AssetFolders &assets) : bank_(assets), assets_(assets)
{
    // 1. The cues: cue,file,volume,note (the header skipped; a bad row is left out).
    std::ifstream in(assets / "sounds.csv");
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line))
    {
        const std::size_t a = line.find(','), b = a == std::string::npos ? a : line.find(',', a + 1);
        const std::size_t c = b == std::string::npos ? b : line.find(',', b + 1);
        if (c == std::string::npos)
            continue;
        cues_[line.substr(0, a)] = {line.substr(a + 1, b - a - 1), std::strtof(line.substr(b + 1, c - b - 1).c_str(), nullptr)};
    }
    // 2. The sound card; without one the night is silent.
    (void)device_.Open(mixer_);
}

void Sounds::Play(const std::string &cue)
{
    const auto it = cues_.find(cue);
    if (it == cues_.end() || !device_.IsOpen() || volume_ <= 0.0F)
        return;
    mixer_.Play(bank_.Get(it->second.file), it->second.volume * volume_);
    for (const umbra::core::Diagnostic &problem : bank_.TakeDiagnostics()) // once per missing file
        umbra::diagnostics::Error(umbra::core::Format(problem));
}

void Sounds::Music(const std::string &place, float fade)
{
    if (place == place_ || !device_.IsOpen())
        return;
    place_ = place;
    // A place with no row of its own plays audio/music/<place>.ogg when that file is there (the battle themes, note 17).
    if (const std::string file = "audio/music/" + place + ".ogg"; !place.empty() && !cues_.contains("music:" + place) && std::filesystem::exists(assets_.RootOf(file) / file))
        cues_["music:" + place] = {file, 1.0F};
    const auto it = cues_.find("music:" + place);
    mixer_.PlayMusic(it == cues_.end() ? nullptr : bank_.GetMusic(it->second.file), it == cues_.end() ? 0.0F : it->second.volume * musicVolume_, fade);
    for (const umbra::core::Diagnostic &problem : bank_.TakeDiagnostics())
        umbra::diagnostics::Error(umbra::core::Format(problem));
}

void Sounds::SetMusicVolume(float volume)
{
    musicVolume_ = volume;
    const auto it = cues_.find("music:" + place_);
    if (it != cues_.end() && device_.IsOpen()) // the same track: only its volume changes
        mixer_.PlayMusic(bank_.GetMusic(it->second.file), it->second.volume * musicVolume_, 0.3F);
}

std::string Sounds::MusicOr(const std::string &place, const std::string &fallback) const
{
    const auto it = cues_.find("music:" + place);
    const std::string file = it != cues_.end() ? it->second.file : "audio/music/" + place + ".ogg"; // a row, else the file by its name
    return std::filesystem::exists(assets_.RootOf(file) / file) ? place : fallback;
}
