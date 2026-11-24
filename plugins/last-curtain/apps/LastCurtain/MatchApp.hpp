#pragma once

#include "DialogApp.hpp"

#include <AssetFolders.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <Match.hpp>

// LastCurtain's faction match (MatchPlay.md): pick, match, end, pick again. seed 0 = from the clock;
// watch: both sides are bots from the start; capture: save the frame after `frames` and quit;
// zoom: wheel steps at the start (a capture of the room view).
int RunMatch(const umbra::assets::AssetFolders &assets, std::uint64_t seed, bool watch, const std::filesystem::path &capture, int frames, float zoom = 0.0F);

struct GameContext;
class MatchPlay;
namespace umbra::cafe { struct Career; }

// A campaign stage in place of the pick screen (StageApp.hpp): a fresh fight (again on "again"), who
// plays the other side, and a call each frame (the stage's scenes).
struct MatchHooks
{
    std::function<umbra::match::MatchState()> start;
    std::function<umbra::match::Action(const umbra::match::MatchState &)> brain;
    std::function<void(MatchPlay &, const SceneBehind &)> beat; // scenes are played over the board (`behind`)
    std::function<void(MatchPlay &)> setup; // after each start: names, the stage's title
    std::function<bool()> done;              // true: the flow takes over (RunMatch returns 3)
    std::string music = "fight";             // the board's theme; "<music>_battle" (more bass) in the battle view
};
// The same in a shared window, between café days: with a career, the player's side starts from it,
// the end writes it back (money = score * moneyPerEssence) and returns 2 (back to the café); 0 is quit.
int RunMatch(GameContext &game, const umbra::assets::AssetFolders &assets, std::uint64_t seed, bool watch, const std::filesystem::path &capture,
             int frames, float zoom, umbra::cafe::Career *career, int moneyPerEssence, const std::filesystem::path &careerFile,
             const MatchHooks *hooks = nullptr);
