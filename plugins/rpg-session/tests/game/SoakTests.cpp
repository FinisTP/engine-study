#include "RpgTestContent.hpp"
#include "assets/TestFiles.hpp"

#include <GameSession.hpp>
#include <SaveGame.hpp>

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/constants.hpp>

using namespace umbra;
using game::Phase;

// Long-run check for the release gate (P9, T74): many fights in both modes with a save, a load
// and a comparison after every one; now and then the save file is damaged and must be rescued
// from its backup. Nothing may drift: same state after a save round trip, no leftover fight
// state, files stay small.
namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }
    constexpr float frame = 1.0F / 60.0F;

    // Everyone on the conventional AI until the fight ends.
    std::optional<game::FightResult> Fight(game::GameSession &s)
    {
        for (int step = 0; step < 60 * 120 && s.GetPhase() == Phase::Fighting; ++step)
        {
            if (auto result = s.CheckFightEnd())
                return result;
            if (turn::TurnEncounter *turnFight = s.Combat().Turn())
            {
                rpg::ActionIntent intent = turn::ChooseTurnAction(C(), *turnFight);
                intent.id = s.NextDecisionId();
                if (s.Combat().Submit(s.FightId(), intent) != rpg::Rejection::None)
                    return std::nullopt;
                continue;
            }
            realtime::RealtimeEncounter &live = *s.Combat().Realtime();
            for (std::size_t i = 0; i < live.GetRoster().size(); ++i)
            {
                realtime::Decision d = realtime::ChooseRealtimeAction(C(), live, i);
                live.SetPlacement(i, live.GetPosition(i) + d.move * 3.0F * frame, d.yaw);
                if (d.act)
                {
                    d.act->id = s.NextDecisionId();
                    (void)s.Combat().Submit(s.FightId(), *d.act);
                }
            }
            s.Combat().Update(frame);
        }
        return s.CheckFightEnd();
    }
}

TEST_CASE("Long run: 300 fights in both modes with save, load and damaged-save rescue (T74)", "[soak]")
{
    const std::filesystem::path folder = test::TempDir("soak");
    const game::SaveSlots slots{folder};
    game::GameSession s(C(), 2026);
    rpg::GrantXp(C(), s.Hero(), rpg::XpForLevel(6)); // strong enough to win most fights
    std::vector<core::Diagnostic> diagnostics;
    int won = 0, lost = 0, realtimeFights = 0, rescues = 0;

    // A first save, so a lost fight can always load one.
    REQUIRE(game::WriteSave(slots.Manual(), *s.MakeSave("scenes/dungeon.scene.json", {}, 0.0F), diagnostics));
    for (int cycle = 0; cycle < 300; ++cycle)
    {
        INFO("cycle " << cycle);
        // 1. Rested (full health and energy, like after a save stone), then every third fight in
        //    real time; groups of 1-3 from the dungeon pool.
        const rpg::StatBlock stats = rpg::DerivedStats(C(), s.Hero());
        s.Hero().health = stats[static_cast<std::size_t>(rpg::Stat::MaxHealth)];
        s.Hero().energy = stats[static_cast<std::size_t>(rpg::Stat::MaxEnergy)];
        if ((s.Mode() == game::CombatMode::Realtime) != (cycle % 3 == 2))
            REQUIRE(s.ToggleMode());
        realtimeFights += s.Mode() == game::CombatMode::Realtime;
        REQUIRE(s.StartFight(s.PickGroup("bandit", {"bandit", "wolf"}, 3)));
        const std::optional<game::FightResult> result = Fight(s);
        REQUIRE(result);
        REQUIRE(s.GetPhase() != Phase::Fighting);
        REQUIRE(s.Enemies().empty());

        // 2. A lost fight: load the last save (what the death menu does).
        if (!result->won)
        {
            ++lost;
            game::SaveData saved;
            REQUIRE(game::ReadSaveOrBackup(slots.Manual(), C(), saved, diagnostics));
            s.ApplySave(saved);
            REQUIRE(s.GetPhase() == Phase::Exploring);
            continue;
        }
        ++won;
        REQUIRE(s.Hero().statuses.empty()); // nothing carries over out of a fight

        // 3. Save, load, apply, save again: the two files must be identical.
        REQUIRE(game::WriteSave(slots.Manual(), *s.MakeSave("scenes/dungeon.scene.json", {1.0F, 0.0F, 2.0F}, 0.5F), diagnostics));
        const std::string first = test::ReadText(slots.Manual());
        game::SaveData loaded;
        REQUIRE(game::ReadSave(slots.Manual(), C(), loaded, diagnostics));
        s.ApplySave(loaded);
        REQUIRE(game::WriteSave(slots.Autosave(), *s.MakeSave("scenes/dungeon.scene.json", {1.0F, 0.0F, 2.0F}, 0.5F), diagnostics));
        REQUIRE(test::ReadText(slots.Autosave()) == first);
        REQUIRE(first.size() < 64 * 1024); // the save doesn't grow without bound

        // 4. Every 25th cycle the save is cut short (a crash while copying): the backup rescues it.
        if (cycle % 25 == 24)
        {
            test::WriteText(slots.Manual(), first.substr(0, first.size() / 2));
            game::SaveData rescued;
            std::vector<core::Diagnostic> rescue;
            REQUIRE(game::ReadSaveOrBackup(slots.Manual(), C(), rescued, rescue));
            REQUIRE_FALSE(rescue.empty()); // a warning says the backup was used
            REQUIRE(game::WriteSave(slots.Manual(), rescued, diagnostics)); // and saving works again
            ++rescues;
        }
    }
    INFO("won " << won << ", lost " << lost);
    CHECK(won + lost == 300);
    CHECK(won > 150);
    CHECK(realtimeFights >= 100);
    CHECK(rescues >= 1);
}
