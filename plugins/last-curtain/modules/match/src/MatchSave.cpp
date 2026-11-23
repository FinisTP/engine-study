#include <MatchSave.hpp>

// A faction match as JSON: every field of MatchState, by name (Match.md "Saves").
namespace umbra::match
{
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Cell, kind, doors, event, resolved)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Board, width, height, cells, hub, door, stageRooms, weakDens, strongDens, itemRooms, centreSpawns)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Unit, kind, def, faction, cell, home, hp, maxHp, attack, defence, speed, move, xp, level, size, back,
                                       pending, essence, kills, abilities, items, boost, cut, flags, ap, from)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Floor, cell, item, essence, treasure)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Battle, on, attacker, defender, round, next, fighters, order, shield)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TurnState, origin, movesLeft, acted, apSpent)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Mode, essence, perform, respawn, fightXp, biding, fallsStay, playerLoss, mustFree, win, roundLimit, maxAp, apPerTurn)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LogEntry, kind, a, b, c)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Spawn, cell, monster, round)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RoomSpawn, cell, kind, round)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LevelCard, kind, a, b)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MatchState, mode, board, units, order, orderSeed, turn, round, score, stage, spawns, roomSpawns, floor, cards, offer, battle,
                                       now, log, rng, over, winner, loss, riteReady, riteUsed)

    namespace
    {
        constexpr const char *format = "umbra.match";
        constexpr int version = 4; // 2: spawns, a fixed stage; 3: the tile board, shared squares; 4: modes, falls, AP, flags
    }

    nlohmann::json SaveMatch(const MatchState &s)
    {
        nlohmann::json j = s;
        j["format"] = format;
        j["version"] = version;
        return j;
    }

    bool LoadMatch(const nlohmann::json &j, MatchState &out, std::string &error)
    {
        // 1. The right kind of file, then every field; a missing or wrong one throws, and nothing changes.
        if (!j.is_object() || j.value("format", "") != format)
            return error = "not a faction match save", false;
        if (j.value("version", 0) != version)
            return error = "the save's version " + std::to_string(j.value("version", 0)) + " is not " + std::to_string(version), false;
        try
        {
            out = j.get<MatchState>();
            return true;
        }
        catch (const nlohmann::json::exception &e)
        {
            error = std::string("a broken save: ") + e.what();
            return false;
        }
    }

    std::uint64_t Fingerprint(const MatchState &s)
    {
        // 1. FNV-1a over the JSON of everything but the log.
        nlohmann::json j = s;
        j.erase("log");
        std::uint64_t h = 1469598103934665603ULL;
        for (const char c : j.dump())
            h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
        return h;
    }
}
