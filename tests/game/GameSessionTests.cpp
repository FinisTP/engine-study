#include "RpgTestContent.hpp"
#include "../assets/TestFiles.hpp"

#include <GameSession.hpp>
#include <Inventory.hpp>

#include <glm/gtc/constants.hpp>

using namespace umbra;
using game::Phase;

namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }
    constexpr float frame = 1.0F / 60.0F;

    void SayLine(game::GameSession &s, const std::string &text)
    {
        story::Conversation &talk = *s.CurrentConversation();
        for (const std::size_t i : talk.VisibleChoices(s.Quests(), s.Hero()))
        {
            if (talk.Node().choices[i].text == text)
            {
                REQUIRE(s.Choose(i) == story::ChoiceResult::Done);
                if (s.CurrentConversation() && s.CurrentConversation()->VisibleChoices(s.Quests(), s.Hero()).empty())
                    s.EndTalk(); // read the last line
                return;
            }
        }
        FAIL("not visible: " << text);
    }

    // Turn fight with everyone (hero included) on the conventional AI.
    game::FightResult PlayTurnFight(game::GameSession &s)
    {
        for (int i = 0; i < 1000 && s.GetPhase() == Phase::Fighting; ++i)
        {
            if (auto result = s.CheckFightEnd())
                return *result;
            turn::TurnEncounter &fight = *s.Combat().Turn();
            rpg::ActionIntent intent = turn::ChooseTurnAction(C(), fight);
            intent.id = s.NextDecisionId();
            REQUIRE(s.Combat().Submit(s.FightId(), intent) == rpg::Rejection::None);
        }
        auto result = s.CheckFightEnd();
        REQUIRE(result);
        return *result;
    }
}

TEST_CASE("The reference journey through the session: accept, win, return, reward")
{
    game::GameSession s(C(), 1);
    REQUIRE(s.Talk("elder_mara"));
    REQUIRE(s.GetPhase() == Phase::Talking);
    SayLine(s, "I'll get it back.");
    REQUIRE(s.GetPhase() == Phase::Exploring);
    REQUIRE(s.Quests().GetState("sun_relic") == rpg::QuestState::Active);

    rpg::GrantXp(C(), s.Hero(), rpg::XpForLevel(6)); // strong enough for the leader alone
    REQUIRE(s.StartFight({"bandit_leader"}));
    REQUIRE_FALSE(s.ToggleMode()); // no mode change inside combat
    const game::FightResult fight = PlayTurnFight(s);
    REQUIRE(fight.won);
    REQUIRE(rpg::CountItem(s.Hero(), "sun_relic") == 1);
    REQUIRE(s.Quests().Get("sun_relic")->objective == 2);
    REQUIRE(s.Hero().statuses.empty());

    REQUIRE(s.Talk("elder_mara"));
    SayLine(s, "I have the relic.");
    REQUIRE(s.Quests().GetState("sun_relic") == rpg::QuestState::Completed);
    REQUIRE(rpg::CountItem(s.Hero(), "iron_sword") == 1);
    REQUIRE_FALSE(s.TakeQuestUpdates().empty());
}

TEST_CASE("Real-time mode is chosen outside combat and runs the real-time controller")
{
    game::GameSession s(C(), 2);
    REQUIRE(s.ToggleMode());
    REQUIRE(s.Mode() == game::CombatMode::Realtime);
    REQUIRE(s.StartFight({"wolf"}));
    REQUIRE(s.Combat().Turn() == nullptr);
    realtime::RealtimeEncounter &fight = *s.Combat().Realtime();
    fight.SetPlacement(1, {0.0F, -3.0F}, glm::pi<float>());
    std::optional<game::FightResult> result;
    for (int f = 0; f < 60 * 60 && !result; ++f)
    {
        for (std::size_t i = 0; i < 2; ++i)
        {
            realtime::Decision d = realtime::ChooseRealtimeAction(C(), fight, i);
            fight.SetPlacement(i, fight.GetPosition(i) + d.move * 3.0F * frame, d.yaw);
            if (d.act)
            {
                d.act->id = s.NextDecisionId();
                (void)s.Combat().Submit(s.FightId(), *d.act);
            }
        }
        s.Combat().Update(frame);
        result = s.CheckFightEnd();
    }
    REQUIRE(result);
    REQUIRE(result->won);
    REQUIRE(s.Hero().xp == 15);
}

