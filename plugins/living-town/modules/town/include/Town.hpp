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
    enum class Gone : std::uint8_t { No, Left, Dead };

    struct ResidentState final
    {
        std::int16_t place = 0;
        std::int16_t activity = 0;
        std::int32_t until = 0; // tick the current activity ends
        std::int32_t money = 0;
        std::int32_t xp = 0;
        std::int16_t level = 1;
        std::uint16_t crisis = 0; // bit per need that is at its limit right now
        Gone gone = Gone::No;     // left town or died: no longer part of anything (S24)
        std::int32_t illUntil = 0; // ill until this tick: stays at home (S24)
        std::array<std::int32_t, maxNeeds> needs{}; // need * ticksPerDay (so a tick adds a whole number)
    };

    enum class EventKind : std::uint8_t { Started, Paid, Earned, LevelUp, Crisis, Friends, Rivals, Trouble }; // Trouble: `what` is its TroubleKind

    struct TownEvent final
    {
        std::int32_t tick = 0;
        EventKind kind = EventKind::Started;
        std::int16_t who = -1, other = -1; // residents
        std::int32_t what = 0;             // activity, need or amount (see Town.md)
    };

    // Something worth remembering (S22). `saw` and `knows` have one bit per resident: who was
    // there, and who knows by now (they saw it or were told). `told`: who has told the player.
    enum class FactKind : std::uint8_t { LevelUp, Crisis, Friends, Rivals, FavourKept, FavourSkipped, Ill, Theft, Feud, Left, Died };
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

    // A line a resident said to the player, as it was said (S23: the model's accepted wording is
    // kept, so a loaded game shows the same words and never asks the model again for them).
    struct SaidLine final
    {
        std::int32_t tick = 0;
        std::int16_t speaker = 0;
        std::string text;
    };

    struct TownState final
    {
        std::vector<SaidLine> said; // the most recent news told to the player (at most maxSaid)
        static constexpr std::size_t maxSaid = 64;
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
    enum class Refusal { None, UnknownResident, UnknownActivity, Busy, Closed, CannotPay, Ill, Gone };

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

    // Trouble (S24, Town.md "Trouble"): the things that can go wrong in a town. The rules never
    // start one themselves: they say what could happen now (Proposals) and what it does (Happen).
    // Who decides is the storyteller (Storyteller.md), or nobody.
    enum class TroubleKind : std::uint8_t { Ill, Theft, Feud, Leave, Death };
    struct Trouble final
    {
        TroubleKind kind = TroubleKind::Ill;
        int who = -1;   // who falls ill, steals, starts the row, leaves or dies
        int other = -1; // theft: from whom; feud: with whom
        bool operator==(const Trouble &) const = default;
    };
    // Can it happen at all (the people are here, there is something to steal)?
    [[nodiscard]] bool Possible(const TownDef &def, const TownState &state, const Trouble &trouble);
    // What the town itself has led up to: the hungry fall ill, the poor steal from those beside
    // them, people who dislike each other row, the unhappy leave, the ill may die.
    [[nodiscard]] std::vector<Trouble> Proposals(const TownDef &def, const TownState &state);
    // How much the town has led up to it (higher = fits better); used to pick a forced one.
    [[nodiscard]] int Likely(const TownDef &def, const TownState &state, const Trouble &trouble);
    // Makes it happen now (false and no change if it is not Possible). Logged and remembered.
    bool Happen(const TownDef &def, TownState &state, const Trouble &trouble);
    [[nodiscard]] std::string Sentence(const TownDef &def, const Trouble &trouble);

    // Memory and talk (S22, Town.md "Memory and rumours").
    // The player talks to a resident: a favour asked or thanked, else the newest thing they know
    // and have not told yet, else a greeting. Changes the state (favours, who told what).
    // `worded`: another wording for the news (step 3), e.g. the model's; empty = the template.
    std::string TalkTo(const TownDef &def, TownState &state, int resident, const std::string &worded = {});
    // What TalkTo would do next, without doing it: the index in `facts` of the news this resident
    // would tell (-1 = they would ask, thank or just greet). Used to have the wording ready.
    [[nodiscard]] int NextNews(const TownDef &def, const TownState &state, int resident);
    // A fact as a plain sentence, for the chronicle and the tool.
    [[nodiscard]] std::string Sentence(const TownDef &def, const Fact &fact);
    // The same fact as `resident` would say it to the player.
    [[nodiscard]] std::string Says(const TownDef &def, const Fact &fact, int resident);
    // What the player has learned so far, oldest first: favours settled in person and what
    // residents said (their words, with who said it).
    [[nodiscard]] std::vector<std::string> Chronicle(const TownDef &def, const TownState &state);

    // One number for the whole state: equal states give equal numbers on every compiler.
    [[nodiscard]] std::uint64_t Fingerprint(const TownState &state);

    // Saves: ids, not indexes, so a town file may gain things later (Town.md "Saves").
    [[nodiscard]] nlohmann::json SaveTown(const TownDef &def, const TownState &state);
    bool LoadTownState(const TownDef &def, const nlohmann::json &saved, TownState &out, std::vector<core::Diagnostic> &diagnostics);
}
