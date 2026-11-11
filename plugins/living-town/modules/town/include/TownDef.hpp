#pragma once

#include <Diagnostic.hpp>
#include <RpgContent.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// A town as the developer writes it (`town/*.town.json`): needs, places, activities and residents.
// It never changes while the game runs; what changes is town::TownState (Town.hpp). See Town.md.
namespace umbra::town
{
    inline constexpr int ticksPerHour = 6;    // one tick = 10 game minutes
    inline constexpr int ticksPerDay = 144;
    inline constexpr int needFull = 1000;     // a need runs 0 (fine) .. 1000 (cannot wait)
    inline constexpr std::size_t maxNeeds = 8;

    struct NeedDef final
    {
        std::string id, name;
        int risePerDay = 0; // how much it grows in a day with nothing done about it
    };

    struct PlaceDef final
    {
        std::string id, name;
        std::string scene, marker; // where it is in the game world (optional; S21 shows residents there)
        int owner = -1;            // resident who is paid what an activity costs here (-1 = nobody)
    };

    struct ActivityDef final
    {
        std::string id, name;
        std::string where;        // "home", "work" or a place id
        int place = -1;           // the place, when `where` names one
        int hours = 1;            // how long one go lasts
        int openFrom = 0, openTo = 24; // hours of the day it may start in (from > to wraps midnight)
        std::vector<int> relief;  // per need: how much it eases in an hour
        int cost = 0;             // coins per hour, paid to the place's owner
        int wage = 0;             // coins per hour, earned from outside the town
        int xp = 0;               // experience per hour (growth)
        bool social = false;      // people doing this in one place get to know each other
    };

    struct ScheduleEntry final
    {
        int from = 0, to = 24;
        int activity = 0;
    };

    struct ResidentDef final
    {
        std::string id, name;
        std::string actor; // the RPG actor this person is (stats, skills, look)
        std::string voice; // a few words on how they talk ("gruff, few words"), for the model (S23)
        int home = 0, work = 0;
        int money = 0;
        std::vector<ScheduleEntry> schedule; // first matching entry is what they plan to do
        std::vector<std::pair<int, int>> bonds; // starting feelings: {other resident, -1000..1000}
    };

    // Something a resident asks of the player: go and see `target` within `hours` (S22).
    struct FavourDef final
    {
        std::string id;
        int resident = 0, target = 0;
        std::string ask, thanks; // what the asker says; what the target says when the player comes
        int hours = 24;
    };

    struct TownDef final
    {
        std::vector<FavourDef> favours;
        std::vector<NeedDef> needs;
        std::vector<PlaceDef> places;
        std::vector<ActivityDef> activities; // activities[0] is the free fallback (rest at home)
        std::vector<ResidentDef> residents;
        int urgent = 700;              // a need above this is dealt with before the schedule
        std::uint64_t fingerprint = 0; // of every id, in order: a saved town must match it

        [[nodiscard]] int Need(const std::string &id) const;
        [[nodiscard]] int Place(const std::string &id) const;
        [[nodiscard]] int Activity(const std::string &id) const;
        [[nodiscard]] int Resident(const std::string &id) const;
    };

    // Reads and checks a town's text; false with diagnostics (code "town.*") if it can't be used.
    // With `content`, every resident's actor must exist in it.
    bool ParseTown(const std::string &text, const std::string &file, const rpg::Content *content, TownDef &out,
                   std::vector<core::Diagnostic> &diagnostics);
    bool LoadTown(const std::filesystem::path &file, const rpg::Content *content, TownDef &out, std::vector<core::Diagnostic> &diagnostics);
}
