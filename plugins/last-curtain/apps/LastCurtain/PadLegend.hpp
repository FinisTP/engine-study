#pragma once

#include <nlohmann/json.hpp>

#include <initializer_list>
#include <string>
#include <utility>

// The pad's prompts (gamepad plan 2026-11-25): a row per button, its PlayStation glyph and what it does
// here. `words` are the screen's words ("padPick"...); `rows` pairs a glyph name (art/ui/pad) with a word.
inline nlohmann::json PadLegend(const nlohmann::json &words, std::initializer_list<std::pair<const char *, const char *>> rows)
{
    nlohmann::json out = nlohmann::json::array();
    for (const auto &[glyph, word] : rows)
        out.push_back({{"glyph", std::string("art/ui/pad/") + glyph + ".png"}, {"text", words.value(word, std::string(word))}});
    return out;
}
