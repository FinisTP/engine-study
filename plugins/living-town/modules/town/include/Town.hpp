#pragma once

#include "TownDef.hpp"

#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

// The living town's changing state and its rules (S21, ADR 0011). Everything is integers and the
// whole state is one value: copy it to fork the world, run the copy forward, throw it away.
// Rules, math and the file formats: Town.md.
namespace umbra::town
{
    struct ResidentState final
    {
        std::int16_t place = 0;
        std::int16_t activity = 0;
        std::int32_t until = 0; // tick the current activity ends
        std::int32_t money = 0;
        std::int32_t xp = 0;
        std::int16_t level = 1;
        std::uint16_t crisis = 0; // bit per need that is at its limit right now
        std::array<std::int32_t, maxNeeds> needs{}; // need * ticksPerDay (so a tick adds a whole number)
    };

    enum class EventKind : std::uint8_t { Started, Paid, Earned, LevelUp, Crisis, Friends, Rivals };

    struct TownEvent final
    {
        std::int32_t tick = 0;
        EventKind kind = EventKind::Started;
        std::int16_t who = -1, other = -1; // residents
        std::int32_t what = 0;             // activity, need or amount (see Town.md)
    };

    // Something worth remembering (S22). `saw` and `knows` have one bit per resident: who was
    // there, and who knows by now (they saw it or were told). `told`: who has told the player.
    enum class FactKind : std::uint8_t { LevelUp, Crisis, Friends, Rivals, FavourKept, FavourSkipped };
    struct Fact final
    {
        std::int32_t tick = 0;
        FactKind kind = FactKind::LevelUp;
        std::int16_t who = -1, other = -1; // residents (a favour: who asked, who was to be visited)
        std::int32_t what = 0;             // level, need or favour
        std::uint64_t saw = 0, knows = 0, told = 0;
    };
    enum class FavourStatus : std::uint8_t { Open, Asked, Kept, Skipped };
    struct FavourState final
    {
        FavourStatus status = FavourStatus::Open;
        std::int32_t due = 0; // tick an asked favour must be kept by
    };

    struct TownState final
    {
        std::vector<Fact> facts;           // the most recent (at most maxFacts), oldest first
        std::vector<std::int16_t> regard;  // per resident: what they think of the player, -1000..1000
        std::vector<FavourState> favours;
        static constexpr std::size_t maxFacts = 128;

        std::uint64_t seed = 0;
        std::uint64_t random = 0; // the town's own random stream
        std::int32_t tick = 0;    // 10-minute steps since day 1, 00:00
        std::int64_t minted = 0;  // coins that entered (wages) minus coins that left (costs with no owner)
        std::vector<ResidentState> residents;
        std::vector<std::int16_t> bonds; // residents x residents, how [a] feels about [b], -1000..1000
        std::vector<TownEvent> events;   // the most recent (at most maxEvents)

        static constexpr std::size_t maxEvents = 512;
        [[nodiscard]] int Day() const { return tick / ticksPerDay + 1; }
        [[nodiscard]] int Hour() const { return tick % ticksPerDay / ticksPerHour; }
        [[nodiscard]] int Minute() const { return tick % ticksPerHour * 10; }
        [[nodiscard]] int Need(std::size_t resident, std::size_t need) const { return residents[resident].needs[need] / ticksPerDay; }
        [[nodiscard]] int Bond(std::size_t a, std::size_t b) const { return bonds[a * residents.size() + b]; }
    };

    // What a resident asks to do next. Players' proxies, the conventional chooser and (S23) the
    // model all go through Legal + Apply: nothing else moves a resident.
    struct Intent final
    {
        int resident = 0;
        int activity = 0;
    };
    enum class Refusal { None, UnknownResident, UnknownActivity, Busy, Closed, CannotPay };

    // A fresh town at day 1, 00:00: everyone at home, resting.
    [[nodiscard]] TownState NewTown(const TownDef &def, std::uint64_t seed);

    [[nodiscard]] Refusal Legal(const TownDef &def, const TownState &state, const Intent &intent);
    [[nodiscard]] std::vector<Intent> LegalIntents(const TownDef &def, const TownState &state, int resident);
    // The rule-based choice: an urgent need first (now, or by the end of the plan), else the
    // schedule, else rest (always legal).
    [[nodiscard]] Intent Choose(const TownDef &def, TownState &state, int resident);
    // Starts the activity now if it is legal; otherwise nothing changes.
    Refusal Apply(const TownDef &def, TownState &state, const Intent &intent);

    // One 10-minute step for everybody. `decide` may answer for a resident who is free (the game
    // gives S23's model proposal here); no answer or an illegal one means Choose decides.
    using Decider = std::optional<Intent> (*)(const TownDef &, const TownState &, int resident, void *user);
    void Tick(const TownDef &def, TownState &state, Decider decide = nullptr, void *user = nullptr);
    void Run(const TownDef &def, TownState &state, int ticks);

    // Memory and talk (S22, Town.md "Memory and rumours").
    // The player talks to a resident: a favour asked or thanked, else the newest thing they know
    // and have not told yet, else a greeting. Changes the state (favours, who told what).
    std::string TalkTo(const TownDef &def, TownState &state, int resident);
    // A fact as a plain sentence, for the chronicle and the tool.
    [[nodiscard]] std::string Sentence(const TownDef &def, const Fact &fact);
    // The same fact as `resident` would say it to the player.
    [[nodiscard]] std::string Says(const TownDef &def, const Fact &fact, int resident);
    // What the player has learned so far: facts someone told them, oldest first.
    [[nodiscard]] std::vector<std::string> Chronicle(const TownDef &def, const TownState &state);

    // One number for the whole state: equal states give equal numbers on every compiler.
    [[nodiscard]] std::uint64_t Fingerprint(const TownState &state);

    // Saves: ids, not indexes, so a town file may gain things later (Town.md "Saves").
    [[nodiscard]] nlohmann::json SaveTown(const TownDef &def, const TownState &state);
    bool LoadTownState(const TownDef &def, const nlohmann::json &saved, TownState &out, std::vector<core::Diagnostic> &diagnostics);
}
