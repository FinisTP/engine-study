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

    // Everyone but these guests leaves the foyer for a far, dark closet (cell 0 of the grid).
    void OnlyIn(NightState &state, std::initializer_list<int> here)
    {
        Closet(Data(), state, 0);
        for (int g = 0; g < state.count; ++g)
            if (std::find(here.begin(), here.end(), g) == here.end())
                state.guests[U(g)].cell = 0;
    }

    // The last Check logged since `since`.
    LogEntry LastCheck(const NightState &state, std::size_t since)
    {
        LogEntry found{};
        for (const LogEntry &e : NewLog(state, since))
            if (e.kind == LogKind::Check)
                found = e;
        return found;
    }
}

TEST_CASE("No guest fights another before midnight (T99)", "[night][company]")
{
    const NightDef def = Quiet();
    NightState state = FoyerNight(def, 1, 0); // all seven in the foyer
    for (int i = 0; i < 70 && !state.taken.present; ++i)
    {
        for (const Action &a : LegalActions(def, state))
            CHECK_FALSE((a.kind == ActionKind::Attack && a.b >= 0));
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
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

    // Every question easy to pass (target 1), so approvals come every time.
    NightDef EasyQuestions()
    {
        NightDef def = Quiet();
        for (QuestionDef &q : def.questions)
            q.physicalTarget = q.magicalTarget = 1;
        def.rules.persuade.bond = 0;
        return def;
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

TEST_CASE("A passed answer is one approval; three make an ally (T107)", "[night][company]")
{
    const NightDef def = EasyQuestions();
    NightState state = PlayerFirst(def);
    for (int talk = 0; talk < 3; ++talk)
    {
        REQUIRE(Apply(def, state, {ActionKind::Chat, 1}));
        REQUIRE(Apply(def, state, {ActionKind::Answer, 0}));
        CHECK(state.approvals[1 * maxGuests + 0] == talk + 1);
        while (Current(state) != 0)
            REQUIRE(Apply(def, state, Current(state) >= 0 && LegalActions(def, state)[0].kind == ActionKind::Answer ? Action{ActionKind::Answer, 0}
                                                                                                                  : Action{ActionKind::EndTurn}));
    }
    CHECK(state.guests[0].group == 0);
    CHECK(state.guests[1].group == 0);
    // A second ally the same way, then the group is full.
    state.approvals[2 * maxGuests + 0] = 2;
    REQUIRE(Apply(def, state, {ActionKind::Chat, 2}));
    REQUIRE(Apply(def, state, {ActionKind::Answer, 0}));
    CHECK(state.guests[2].group == 0);
    while (Current(state) != 0)
        REQUIRE(Apply(def, state, Current(state) >= 0 && LegalActions(def, state)[0].kind == ActionKind::Answer ? Action{ActionKind::Answer, 0}
                                                                                                              : Action{ActionKind::EndTurn}));
    CHECK_FALSE(Has(LegalActions(def, state), {ActionKind::Chat, 3}));
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

TEST_CASE("Allies fight beside the player and are the player's to move (T99)", "[night][company]")
{
    const NightDef def = Quiet();
    NightState state = FoyerNight(def, 1, 0);
    while (Current(state) != 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const int ally = 1, other = 2;
    state.guests[0].group = state.guests[U(ally)].group = 0;
    CHECK(Controls(state, 0, 0));
    CHECK(Controls(state, 0, ally));
    CHECK_FALSE(Controls(state, 0, other));
    OnlyIn(state, {0, ally});
    state.monsters.push_back({0, state.foyer, 99, 99, 1, 1});
    REQUIRE(Apply(def, state, {ActionKind::Attack, 0}));
    REQUIRE(state.battle.active);
    bool allyBeside = false;
    for (int f = 0; f < state.battle.count; ++f)
        allyBeside = allyBeside || (state.battle.fighters[U(f)].guest == ally && state.battle.fighters[U(f)].side == 0);
    CHECK(allyBeside);
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

    std::size_t Pair(int asker, int answerer) { return static_cast<std::size_t>(asker * maxGuests + answerer); }
}

TEST_CASE("Two bots can chat; three passes bring the other into the opener's group (T120)", "[night][company]")
{
    const NightDef def = EasyTalk();
    NightState state = FoyerNight(def, 6); // no player: all bots
    const int g = state.order[0], o = state.order[1];
    for (int time = 1; time <= def.rules.persuade.approvals; ++time)
    {
        ToTurnOf(def, state, g);
        REQUIRE(Apply(def, state, {ActionKind::Chat, static_cast<std::int8_t>(o)}));
        REQUIRE(Current(state) == g); // the opener answers the other's question
        REQUIRE(Apply(def, state, {ActionKind::Answer, 0}));
        CHECK(state.approvals[Pair(o, g)] == time);
        CHECK(Current(state) != g); // the chat used the turn
    }
    CHECK(state.guests[static_cast<std::size_t>(g)].group == 1 + g);
    CHECK(state.guests[static_cast<std::size_t>(o)].group == 1 + g);
    CHECK(GoalDone(def, state, g) == (def.goals[static_cast<std::size_t>(state.guests[static_cast<std::size_t>(g)].goal)].kind == GoalKind::Ally &&
                                      def.goals[static_cast<std::size_t>(state.guests[static_cast<std::size_t>(g)].goal)].target == o));
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

TEST_CASE("Groups hold at most three (T120)", "[night][company]")
{
    const NightDef def = EasyTalk();
    NightState state = FoyerNight(def, 6);
    const int g = state.order[0];
    ToTurnOf(def, state, g);
    int members = 1;
    state.guests[static_cast<std::size_t>(g)].group = static_cast<std::int8_t>(1 + g);
    for (int o = 0; o < state.count && members < 3; ++o)
        if (o != g)
            state.guests[static_cast<std::size_t>(o)].group = static_cast<std::int8_t>(1 + g), ++members;
    for (const Action &a : LegalActions(def, state))
        CHECK(a.kind != ActionKind::Chat);
}
