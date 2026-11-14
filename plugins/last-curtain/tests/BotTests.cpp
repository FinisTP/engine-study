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

#include <NightSoak.hpp>

TEST_CASE("In the hunt a strong bot goes for a minion first (T104)", "[night][bots]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 2);
    // Build the board, then turn guest 6 by hand at midnight.
    for (bool opened = true; opened;)
    {
        opened = false;
        for (int c = 0; c < maxCells && !opened; ++c)
            for (int d = 0; d < 4 && !opened; ++d)
                if (Doorway(def, state, c, d))
                    opened = Open(def, state, c, d) >= 0;
    }
    state.marked = 6;
    while (!state.taken.present)
        REQUIRE(Apply(def, state, Current(state) >= 0 && LegalActions(def, state)[0].kind == ActionKind::LevelUp ? LegalActions(def, state)[0]
                                                                                                            : Action{ActionKind::EndTurn}));
    const int g = Current(state);
    REQUIRE(!state.monsters.empty());
    // A minion in the room: attack it.
    NightState here = state;
    here.monsters[0].cell = here.guests[U(g)].cell;
    CHECK(ChooseAction(def, here, g) == Action{ActionKind::Attack, 0});
    // Ignoring minions (a soak strategy): not.
    CHECK_FALSE(ChooseAction(def, here, g, false) == Action{ActionKind::Attack, 0});
}

TEST_CASE("The soak report adds up (T105)", "[night][soak]")
{
    const NightDef def = Data();
    const SoakReport r = Soak(def, 1, 200);
    CHECK(r.nights == 200);
    CHECK(r.nights == r.companyWins + r.takenWins + r.faults);
    int hunts = 0, marked = 0;
    for (int g = 0; g < maxGuests; ++g)
        hunts += r.forms[static_cast<std::size_t>(g)].nights, marked += r.marked[static_cast<std::size_t>(g)];
    CHECK(hunts == r.nights - r.faults);
    CHECK(marked == r.nights - r.faults);
    CHECK(r.cursed <= r.nights);
}

TEST_CASE("A thousand nights have no faults (T105)", "[night][soak]")
{
    const NightDef def = Data();
    const SoakReport r = Soak(def, 1, 1000);
    CHECK(r.faults == 0);
    for (int g = 0; g < static_cast<int>(def.guests.size()); ++g)
        CHECK(r.marked[static_cast<std::size_t>(g)] >= 20);
    CHECK(r.rooms / r.nights >= 12);
}

// Slow (several thousand nights): run with "[balance]", or UmbraTool night-soak.
TEST_CASE("The shipped data is balanced (T105)", "[.][night][soak][balance]")
{
    const NightDef def = Data();
    const SoakReport bot = Soak(def, 1, 1000);
    INFO(Text(def, bot));
    CHECK(bot.Holds(def));
    const SoakReport bot500 = Soak(def, 1, 500), noMinions = Soak(def, 1, 500, 1), refuser = Soak(def, 1, 500, 2), taker = Soak(def, 1, 500, 3);
    // Killing minions must pay: the company wins clearly more hunts when it goes for them.
    CHECK(bot500.companyWins >= noMinions.companyWins + 500 * 5 / 100);
    CHECK(refuser.playerWins * 100 / 500 <= bot500.playerWins * 100 / 500 + 5);
    // The curse rarely strikes early, and then not in the first hours.
    CHECK(bot.cursed * 100 / bot.nights <= 25);
    CHECK(bot.cursedRounds >= 3LL * bot.cursed);
    const int hunts = taker.companyWins + taker.takenWins;
    CHECK(taker.companyWins * 100 / std::max(1, hunts) <= def.rules.balance.companyWinMax);
}
