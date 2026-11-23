#pragma once

#include <Diagnostic.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace umbra::campaign
{
    // How the engine tints a character (plan 7).
    enum class Form : std::uint8_t
    {
        Player,
        Ally,
        Strung,
        Ghost,
        Host,
        Zodiac
    };

    // One row of cast.csv.
    struct CastMember
    {
        std::string id, name, sprite, bound; // bound: the sprite while strung ("" = none)
        Form form = Form::Ghost;
        std::string voice;
    };

    struct Cast
    {
        std::vector<CastMember> members;
        const CastMember *Find(const std::string &id) const;
    };

    bool LoadCast(const std::filesystem::path &file, Cast &cast, std::vector<core::Diagnostic> &d);

    enum class Side : std::uint8_t
    {
        Left,
        Right
    };

    // A portrait brought in (or moved) before a line.
    struct Stage
    {
        std::string who;
        Side side = Side::Left;
        int slot = 0; // 0 nearest the middle, 1 behind
        std::string face;
    };

    struct Choice
    {
        std::string text, fact, value, jump;
    };

    struct Line
    {
        std::string id, speaker, face, text, sound, jump; // speaker "" = narration; jump: the next line's id
        std::vector<Stage> enter;
        std::vector<std::string> leave;
        std::vector<Choice> choices; // 0 or 2-4
        bool model = false;          // plan 6: the model may reword it
    };

    struct Scene
    {
        std::string id;
        std::vector<Line> lines;
    };

    // The faces every portrait may have (face_<name>.png).
    bool KnownFace(const std::string &face);

    // Reads and checks a scene; each problem names the file, the line's field and the fix.
    bool LoadScene(const std::filesystem::path &file, const Cast &cast, Scene &scene, std::vector<core::Diagnostic> &d);
}
