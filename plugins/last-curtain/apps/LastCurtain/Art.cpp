#include "Art.hpp"

#include <algorithm>

namespace umbra::night
{
    std::filesystem::path ArtPath(const std::string &set, const std::string &id)
    {
        const bool painting = set == "rooms" || set == "events" || set == "portraits";
        return std::filesystem::path("art") / set / (id + (painting ? ".jpg" : ".png"));
    }

    std::vector<int> StartCells(const NightDef &def, const NightState &state)
    {
        // The same seed deals the same board and the same start rooms (NewNight).
        const NightState first = NewNight(def, state.seed, state.player);
        std::vector<int> cells;
        for (int g = 0; g < first.count; ++g)
            cells.push_back(first.guests[static_cast<std::size_t>(g)].cell);
        return cells;
    }

    std::vector<bool> FeaturedCells(const NightDef &def, const NightState &state)
    {
        std::vector<bool> featured(maxCells, false);
        // 1. Rooms that matter whatever the night: the foyer, the under-stage, goal rooms.
        for (int c = 0; c < maxCells; ++c)
        {
            const int room = state.cells[static_cast<std::size_t>(c)].room;
            if (room < 0)
                continue;
            const RoomKind kind = def.rooms[static_cast<std::size_t>(room)].kind;
            const bool goal = std::any_of(def.goals.begin(), def.goals.end(), [&](const GoalDef &g) { return g.kind == GoalKind::Room && g.target == room; });
            featured[static_cast<std::size_t>(c)] = kind == RoomKind::Foyer || kind == RoomKind::UnderStage || goal;
        }
        // 2. Tonight's start rooms.
        for (const int cell : StartCells(def, state))
            if (cell >= 0)
                featured[static_cast<std::size_t>(cell)] = true;
        return featured;
    }

    bool Featured(const NightDef &def, const NightState &state, int cell) { return FeaturedCells(def, state)[static_cast<std::size_t>(cell)]; }

    std::vector<std::string> MissingArt(const NightDef &def, const std::filesystem::path &assets)
    {
        std::vector<std::string> missing;
        const auto need = [&](const char *set, const std::string &id) {
            const std::filesystem::path path = ArtPath(set, id);
            if (!std::filesystem::exists(assets / path))
                missing.push_back(path.generic_string());
        };
        for (const RoomDef &r : def.rooms)
            need("rooms", r.id);
        for (const EventDef &e : def.events)
            need("events", e.id);
        for (const GuestDef &g : def.guests)
            need("portraits", g.id);
        for (const ItemDef &i : def.items)
            need("items", i.id);
        for (const MonsterDef &m : def.monsters)
            need("cards", m.id);
        for (const char *ui : {"panel", "paper", "title"})
            need("ui", ui);
        return missing;
    }
}
