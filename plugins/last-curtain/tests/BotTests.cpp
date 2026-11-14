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
        NightState state = FoyerNight(def, seed);
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
        NightState state = FoyerNight(def, seed);
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
    NightState state = FoyerNight(def, 2);
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
    NightState state = FoyerNight(def, 2);
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
    for (int o = 0; o < state.count; ++o) // healthy: this test is about minions, not the failed-goal penalty
        if (o != 6)
            state.guests[U(o)].hp = state.guests[U(o)].maxHp;
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
    CHECK(r.nights == r.companyWins + r.takenWins + r.faults + r.allFinished);
    int hunts = 0, marked = 0;
    for (int g = 0; g < maxGuests; ++g)
        hunts += r.forms[static_cast<std::size_t>(g)].nights, marked += r.marked[static_cast<std::size_t>(g)];
    CHECK(hunts == r.nights - r.faults - r.allFinished);
    CHECK(marked == r.nights - r.faults - r.allFinished);
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
    const SoakReport bot500 = Soak(def, 1, 500), noMinions = Soak(def, 1, 500, 1);
    // Killing minions must pay: the company wins clearly more hunts when it goes for them.
    CHECK(bot500.companyWins >= noMinions.companyWins + 500 * 5 / 100);
}

namespace
{
    // Opens `n` rooms in a row from `from`; fewer if a room has no door onward.
    std::vector<int> Chain(const NightDef &def, NightState &state, int from, int n)
    {
        std::vector<int> cells;
        for (int at = from; static_cast<int>(cells.size()) < n;)
        {
            int next = -1;
            for (int d = 0; d < 4 && next < 0; ++d)
                if (Doorway(def, state, at, d))
                    next = Open(def, state, at, d);
            if (next < 0)
                break;
            cells.push_back(at = next);
        }
        return cells;
    }

    // A night whose first guest stands in the foyer with a chain of `n` rooms ahead.
    NightState ChainNight(const NightDef &def, int n, std::vector<int> &cells)
    {
        for (std::uint64_t seed = 1;; ++seed)
        {
            NightState state = FoyerNight(def, seed);
            cells = Chain(def, state, state.foyer, n);
            if (static_cast<int>(cells.size()) == n)
                return state;
        }
    }

    // Gives guest `g` a goal of its own (added to the data).
    void GiveGoal(NightDef &def, NightState &state, int g, GoalKind kind, int target)
    {
        def.goals.push_back({g, "test", kind, target, 1, "test"});
        state.guests[U(g)].goal = static_cast<std::int8_t>(def.goals.size() - 1);
    }

    // Plays guest g's turn by its bot; returns false if a step went back to the cell before.
    bool PlayTurn(const NightDef &def, NightState &state, int g)
    {
        int left = -1;
        while (Current(state) == g && !state.chat.active)
        {
            const Action a = ChooseAction(def, state, g);
            const int before = state.guests[U(g)].cell;
            REQUIRE(Apply(def, state, a));
            if (a.kind == ActionKind::Move)
            {
                if (state.guests[U(g)].cell == left && left != before)
                    return false;
                left = before;
            }
            if (a.kind == ActionKind::EndTurn)
                break;
        }
        return true;
    }
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
                INFO("seed " << seed << " guest " << g);
                CHECK((state.guests[U(g)].cell != left[U(g)] || left[U(g)] == before));
                left[U(g)] = before;
            }
            else if (a.kind == ActionKind::Move)
                left[U(g)] = state.guests[U(g)].from;
        }
    }
}

TEST_CASE("A bot walks toward its known goal room (T121)", "[night][bots]")
{
    NightDef def = Data();
    def.rules.monsters.chancePercent = 0;
    std::vector<int> cells;
    NightState state = ChainNight(def, 3, cells);
    const int g = Current(state), goal = cells.back();
    GiveGoal(def, state, g, GoalKind::Room, state.cells[U(goal)].room);
    const int start = Steps(def, state, state.foyer, goal);
    REQUIRE(PlayTurn(def, state, g));
    const int now = Steps(def, state, state.guests[U(g)].cell, goal);
    CHECK((now < start || state.guests[U(g)].goalRoomSeen));
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

TEST_CASE("A bot whose ally goal is the player opens a chat with the player (T121)", "[night][bots]")
{
    NightDef def = Data();
    NightState state = FoyerNight(def, 4, 0);
    while (Current(state) == 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const int g = Current(state);
    def.styles[U(def.guests[U(g)].style)].chatty = 0; // the goal alone asks for it
    GiveGoal(def, state, g, GoalKind::Ally, 0);
    CHECK(ChooseAction(def, state, g) == Action{ActionKind::OpenChat, 0});
}

TEST_CASE("A bot helps a group member's known goal (T121)", "[night][bots]")
{
    NightDef def = Data();
    def.rules.monsters.chancePercent = 0;
    std::vector<int> cells;
    NightState state = ChainNight(def, 3, cells);
    const int g = Current(state), member = (g + 1) % state.count;
    const int keepsake = NightDef::Find(def.items, "watch");
    GiveGoal(def, state, g, GoalKind::Room, state.cells[U(state.foyer)].room); // its own goal: done
    state.guests[U(g)].goalRoomSeen = true;
    GiveGoal(def, state, member, GoalKind::Item, keepsake);
    state.guests[U(g)].group = state.guests[U(member)].group = 5;
    state.guests[U(g)].knows = static_cast<std::uint8_t>(1u << member);
    state.guests[U(member)].cell = I8(cells[0]);
    state.floor.push_back({I8(cells.back()), I8(keepsake)});
    const int start = Steps(def, state, state.foyer, cells.back());
    REQUIRE(PlayTurn(def, state, g));
    CHECK(Steps(def, state, state.guests[U(g)].cell, cells.back()) < start);
}

TEST_CASE("A bot does not chase a keepsake holder it cannot see (T121)", "[night][bots]")
{
    NightDef def = Data();
    def.rules.monsters.chancePercent = 0;
    std::vector<int> cells;
    NightState state = ChainNight(def, 6, cells);
    const int g = Current(state), holder = (g + 1) % state.count;
    const int keepsake = NightDef::Find(def.items, "watch");
    GiveGoal(def, state, g, GoalKind::Item, keepsake);
    state.guests[U(holder)].cell = I8(cells.back());
    REQUIRE_FALSE(Lit(def, state, state.foyer, cells.back(), Stat(def, state, g, StatKind::Light)));
    NightState carrying = state;
    carrying.guests[U(holder)].items[0] = I8(keepsake);
    CHECK(ChooseAction(def, carrying, g) == ChooseAction(def, state, g));
}
