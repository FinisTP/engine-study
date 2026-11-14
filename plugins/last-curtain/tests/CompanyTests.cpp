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
    NightState state = NewNight(def, 1, 0); // all seven in the foyer
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
        NightState state = NewNight(def, seed, 0);
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

    // A bot never chats first through Chat; nobody chats with someone in another room.
    const auto botLegal = LegalActions(def, state);
    CHECK(std::none_of(botLegal.begin(), botLegal.end(), [](const Action &a) { return a.kind == ActionKind::Chat; }));
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
        CHECK(state.guests[1].approvals == talk + 1);
        while (Current(state) != 0)
            REQUIRE(Apply(def, state, Current(state) >= 0 && LegalActions(def, state)[0].kind == ActionKind::Answer ? Action{ActionKind::Answer, 0}
                                                                                                                  : Action{ActionKind::EndTurn}));
    }
    CHECK(state.guests[0].group == 0);
    CHECK(state.guests[1].group == 0);
    // A second ally the same way, then the group is full.
    state.guests[2].approvals = 2;
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
        state.guests[0].group = -1, state.guests[1].group = -1, state.guests[1].approvals = 0; // never join
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
    NightState state = NewNight(def, 1, 0);
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
    NightState state = NewNight(def, 1, 0);
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
    // Every room but the foyer and the under-stage is an omen room with four doors.
    NightDef OmenRooms()
    {
        NightDef def = Quiet();
        for (RoomDef &room : def.rooms)
            if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
                room.kind = RoomKind::Omen, room.doors = 15;
        return def;
    }

    // The current guest walks through a new doorway (into an omen room).
    void WalkIn(const NightDef &def, NightState &state)
    {
        const int g = Current(state);
        for (std::int8_t d = 0; d < 4; ++d)
            if (Doorway(def, state, state.guests[U(g)].cell, d))
            {
                REQUIRE(Apply(def, state, {ActionKind::Move, d}));
                return;
            }
        FAIL("no doorway");
    }
}

TEST_CASE("An omen gives its gift and costs a roll against a rising target (T100)", "[night][omen]")
{
    NightDef def = OmenRooms();
    def.rules.omens.targets = {1, 1, 1}; // a d20 never rolls below 1: no omen turns anyone here
    NightState state = NewNight(def, 3);
    for (std::size_t n = 0; n < 3; ++n)
    {
        const int g = Current(state);
        WalkIn(def, state);
        const auto legal = LegalActions(def, state);
        REQUIRE(legal.size() == 2);
        const Cell &cell = state.cells[U(state.guests[U(g)].cell)];
        REQUIRE(cell.omen >= 0);
        const OmenDef &omen = def.omens[U(cell.omen)];
        const int sanity = Stat(def, state, g, StatKind::Sanity), maxHp = Stat(def, state, g, StatKind::MaxHp);
        const int hp = state.guests[U(g)].hp, might = Stat(def, state, g, StatKind::Might);
        const std::size_t since = state.logCount;
        const int round = state.round;
        REQUIRE(Apply(def, state, {ActionKind::TakeOmen}));
        if (state.taken.present)
            break; // an unlucky roll: the guest turned
        REQUIRE(state.round == round);
        const LogEntry check = LastCheck(state, since);
        CHECK(check.b == def.rules.omens.targets[n] + def.rules.omens.perHour * ((state.round - 1) * 30 / 60));
        CHECK(check.other == 0); // a plain d20: the same odds for everyone
        CHECK(Stat(def, state, g, StatKind::Sanity) == sanity + omen.sanity);
        CHECK(Stat(def, state, g, StatKind::Might) == might + omen.might);
        CHECK(Stat(def, state, g, StatKind::MaxHp) == maxHp + omen.maxhp);
        CHECK(state.guests[U(g)].hp == hp + omen.maxhp);
        CHECK(state.omensTaken == static_cast<int>(n) + 1);
        CHECK(Current(state) != g); // taking ends the turn
    }
}

TEST_CASE("The curse target rises with omens taken and with the hour (T100)", "[night][omen]")
{
    NightDef def = OmenRooms();
    def.rules.omens.targets = {2, 5};
    def.rules.omens.perHour = 3;
    NightState state = NewNight(def, 3);
    CHECK(CurseTarget(def, state) == 2);
    state.omensTaken = 1;
    CHECK(CurseTarget(def, state) == 5);
    state.omensTaken = 4; // the last target repeats
    CHECK(CurseTarget(def, state) == 5);
    state.round = 5; // 21:00, two hours in
    CHECK(CurseTarget(def, state) == 5 + 2 * 3);
}

TEST_CASE("A failed curse roll turns that guest at once and the hunt starts at midnight (T100)", "[night][omen]")
{
    NightDef def = OmenRooms();
    def.rules.omens.targets = {99};
    NightState state = NewNight(def, 3);
    const int taker = Current(state);
    WalkIn(def, state);
    REQUIRE(Apply(def, state, {ActionKind::TakeOmen}));
    REQUIRE(state.taken.present);
    CHECK(state.taken.guest == taker);
    CHECK(state.round == def.rules.clock.turnAfterRound + 1);
    CHECK(Hour(def, state) == 0);
    CHECK(state.guests[U(taker)].cell == state.understage);
    for (int g = 0; g < state.count; ++g)
        if (g != taker)
            CHECK(state.guests[U(g)].cell == state.foyer);
    CHECK(Current(state) == state.order[0]); // the company moves first
    CHECK(state.order[U(state.count - 1)] == taker);
}

TEST_CASE("Leaving an omen leaves it for the next guest (T100)", "[night][omen]")
{
    const NightDef def = OmenRooms();
    NightState state = NewNight(def, 3);
    const int first = Current(state);
    WalkIn(def, state);
    const int cell = state.guests[U(first)].cell;
    const int omen = state.cells[U(cell)].omen;
    REQUIRE(Apply(def, state, {ActionKind::LeaveOmen}));
    CHECK(state.guests[U(first)].omens == 0);
    CHECK(Current(state) != first);
    // The next guest walks into the same room through the passage now open.
    const int next = Current(state);
    std::int8_t way = -1;
    for (std::int8_t d = 0; d < 4; ++d)
        if (Neighbour(def, state.guests[U(next)].cell, d) == cell && Passage(def, state, state.guests[U(next)].cell, d))
            way = d;
    REQUIRE(way >= 0);
    REQUIRE(Apply(def, state, {ActionKind::Move, way}));
    REQUIRE(Apply(def, state, {ActionKind::TakeOmen}));
    CHECK(((state.guests[U(next)].omens >> omen) & 1u) == 1u);
}

