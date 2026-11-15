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

    Mood NightMood(const NightDef &def, const NightState &state)
    {
        Mood mood;
        const float turn = static_cast<float>(def.rules.clock.turnAfterRound), rounds = static_cast<float>(def.rules.clock.rounds);
        const float round = static_cast<float>(state.round);
        // 1. The creep: round 1 is plain, the round before midnight is the full creep.
        mood.creep = state.taken.present ? 1.0F : std::clamp((round - 1.0F) / std::max(1.0F, turn - 1.0F), 0.0F, 1.0F);
        mood.flicker = 0.05F + 0.3F * mood.creep;
        mood.post.saturation = 1.0F - 0.45F * mood.creep;
        mood.post.contrast = 1.0F + 0.25F * mood.creep;
        mood.post.temperature = -0.45F * mood.creep;
        mood.post.vignette = 0.25F + 0.4F * mood.creep;
        mood.post.exposure = -0.35F * mood.creep;
        if (!state.taken.present)
            return mood;
        // 2. The snap: blood from the Turn on, spreading to dawn; the blood grade; heavy corners.
        mood.blood = std::clamp(0.35F + 0.65F * (round - turn - 1.0F) / std::max(1.0F, rounds - turn - 1.0F), 0.35F, 1.0F);
        mood.bloodGrade = true;
        mood.flicker = 0.45F;
        mood.post.saturation = 0.8F;
        mood.post.contrast = 1.35F;
        mood.post.temperature = 0.15F;
        mood.post.vignette = 0.75F + 0.15F * mood.blood;
        mood.post.exposure = -0.3F;
        return mood;
    }

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
        for (const char *ui : {"panel", "paper", "title", "drips", "blood_lut"})
            need("ui", ui);
        return missing;
    }
}
