#include "RpgTestContent.hpp"

#include <Conversation.hpp>
#include <Inventory.hpp>

using namespace umbra;
using rpgtest::Parse;
using story::ChoiceResult;

namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }

    std::vector<std::string> Texts(const story::Conversation &talk, const story::QuestLog &quests, const rpg::Character &hero)
    {
        std::vector<std::string> texts;
        for (const std::size_t i : talk.VisibleChoices(quests, hero))
            texts.push_back(talk.Node().choices[i].text);
        return texts;
    }

    // Index of the visible choice with this text (fails the test if hidden).
    std::size_t Pick(const story::Conversation &talk, const story::QuestLog &quests, const rpg::Character &hero, const std::string &text)
    {
        for (const std::size_t i : talk.VisibleChoices(quests, hero))
        {
            if (talk.Node().choices[i].text == text)
                return i;
        }
        FAIL("choice not visible: " << text);
        return 0;
    }
}

TEST_CASE("Quests and conversations load; broken links and references are rejected")
{
    REQUIRE(C().FindQuest("sun_relic")->objectives.size() == 3);
    REQUIRE(C().FindQuest("sun_relic")->failOnDeath);
    REQUIRE(C().FindConversation("elder_mara")->nodes.size() == 6);
    REQUIRE(C().FindActor("hero")->growth[0] == 10);

    REQUIRE(Parse(R"("conversations":[{"id":"c","npc":"n","start":"a","nodes":[{"id":"a","choices":[{"text":"x","next":"gone"}]}]}])").Has("conversation.next"));
    REQUIRE(Parse(R"("conversations":[{"id":"c","npc":"n","start":"zz","nodes":[{"id":"a"}]}])").Has("conversation.start"));
    REQUIRE(Parse(R"("conversations":[{"id":"c","start":"a","nodes":[{"id":"a"}]}])").Has("conversation.npc"));
    REQUIRE(Parse(R"("conversations":[{"id":"c","npc":"n","start":"a","nodes":[{"id":"a","choices":[{"text":"x","do":[{"startQuest":"nope"}]}]}]}])").Has("ref.quest"));
    REQUIRE(Parse(R"("quests":[{"id":"q"}])").Has("quest.objectives"));
    REQUIRE(Parse(R"("quests":[{"id":"q","objectives":[{"defeat":"dragon"}]}])").Has("ref.actor"));
    REQUIRE(Parse(R"("quests":[{"id":"q","objectives":[{"talk":"a"}],"failOn":["timeout"]}])").Has("quest.failOn"));
}

TEST_CASE("Choices depend on quest state and items; accepting starts the quest")
{
    rpg::Character hero = rpgtest::Make("hero", "hero");
    story::QuestLog quests(C());
    std::vector<story::StoryEvent> events;

    story::Conversation talk(C(), *C().FindConversation("elder_mara"));
    REQUIRE(Texts(talk, quests, hero) == std::vector<std::string>{"I'll get it back.", "Goodbye."});
    REQUIRE(talk.Choose(Pick(talk, quests, hero, "I'll get it back."), quests, hero, events) == ChoiceResult::Done);
    REQUIRE(quests.GetState("sun_relic") == rpg::QuestState::Active);
    REQUIRE(events.front().kind == story::StoryEvent::Kind::QuestStarted);
    REQUIRE(talk.Node().speaker == "Elder Mara"); // the "accepted" line
    REQUIRE(talk.VisibleChoices(quests, hero).empty());
    talk.Close();
    REQUIRE(talk.IsOver());

    // Talking again: no second quest start, the active-quest line instead.
    story::Conversation again(C(), *C().FindConversation("elder_mara"));
    REQUIRE(Texts(again, quests, hero) == std::vector<std::string>{"Where are the bandits again?", "Goodbye."});
    REQUIRE(rpg::AddItem(C(), hero, "sun_relic", 1));
    REQUIRE(Texts(again, quests, hero).front() == "I have the relic.");
}

TEST_CASE("A choice whose actions can't all happen changes nothing")
{
    const auto p = Parse(R"("items":[{"id":"rock","stack":1}],
        "actors":[{"id":"a","stats":{"maxHealth":5},"inventorySlots":1}],
        "quests":[{"id":"q","objectives":[{"talk":"n"}]}],
        "conversations":[{"id":"c","npc":"n","start":"a","nodes":[{"id":"a","choices":[
            {"text":"take two","do":[{"startQuest":"q"},{"give":"rock","count":2}]},
            {"text":"pay","do":[{"take":"rock"}]}]}]}])");
    REQUIRE(p.ok);
    rpg::Character hero = *rpg::MakeCharacter(p.content, "a", "a");
    story::QuestLog quests(p.content);
    std::vector<story::StoryEvent> events;
    story::Conversation talk(p.content, *p.content.FindConversation("c"));

    REQUIRE(talk.Choose(0, quests, hero, events) == ChoiceResult::NoRoom); // second rock has no slot
    REQUIRE(quests.GetState("q") == rpg::QuestState::NotStarted);          // quest start rolled back
    REQUIRE(hero.inventory.empty());
    REQUIRE(events.empty());
    REQUIRE(talk.Choose(1, quests, hero, events) == ChoiceResult::MissingItem);
    REQUIRE(talk.Choose(7, quests, hero, events) == ChoiceResult::Hidden);
    REQUIRE_FALSE(talk.IsOver()); // still at the same node
}

TEST_CASE("Walking away mid-conversation changes nothing")
{
    rpg::Character hero = rpgtest::Make("hero", "hero");
    story::QuestLog quests(C());
    story::Conversation talk(C(), *C().FindConversation("elder_mara"));
    talk.Cancel();
    REQUIRE(talk.IsOver());
    REQUIRE(quests.All().empty());
}
