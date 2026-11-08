#include "RpgTestContent.hpp"

#include <Conversation.hpp>
#include <Inventory.hpp>

using namespace umbra;
using story::GameEvent;
using story::QuestUpdate;

namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }

    // Picks the visible choice with this text.
    void Say(story::Conversation &talk, story::QuestLog &quests, rpg::Character &hero, const std::string &text,
             std::vector<story::StoryEvent> &events)
    {
        for (const std::size_t i : talk.VisibleChoices(quests, hero))
        {
            if (talk.Node().choices[i].text == text)
            {
                REQUIRE(talk.Choose(i, quests, hero, events) == story::ChoiceResult::Done);
                return;
            }
        }
        FAIL("not visible: " << text);
    }

    // Forwards conversation events to the quest log, as the game session does.
    void Forward(const std::vector<story::StoryEvent> &events, story::QuestLog &quests, rpg::Character &hero)
    {
        for (const story::StoryEvent &e : events)
        {
            if (e.kind == story::StoryEvent::Kind::Talked)
                quests.Notify({GameEvent::Kind::Talked, e.id}, hero);
            else
                quests.Notify({GameEvent::Kind::ItemsChanged, ""}, hero);
        }
    }
}

TEST_CASE("The reference quest from start to reward, with the reward given once")
{
    rpg::Character hero = rpgtest::Make("hero", "hero");
    story::QuestLog quests(C());
    std::vector<story::StoryEvent> events;

    // 1. Accept from Elder Mara.
    story::Conversation talk(C(), *C().FindConversation("elder_mara"));
    Say(talk, quests, hero, "I'll get it back.", events);
    Forward(events, quests, hero);
    events.clear();

    // 2. Talking to the elder now doesn't skip ahead: the current objective is the bandit leader.
    quests.Notify({GameEvent::Kind::Talked, "elder"}, hero);
    REQUIRE(quests.Get("sun_relic")->objective == 0);

    // 3. Win against the leader: the Defeat objective, then the relic he carried completes Obtain.
    const rpg::Character leader = rpgtest::Make("bandit_leader", "leader");
    rpg::AwardVictory(C(), hero, {&leader});
    auto updates = quests.Notify({GameEvent::Kind::Defeated, "bandit_leader"}, hero);
    REQUIRE(updates.front().kind == QuestUpdate::Kind::Progress);
    REQUIRE(quests.Get("sun_relic")->objective == 2); // defeat and obtain done
    const int levelBefore = hero.level;
    const int pointsBefore = hero.skillPoints;

    // 4. Return the relic.
    story::Conversation back(C(), *C().FindConversation("elder_mara"));
    Say(back, quests, hero, "I have the relic.", events);
    Forward(events, quests, hero);
    REQUIRE(quests.GetState("sun_relic") == rpg::QuestState::Completed);
    REQUIRE(rpg::CountItem(hero, "sun_relic") == 0);
    REQUIRE(rpg::CountItem(hero, "iron_sword") == 1);
    REQUIRE(hero.skillPoints > pointsBefore);
    REQUIRE(hero.level > levelBefore);

    // 5. Anything later gives nothing more.
    const int xp = hero.xp;
    quests.Notify({GameEvent::Kind::Talked, "elder"}, hero);
    quests.Notify({GameEvent::Kind::Defeated, "bandit_leader"}, hero);
    REQUIRE(rpg::CountItem(hero, "iron_sword") == 1);
    REQUIRE(hero.xp == xp);
}

TEST_CASE("Events before a quest starts or for a later objective are ignored")
{
    rpg::Character hero = rpgtest::Make("hero", "hero");
    story::QuestLog quests(C());
    REQUIRE(quests.Notify({GameEvent::Kind::Defeated, "bandit_leader"}, hero).empty()); // not started
    REQUIRE(quests.Start("sun_relic"));
    REQUIRE_FALSE(quests.Start("sun_relic"));
    REQUIRE(quests.Get("sun_relic")->objective == 0);

    // Holding the relic early counts once its objective comes up (Obtain looks at the inventory).
    REQUIRE(rpg::AddItem(C(), hero, "sun_relic", 1));
    quests.Notify({GameEvent::Kind::ItemsChanged, ""}, hero);
    REQUIRE(quests.Get("sun_relic")->objective == 0); // still needs the leader
    quests.Notify({GameEvent::Kind::Defeated, "wolf"}, hero);
    REQUIRE(quests.Get("sun_relic")->objective == 0);
    quests.Notify({GameEvent::Kind::Defeated, "bandit_leader"}, hero);
    REQUIRE(quests.Get("sun_relic")->objective == 2);
}

TEST_CASE("Death fails quests that fail on death; failed quests stay failed")
{
    rpg::Character hero = rpgtest::Make("hero", "hero");
    story::QuestLog quests(C());
    quests.Start("sun_relic");
    const auto updates = quests.Notify({GameEvent::Kind::PlayerDied, ""}, hero);
    REQUIRE(updates.size() == 1);
    REQUIRE(updates[0].kind == QuestUpdate::Kind::Failed);
    REQUIRE(quests.GetState("sun_relic") == rpg::QuestState::Failed);
    quests.Notify({GameEvent::Kind::Defeated, "bandit_leader"}, hero);
    REQUIRE(quests.GetState("sun_relic") == rpg::QuestState::Failed);

    story::Conversation talk(C(), *C().FindConversation("elder_mara"));
    REQUIRE(talk.Node().choices[talk.VisibleChoices(quests, hero).front()].text == "About the relic...");
}

TEST_CASE("A quest reward that doesn't fit is kept, not lost")
{
    rpg::Character hero = rpgtest::Make("hero", "hero");
    story::QuestLog quests(C());
    quests.Start("sun_relic");
    quests.Notify({GameEvent::Kind::Defeated, "bandit_leader"}, hero);
    REQUIRE(rpg::AddItem(C(), hero, "sun_relic", 1));
    quests.Notify({GameEvent::Kind::ItemsChanged, ""}, hero);
    hero.inventorySlots = static_cast<int>(hero.inventory.size()); // full
    quests.Notify({GameEvent::Kind::Talked, "elder"}, hero);
    REQUIRE(quests.GetState("sun_relic") == rpg::QuestState::Completed);
    REQUIRE(rpg::CountItem(hero, "iron_sword") == 0);
    REQUIRE(hero.pending.front().item == "iron_sword");
}
