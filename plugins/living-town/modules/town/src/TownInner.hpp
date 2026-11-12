#pragma once

#include "Town.hpp"

// Helpers the town's rule files share (Town.cpp, Trouble.cpp). Not part of the public headers.
namespace umbra::town::inner
{
    inline constexpr int friendsAt = 300, rivalsAt = -300;

    inline std::uint64_t Bit(int resident) { return 1ULL << resident; }
    void Log(TownState &state, EventKind kind, int who, int other, int what);
    // A resident learns a fact (saw it or was told).
    void Learn(TownState &state, Fact &fact, int resident);
    // Something happened: remembered by those named and (if `seen`) everyone at the first one's place.
    void Remember(TownState &state, FactKind kind, int who, int other, int what, bool seen);
    void ChangeBond(const TownDef &def, TownState &state, int a, int b, int amount);
}
