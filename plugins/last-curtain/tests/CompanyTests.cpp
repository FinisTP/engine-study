#include "NightTestData.hpp"

#include <algorithm>

using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    NightDef Quiet()
    {
        NightDef def = Data();
        def.rules.monsters.chancePercent = 0;
        return def;
    }

    bool Has(const std::vector<Action> &legal, const Action &action)
    {
        return std::find(legal.begin(), legal.end(), action) != legal.end();
    }


}

namespace
{
    // Guest 0 is the player and acts now; everyone stands in the foyer.
    NightState PlayerFirst(const NightDef &def, std::uint64_t seed = 1)
    {
        NightState state = FoyerNight(def, seed, 0);
        while (Current(state) != 0)
            REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
        return state;
    }

}

TEST_CASE("Chatting asks one of the guest's questions and uses the turn (T107)", "[night][company]")
{
    const NightDef def = Quiet();
    NightState state = PlayerFirst(def);
    CHECK(Has(LegalActions(def, state), {ActionKind::Chat, 1}));
    REQUIRE(Apply(def, state, {ActionKind::Chat, 1}));
    REQUIRE(state.chat.active);
    CHECK(state.chat.asker == 1);
    CHECK(def.questions[U(state.chat.question)].guest == 1);
    CHECK(Current(state) == 0);
    const auto legal = LegalActions(def, state);
    CHECK(legal == std::vector<Action>{{ActionKind::Answer, 0}, {ActionKind::Answer, 1}});
    REQUIRE(Apply(def, state, {ActionKind::Answer, 1}));
    CHECK_FALSE(state.chat.active);
    CHECK(Current(state) != 0); // the chat used the turn

    // Nobody chats with someone in another room.
    NightState apart = PlayerFirst(def);
    Closet(def, apart, 0);
    apart.guests[1].cell = 0;
    CHECK_FALSE(Has(LegalActions(def, apart), {ActionKind::Chat, 1}));
}

TEST_CASE("The bond moves an answer's target (T107)", "[night][company]")
{
    const NightDef def = Quiet();
    NightState state = PlayerFirst(def);
    REQUIRE(Apply(def, state, {ActionKind::Chat, 1}));
    const QuestionDef &q = def.questions[U(state.chat.question)];
    state.bonds[U(1 * maxGuests + 0)] = 300;
    CHECK(AnswerTarget(def, state, 0) == q.physicalTarget - def.rules.persuade.bond);
    CHECK(AnswerTarget(def, state, 1) == q.magicalTarget - def.rules.persuade.bond);
    state.bonds[U(1 * maxGuests + 0)] = -300;
    CHECK(AnswerTarget(def, state, 0) == q.physicalTarget + def.rules.persuade.bond);
    state.bonds[U(1 * maxGuests + 0)] = 0;
    CHECK(AnswerTarget(def, state, 1) == q.magicalTarget);
}

TEST_CASE("Questions are not asked twice while unasked ones remain (T107)", "[night][company]")
{
    const NightDef def = Quiet();
    NightState state = PlayerFirst(def);
    std::vector<int> asked;
    for (int talk = 0; talk < 5; ++talk)
    {
        state.guests[0].group = -1, state.guests[1].group = -1, state.approvals[1 * maxGuests + 0] = 0; // never join
        REQUIRE(Apply(def, state, {ActionKind::Chat, 1}));
        asked.push_back(state.chat.question);
        REQUIRE(Apply(def, state, {ActionKind::Answer, 0}));
        while (Current(state) != 0)
            REQUIRE(Apply(def, state, Current(state) >= 0 && LegalActions(def, state)[0].kind == ActionKind::Answer ? Action{ActionKind::Answer, 0}
                                                                                                                  : Action{ActionKind::EndTurn}));
    }
    std::sort(asked.begin(), asked.end());
    CHECK(std::adjacent_find(asked.begin(), asked.end()) == asked.end());
}

TEST_CASE("A bot may open a chat; it does not use the player's turn (T107)", "[night][company]")
{
    NightDef def = Quiet();
    for (StyleDef &s : def.styles)
        s.chatty = 10;
    NightState state = FoyerNight(def, 1, 0);
    while (Current(state) == 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const int bot = Current(state);
    REQUIRE(Has(LegalActions(def, state), {ActionKind::OpenChat, 0}));
    REQUIRE(Apply(def, state, {ActionKind::OpenChat, 0}));
    CHECK(state.chat.asker == bot);
    CHECK(Current(state) == 0);
    REQUIRE(Apply(def, state, {ActionKind::Answer, 0}));
    CHECK(Current(state) == bot); // the bot's turn goes on
    CHECK_FALSE(Has(LegalActions(def, state), {ActionKind::OpenChat, 0})); // once a turn
}

TEST_CASE("A chance is the share of d20 rolls that reach the target (T107)", "[night][company]")
{
    CHECK(ChancePercent(3, 10) == 70);
    CHECK(ChancePercent(6, 16) == 55);
    CHECK(ChancePercent(0, 1) == 100);
    CHECK(ChancePercent(0, 21) == 0);
    CHECK(ChancePercent(25, 13) == 100);
}

namespace
{
    // Every question is passed with any answer.
    NightDef EasyTalk()
    {
        NightDef def = Data();
        def.rules.monsters.chancePercent = 0;
        for (QuestionDef &q : def.questions)
            q.physicalTarget = q.magicalTarget = -20;
        return def;
    }

    // Plays EndTurn until it is guest g's board turn.
    void ToTurnOf(const NightDef &def, NightState &state, int g)
    {
        while (Current(state) != g)
            REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    }

}

TEST_CASE("Both bots learn each other's goal on a pass (T120)", "[night][company]")
{
    const NightDef def = EasyTalk();
    NightState state = FoyerNight(def, 6);
    const int g = state.order[0], o = state.order[1];
    ToTurnOf(def, state, g);
    REQUIRE(Apply(def, state, {ActionKind::Chat, static_cast<std::int8_t>(o)}));
    REQUIRE(Apply(def, state, {ActionKind::Answer, 1}));
    CHECK((state.guests[static_cast<std::size_t>(g)].knows >> o) & 1u);
    CHECK((state.guests[static_cast<std::size_t>(o)].knows >> g) & 1u);
}

TEST_CASE("A bot never recruits the player through Chat (final review)", "[night][company]")
{
    const NightDef def = EasyTalk();
    NightState state = FoyerNight(def, 6, 0);
    const int g = state.order[1] == 0 ? state.order[2] : state.order[1];
    ToTurnOf(def, state, g);
    const auto legal = LegalActions(def, state);
    CHECK(std::none_of(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Chat && a.a == 0; }));
    CHECK(std::find(legal.begin(), legal.end(), Action{ActionKind::OpenChat, 0}) != legal.end()); // it asks the player instead
}

