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

TEST_CASE("Seven bots finish a run of nights on every seed (T104, T235)", "[night][bots]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 60; ++seed)
    {
        NightState state = NewNight(def, seed);
        const int applies = PlayNight(def, state);
        INFO("seed " << seed);
        CHECK(applies < 40000);
        CHECK(Result(state) != Outcome::Running);
    }
}

TEST_CASE("A hurt bot heals or flees (T104)", "[night][bots]")
{
    NightDef def = Data();
    def.rules.monsters.chancePercent = 0;
    NightState state = FoyerNight(def, 2);
    const int g = Current(state);
    Guest &guest = state.guests[U(g)];
    guest.hp = 2;
    guest.items[0] = static_cast<std::int8_t>(NightDef::Find(def.items, "bandage"));
    CHECK(ChooseAction(def, state, g) == Action{ActionKind::UseItem, 0});
    // Without a heal, it does not walk into a monster.
    guest.items[0] = -1;
    int d = 0;
    while (d < 4 && !Passage(def, state, state.foyer, d))
        ++d;
    REQUIRE(d < 4);
    state.monsters.push_back({0, I8(Neighbour(def, state.foyer, d)), 99, 5, 2, 1});
    const Action choice = ChooseAction(def, state, g);
    CHECK_FALSE((choice.kind == ActionKind::Move && choice.a == d));
}

#include <NightSoak.hpp>

TEST_CASE("The soak report adds up: every run ends, with one escape or the house (T105, T235)", "[night][soak]")
{
    const NightDef def = Data();
    const SoakReport r = Soak(def, 1, 60);
    CHECK(r.runs == 60);
    CHECK(r.faults == 0);
    int wins = 0;
    for (const int w : r.wins)
        wins += w;
    CHECK(wins == r.escapes);
    CHECK(r.escapes + r.houseWins == r.runs - r.faults);
    CHECK(r.nights >= r.runs - r.faults);
    CHECK(r.deaths >= 6 * (r.runs - r.faults) - r.houseWins * 0); // six of seven die in an escape, all seven when the house wins
}

// Slow: run with "[balance]", or UmbraTool night-soak.
TEST_CASE("The shipped data is balanced (T105, T235)", "[.][night][soak][balance]")
{
    const NightDef def = Data();
    const SoakReport bot = Soak(def, 1, 400);
    INFO(Text(def, bot));
    CHECK(bot.Holds(def));
}


TEST_CASE("A bot never steps back the way it came in a turn (T121)", "[night][bots]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 200; ++seed)
    {
        NightState state = NewNight(def, seed);
        std::array<int, maxGuests> left;
        left.fill(-1);
        int round = state.round, turn = state.turn, applies = 0;
        while (Current(state) >= 0 && !state.taken.present && applies++ < 5000)
        {
            if (state.round != round || state.turn != turn)
                left.fill(-1), round = state.round, turn = state.turn;
            const int g = Current(state);
            const Action a = ChooseAction(def, state, g);
            const int before = state.guests[U(g)].cell;
            const std::uint32_t logBefore = state.logCount;
            REQUIRE(Apply(def, state, a));
            if (a.kind == ActionKind::Move && state.logCount == logBefore) // a plain step (a room or trap may move the guest)
            {
                INFO("seed " << seed << " guest " << g << " taken " << state.taken.present << " round " << state.round << " move " << int(a.a) << " visited " << state.guests[U(g)].visited.count() << " steps " << int(state.guests[U(g)].steps));
                CHECK((state.guests[U(g)].cell != left[U(g)] || left[U(g)] == before));
                left[U(g)] = before;
            }
            else if (a.kind == ActionKind::Move)
                left[U(g)] = state.guests[U(g)].from;
        }
    }
}

TEST_CASE("A bot with greed 0 leaves a worn item on the floor (T121)", "[night][bots]")
{
    NightDef def = Data();
    NightState state = FoyerNight(def, 3);
    const int g = Current(state);
    def.styles[U(def.guests[U(g)].style)].greed = 0;
    const int worn = [&] {
        for (std::size_t i = 0; i < def.items.size(); ++i)
            if (def.items[i].kind == ItemKind::Worn)
                return static_cast<int>(i);
        return -1;
    }();
    REQUIRE(worn >= 0);
    state.floor.push_back({I8(state.foyer), I8(worn)});
    CHECK_FALSE(ChooseAction(def, state, g).kind == ActionKind::PickUp);
    // Healing it takes.
    state.floor[0].item = I8(NightDef::Find(def.items, "bandage"));
    CHECK(ChooseAction(def, state, g).kind == ActionKind::PickUp);
}

TEST_CASE("A bot never walks into a room twice in one turn (T167)", "[night][bots]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 60; ++seed)
    {
        NightState state = NewNight(def, seed);
        std::array<std::bitset<maxCells>, maxGuests> seen{}; // per guest: chats hand the turn over for a moment
        int turnKey = -1, applies = 0;
        while (Current(state) >= 0 && applies++ < 4000)
        {
            const int g = Current(state);
            if (state.round * 8 + state.turn != turnKey)
            {
                for (auto &s : seen)
                    s.reset();
                turnKey = state.round * 8 + state.turn;
            }
            seen[U(g)].set(U(state.guests[U(g)].cell));
            const Action a = ChooseAction(def, state, g);
            const std::uint32_t log = state.logCount;
            REQUIRE(Apply(def, state, a));
            if (a.kind == ActionKind::Move && state.logCount == log && Current(state) == g)
            {
                INFO("seed " << seed << " guest " << g);
                CHECK_FALSE(seen[U(g)].test(U(state.guests[U(g)].cell)));
            }
        }
    }
}