TEST_CASE("Fleeing ends the fight with no reward; enemies come from the area's pool")
{
    game::GameSession s(C(), 3);
    REQUIRE(s.StartFight({"bandit"}));
    REQUIRE(s.Flee());
    REQUIRE(s.GetPhase() == Phase::Exploring);
    REQUIRE(s.Hero().xp == 0);
    REQUIRE(s.Enemies().empty());
    REQUIRE_FALSE(s.CheckFightEnd());

    // Groups: the touched enemy first, 1 to 3 in total, all from the pool; repeatable per seed.
    game::GameSession a(C(), 42), b(C(), 42);
    for (int i = 0; i < 20; ++i)
    {
        const auto group = a.PickGroup("wolf", {"wolf", "bandit"}, 3);
        REQUIRE(group == b.PickGroup("wolf", {"wolf", "bandit"}, 3));
        REQUIRE(group.front() == "wolf");
        REQUIRE(group.size() >= 1);
        REQUIRE(group.size() <= 3);
    }
    REQUIRE(a.PickGroup("bandit_leader", {"bandit_leader"}, 1).size() == 1);
}

TEST_CASE("Dying fails the quest; loading a save brings the saved state back")
{
    const auto dir = test::TempDir("session-death");
    game::GameSession s(C(), 4);
    s.Quests().Start("sun_relic");
    REQUIRE_FALSE(s.ToggleMode() == false);
    s.ToggleMode(); // back to turn-based
    const auto save = s.MakeSave("scenes/dungeon.scene.json", {0, 0, 9}, 0.0F);
    REQUIRE(save);
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(game::WriteSave(game::SaveSlots{dir}.Autosave(), *save, diagnostics));

    s.Hero().health = 1;
    REQUIRE(s.StartFight({"bandit_leader"}));
    REQUIRE_FALSE(s.MakeSave("x", {}, 0.0F)); // no saving mid-fight
    const game::FightResult fight = PlayTurnFight(s);
    REQUIRE_FALSE(fight.won);
    REQUIRE(s.GetPhase() == Phase::Dead);
    REQUIRE(s.Quests().GetState("sun_relic") == rpg::QuestState::Failed);

    // "Load last autosave": everything as it was when the area was entered.
    game::SaveData loaded;
    REQUIRE(game::ReadSaveOrBackup(game::SaveSlots{dir}.Autosave(), C(), loaded, diagnostics));
    s.ApplySave(loaded);
    REQUIRE(s.GetPhase() == Phase::Exploring);
    REQUIRE(s.Quests().GetState("sun_relic") == rpg::QuestState::Active);
    REQUIRE(s.Hero().health == 110);
}

TEST_CASE("Retrying after a load never pays a reward twice")
{
    game::GameSession s(C(), 5);
    s.Quests().Start("sun_relic");
    rpg::GrantXp(C(), s.Hero(), rpg::XpForLevel(6));
    const auto beforeFight = *s.MakeSave("scenes/dungeon.scene.json", {}, 0.0F);
    REQUIRE(s.StartFight({"bandit_leader"}));
    REQUIRE(PlayTurnFight(s).won);
    const int xpAfter = s.Hero().xp;

    s.ApplySave(beforeFight); // reload: the fight never happened
    REQUIRE(rpg::CountItem(s.Hero(), "sun_relic") == 0);
    REQUIRE(s.StartFight({"bandit_leader"}));
    REQUIRE(PlayTurnFight(s).won);
    REQUIRE(s.Hero().xp == xpAfter); // the same reward once, not twice
    REQUIRE(rpg::CountItem(s.Hero(), "sun_relic") == 1);
}
