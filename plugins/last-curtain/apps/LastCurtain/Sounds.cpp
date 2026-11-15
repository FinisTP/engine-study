#include "Sounds.hpp"

#include <Log.hpp>

#include <cstdlib>
#include <fstream>

Sounds::Sounds(const umbra::assets::AssetFolders &assets) : bank_(assets)
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
    if (it == cues_.end() || !device_.IsOpen())
        return;
    mixer_.Play(bank_.Get(it->second.file), it->second.volume);
    for (const umbra::core::Diagnostic &problem : bank_.TakeDiagnostics()) // once per missing file
        umbra::diagnostics::Error(umbra::core::Format(problem));
}
