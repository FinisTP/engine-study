#pragma once

#include <Diagnostic.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace umbra::authoring
{
    // One way the editor's Play (F5) can start the game: project.json "play" (Authoring.md "Play").
    struct PlayStart
    {
        std::string label, program; // program: a name in the build's programs file
        std::vector<std::string> args;
    };

    // The project's starts; with none, one start: the game ("Umbra") on the open scene.
    std::vector<PlayStart> LoadPlayStarts(const nlohmann::json &project, std::vector<core::Diagnostic> &diagnostics);

    // The program's path, then the arguments with {scene} filled; empty if the build lacks the program.
    std::vector<std::string> PlayCommand(const PlayStart &start, const nlohmann::json &programs, const std::string &scene);
}
