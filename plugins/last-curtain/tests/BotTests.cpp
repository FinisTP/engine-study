#include "NightTestData.hpp"

#include <NightBots.hpp>

#include <algorithm>

using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

    // Plays every guest by its bot and calls `seen` with each new log entry.
    template <class F> int PlayWatching(const NightDef &def, NightState &state, F seen)
    {
        int applies = 0;
        while (Current(state) >= 0 && applies < 5000)
        {
            const std::size_t since = state.logCount;
            REQUIRE(Apply(def, state, ChooseAction(def, state, Current(state))));
            ++applies;
            for (const LogEntry &e : NewLog(state, since))
                seen(e);
        }
        return applies;
    }
}

TEST_CASE("Seven bots finish a night on every seed (T104)", "[night][bots]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 300; ++seed)
    {
        NightState state = NewNight(def, seed);
        const int applies = PlayNight(def, state);
        INFO("seed " << seed);
        CHECK(applies < 5000);
        CHECK(Result(state) != Outcome::Running);
        CHECK(state.round <= def.rules.clock.rounds + 1);
    }
}

TEST_CASE("A bot uses only what it could have seen (T104)", "[night][bots]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 30; ++seed)
    {
        NightState a = NewNight(def, seed), b = NewNight(def, seed);
        b.marked = I8(static_cast<int>(seed % 7)); // nobody saw it happen
        while (Current(a) >= 0 && !a.taken.present && a.marked < 0 && Result(b) == Outcome::Running)
        {
            const int g = Current(a);
            REQUIRE(Current(b) == g);
            const Action choice = ChooseAction(def, a, g);
            CHECK(ChooseAction(def, b, g) == choice);
            REQUIRE(Apply(def, a, choice));
            REQUIRE(Apply(def, b, choice));
        }
    }
}

TEST_CASE("A bot that saw the failure hunts; a seen marked bot hides (T104)", "[night][bots]")
{
    NightDef def = Data();
    def.rules.monsters.chancePercent = 0;
    NightState state = NewNight(def, 2);
    // Open the board a little so there is somewhere to go.
    for (int i = 0; i < 40 && Current(state) >= 0 && state.round < 4; ++i)
        REQUIRE(Apply(def, state, ChooseAction(def, state, Current(state))));
    const int hunter = Current(state);
    REQUIRE(hunter >= 0);
    const int marked = (hunter + 1) % 7;
    state.marked = I8(marked);
    state.guests[U(hunter)].sawFail = static_cast<std::uint8_t>(1u << marked);
    state.guests[U(hunter)].sawOmen = static_cast<std::uint8_t>(1u << marked);
    // In the same room: attack.
    state.guests[U(marked)].cell = state.guests[U(hunter)].cell;
    CHECK(ChooseAction(def, state, hunter) == Action{ActionKind::Attack, -1, I8(marked)});
    // Elsewhere but in sight: a step toward.
    NightState apart = state;
    int other = -1;
    for (int d = 0; d < 4 && other < 0; ++d)
        if (Passage(def, apart, apart.guests[U(hunter)].cell, d))
            other = Neighbour(def, apart.guests[U(hunter)].cell, d);
    if (other >= 0 && apart.guests[U(hunter)].movesLeft > 0)
    {
        apart.guests[U(marked)].cell = I8(other);
        const Action step = ChooseAction(def, apart, hunter);
        CHECK(step.kind == ActionKind::Move);
        CHECK(Neighbour(def, apart.guests[U(hunter)].cell, step.a) == other);
    }
    // The marked bot, seen, moves away from the hunter when it can.
    NightState hide = state;
    hide.guests[U(marked)].cell = hide.guests[U(hunter)].cell;
    while (Current(hide) != marked && Current(hide) >= 0)
        REQUIRE(Apply(def, hide, {ActionKind::EndTurn}));
    if (Current(hide) == marked && hide.guests[U(marked)].movesLeft > 0)
        CHECK(ChooseAction(def, hide, marked).kind == ActionKind::Move);
}

TEST_CASE("Bots do not attack guests without cause (T104)", "[night][bots]")
{
    NightDef def = Data();
    def.rules.light.steps = 0;
    def.rules.omens.targets = {-99}; // nobody ever fails
    for (std::uint64_t seed = 1; seed <= 200; ++seed)
    {
        NightState state = NewNight(def, seed);
        bool turned = false;
        PlayWatching(def, state, [&](const LogEntry &e) {
            turned = turned || e.kind == LogKind::Turned;
            if (!turned && e.kind == LogKind::Battle)
                CHECK_FALSE((e.who >= 0 && e.other >= 0)); // no guest fought a guest
        });
    }
}

TEST_CASE("Styles differ: the explorer opens more rooms than the fighter (T104)", "[night][bots]")
{
    NightDef def = Data();
    // Same speed for all, so only the style differs.
    for (GuestDef &g : def.guests)
        g.speed = 4;
    const int explorer = NightDef::Find(def.guests, "nhi"), fighter = NightDef::Find(def.guests, "khang");
    long opened[2] = {0, 0};
    for (std::uint64_t seed = 1; seed <= 100; ++seed)
    {
        NightState state = NewNight(def, seed);
        PlayWatching(def, state, [&](const LogEntry &e) {
            if (e.kind == LogKind::Opened && (e.who == explorer || e.who == fighter))
                ++opened[e.who == explorer ? 0 : 1];
        });
    }
    CHECK(opened[0] > opened[1]);
}

TEST_CASE("A hurt bot heals or flees (T104)", "[night][bots]")
{
    NightDef def = Data();
    def.rules.monsters.chancePercent = 0;
    NightState state = NewNight(def, 2);
    const int g = Current(state);
    Guest &guest = state.guests[U(g)];
    guest.hp = 2;
    guest.items[0] = static_cast<std::int8_t>(NightDef::Find(def.items, "bandage"));
    CHECK(ChooseAction(def, state, g) == Action{ActionKind::UseItem, 0});
    // Without a heal, it does not walk into a monster.
    guest.items[0] = -1;
    const int monsterCell = Open(def, state, state.foyer, 0);
    state.monsters.push_back({0, I8(monsterCell), 99, 5, 2, 1});
    const Action choice = ChooseAction(def, state, g);
    CHECK_FALSE((choice.kind == ActionKind::Move && choice.a == 0));
}
