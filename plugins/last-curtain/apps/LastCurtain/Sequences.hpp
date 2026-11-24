#pragma once

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

// How a told line plays out on screen over time (Sequences.md): the camera, effects, a flying effect,
// the hit, a shake, a sound, a bubble, each at its second. Display only: the rules never read it.
struct Cue
{
    float at = 0.0F;        // seconds after the line is told
    std::string what;       // camera, effect, projectile, hit, shake, sound, bubble
    std::string on = "user"; // user (the one acting) or target (the one reached)
    std::string to;         // a projectile's end: user or target
    std::string effect;     // effect name (assets/vfx/<name>.vfx.json)
    std::string sound;      // sound cue
    float seconds = 0.0F;   // a projectile's flight
    float amount = 0.0F;    // a shake's strength
};

struct Sequence
{
    float length = 0.0F; // the line is held this long before the next
    std::vector<Cue> cues; // by time
    [[nodiscard]] bool Has(const std::string &what) const;
};

struct Sequences
{
    std::map<std::string, Sequence> named;
    std::map<std::string, std::string> byKind, byAbility; // log kind or ability id -> sequence name
    // The ability's own sequence, else its log kind's; nullptr: told the plain way.
    [[nodiscard]] const Sequence *For(const std::string &kind, const std::string &ability) const;
    [[nodiscard]] std::vector<std::string> Effects() const; // every effect a cue names
};

// Reads assets/vfx/sequences.json ("lastcurtain.sequences" version 1); false with errors if unusable.
bool LoadSequences(const nlohmann::json &j, Sequences &out, std::vector<std::string> &errors);
