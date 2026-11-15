#include "NightTestData.hpp"

#include <NightBots.hpp>

#include <algorithm>

// The battle royale's rules core (docs/design/2026-11-16-battle-royale.md, plan stage 1).
using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    // Ends turns until it is `g`'s board turn (never through a battle or a chat).
    void TurnOf(const NightDef &def, NightState &state, int g)
    {
        for (int i = 0; i < 40 && Current(state) != g; ++i)
            REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
        REQUIRE(Current(state) == g);
    }

    // Kills a guest outright (the house's damage).
    void Kill(const NightDef &def, NightState &state, int g) { Hurt(def, state, g, 999, -1); }
}

TEST_CASE("The whole map is laid at the start, all reachable; no move opens a room (T228)", "[night][royale]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        const NightState state = NewNight(def, seed, 0);
        int opened = 0;
        for (int c = 0; c < maxCells; ++c)
            if (state.cells[U(c)].opened)
            {
                ++opened;
                CHECK(Steps(def, state, state.foyer, c) >= 0);
            }
        CHECK(opened >= def.rules.board.rooms - 2); // a stalled layout may stop just short
        CHECK(state.understage >= 0);
        for (const Action &a : LegalActions(def, state))
            if (a.kind == ActionKind::Move && a.a < transportMove)
                CHECK(Passage(def, state, state.guests[U(Current(state))].cell, a.a));
    }
}

TEST_CASE("Guests start far apart (T229)", "[night][royale]")
{
    const NightDef def = Data();
    int apart = 0;
    for (std::uint64_t seed = 1; seed <= 30; ++seed)
    {
        const NightState state = NewNight(def, seed, 0);
        int nearest = 99;
        for (int a = 0; a < state.count; ++a)
            for (int b = a + 1; b < state.count; ++b)
                nearest = std::min(nearest, Steps(def, state, state.guests[U(a)].cell, state.guests[U(b)].cell));
        CHECK(nearest >= 2);
        apart += nearest >= def.rules.spawn.minSteps ? 1 : 0;
    }
    CHECK(apart >= 25); // nearly always at least minSteps
}

TEST_CASE("Any guest may attack another in the same room (T230)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 3, 0);
    const int g = Current(state);
    const int other = g == 0 ? 1 : 0;
    const std::vector<Action> legal = LegalActions(def, state);
    const Action attack{ActionKind::Attack, -1, static_cast<std::int8_t>(other)};
    REQUIRE(std::find(legal.begin(), legal.end(), attack) != legal.end());
    REQUIRE(Apply(def, state, attack));
    CHECK(state.battle.active);
    bool both = false;
    for (int f = 0; f < state.battle.count; ++f)
        both = both || (state.battle.fighters[U(f)].guest == other && state.battle.fighters[U(f)].side == 1);
    CHECK(both);
}

TEST_CASE("0 HP is death: items fall, the guest becomes a ghost (T231)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 3, 0);
    state.guests[2].items[0] = static_cast<std::int8_t>(NightDef::Find(def.items, "bandage"));
    const std::size_t floor = state.floor.size();
    const int living = LivingCount(state);
    Hurt(def, state, 2, 999, 1);
    const Guest &dead = state.guests[2];
    CHECK(dead.dead);
    CHECK(dead.ghost);
    CHECK_FALSE(dead.down);
    CHECK_FALSE(Living(state, 2));
    CHECK(state.floor.size() == floor + 1);
    CHECK(LivingCount(state) == living - 1);
    bool logged = false;
    for (const LogEntry &e : NewLog(state, 0))
        logged = logged || (e.kind == LogKind::Died && e.who == 2 && e.other == 1);
    CHECK(logged);
}

TEST_CASE("One guest left alive escapes; none left, the house wins (T232)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 3, 0);
    for (int g = 1; g < state.count; ++g)
        Kill(def, state, g);
    CHECK(state.over);
    CHECK(state.outcome == Outcome::Escaped);
    CHECK(state.winner == 0);
    NightState none = FoyerNight(def, 3, 0);
    for (int g = 0; g < none.count; ++g)
        Kill(def, none, g);
    CHECK(none.over);
    CHECK(none.outcome == Outcome::House);
}

TEST_CASE("A ghost rises at the under-stage, attacks the living, and comes back when struck down (T233)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 5, 0);
    const int g = state.order[1];
    Kill(def, state, g);
    REQUIRE(state.guests[U(g)].ghost);
    CHECK(state.guests[U(g)].cell < 0); // out of play until its round
    // 1. It rises at the next round's start, weak, at the under-stage.
    const int round = state.round;
    for (int i = 0; i < 40 && state.round == round; ++i)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK(state.guests[U(g)].cell == state.understage);
    CHECK(state.guests[U(g)].hp == def.rules.ghost.hp);
    CHECK(Stat(def, state, g, StatKind::Might) == def.rules.ghost.might);
    CHECK(InPlay(state, g));
    // 2. On its turn: moves, attacks on the living in its room, end turn; no items.
    const int victim = state.order[0] == g ? state.order[2] : state.order[0];
    state.guests[U(victim)].cell = state.understage;
    TurnOf(def, state, g);
    for (const Action &a : LegalActions(def, state))
        CHECK((a.kind == ActionKind::Move || a.kind == ActionKind::EndTurn || (a.kind == ActionKind::Attack && a.b >= 0)));
    const std::vector<Action> legal = LegalActions(def, state);
    CHECK(std::find(legal.begin(), legal.end(), Action{ActionKind::Attack, -1, static_cast<std::int8_t>(victim)}) != legal.end());
    // 3. Struck down, it leaves the board and comes back after respawnRounds.
    Hurt(def, state, g, 999, victim);
    CHECK(state.guests[U(g)].cell < 0);
    CHECK(state.guests[U(g)].returnRound == state.round + def.rules.ghost.respawnRounds);
    CHECK(Living(state, victim));
}

TEST_CASE("At dawn with two or more alive, a new night on a new map; the living keep their gains (T234)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 9, 0);
    Kill(def, state, 3);
    state.guests[0].hp = 2, state.guests[0].might = 9;
    state.guests[0].items[0] = static_cast<std::int8_t>(NightDef::Find(def.items, "bandage"));
    const auto before = state.cells;
    for (int i = 0; i < 400 && state.night == 1 && !state.over; ++i)
        REQUIRE(Apply(def, state, LegalActions(def, state).back())); // end turns (and leave battles)
    REQUIRE(state.night == 2);
    CHECK_FALSE(state.over);
    CHECK(state.round == 1);
    CHECK(state.guests[0].hp == Stat(def, state, 0, StatKind::MaxHp));
    CHECK(state.guests[0].might == 9);
    CHECK(state.guests[0].items[0] >= 0);
    CHECK(state.guests[3].ghost);
    bool changed = false;
    for (int c = 0; c < maxCells; ++c)
        changed = changed || before[U(c)].room != state.cells[U(c)].room;
    CHECK(changed);
}

// Stage 2: the wandering killer (Night.md "The killer").
namespace
{
    // Ends turns until a new round has begun (its start moves the killer).
    void NextRound(const NightDef &def, NightState &state)
    {
        const int round = state.round;
        for (int i = 0; i < 40 && state.round == round && !state.over; ++i)
            REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    }

    // A night just before the killer wakes, no monsters, guests where they spawned.
    NightState BeforeWake(const NightDef &def, std::uint64_t seed)
    {
        NightState state = NewNight(def, seed, -1);
        while (state.round < def.rules.killer.wakeRound - 1)
            NextRound(def, state);
        return state;
    }

    NightDef NoMonsters()
    {
        NightDef def = Data();
        def.rules.monsters.chancePercent = 0;
        return def;
    }
}

TEST_CASE("The killer wakes at midnight in the under-stage (T236)", "[night][royale][killer]")
{
    const NightDef def = NoMonsters();
    NightState state = BeforeWake(def, 3);
    CHECK(state.killer.cell < 0);
    NextRound(def, state);
    REQUIRE(state.round == def.rules.killer.wakeRound);
    CHECK(state.killer.cell >= 0);
    bool woke = false;
    for (const LogEntry &e : NewLog(state, 0))
        woke = woke || (e.kind == LogKind::Killer && e.b == 0);
    CHECK(woke);
}

TEST_CASE("The killer closes on the nearest living guest it senses, and kills one in a room it enters (T237)", "[night][royale][killer]")
{
    const NightDef def = NoMonsters();
    NightState state = BeforeWake(def, 4);
    NextRound(def, state); // awake
    const int g = 2;
    // 1. A guest two rooms off: after a round it is nearer (or dead).
    int two = -1;
    for (int c = 0; c < maxCells && two < 0; ++c)
        if (state.cells[U(c)].opened && Steps(def, state, state.killer.cell, c) == 2)
            two = c;
    REQUIRE(two >= 0);
    for (int o = 0; o < state.count; ++o) // everyone else far away
        if (o != g)
            state.guests[U(o)].cell = state.foyer;
    state.guests[U(g)].cell = static_cast<std::int8_t>(two);
    NextRound(def, state);
    // Speed 2 at least: it reaches the room and kills.
    CHECK(state.guests[U(g)].dead);
    CHECK(state.killer.cell == two);
    bool logged = false;
    for (const LogEntry &e : NewLog(state, 0))
        logged = logged || (e.kind == LogKind::Died && e.who == g && e.other == -1);
    CHECK(logged);
}

TEST_CASE("A guest who walks into the killer's room dies; a ghost there is untouched; it cannot be attacked (T238)", "[night][royale][killer]")
{
    const NightDef def = NoMonsters();
    NightState state = BeforeWake(def, 5);
    NextRound(def, state);
    const int g = Current(state);
    REQUIRE(g >= 0);
    // A passage from a neighbour into the killer's room: the guest stands there and steps in.
    int from = -1, dir = -1;
    for (int d = 0; d < 4 && from < 0; ++d)
        if (const int n = Neighbour(def, state.killer.cell, d); n >= 0 && Passage(def, state, state.killer.cell, d))
            from = n, dir = (d + 2) % 4;
    REQUIRE(from >= 0);
    Guest &guest = state.guests[U(g)];
    guest.cell = guest.origin = static_cast<std::int8_t>(from);
    guest.budget = guest.movesLeft = 3;
    for (const Action &a : LegalActions(def, state))
        CHECK_FALSE((a.kind == ActionKind::Attack && a.a < 0 && a.b < 0));
    REQUIRE(Apply(def, state, {ActionKind::Move, static_cast<std::int8_t>(dir)}));
    CHECK(guest.dead);
    // A ghost standing with the killer is never harmed by it.
    NightState ghostly = BeforeWake(def, 5);
    NextRound(def, ghostly);
    Kill(def, ghostly, 1);
    ghostly.guests[1].cell = ghostly.killer.cell, ghostly.guests[1].hp = def.rules.ghost.hp;
    NextRound(def, ghostly);
    CHECK(ghostly.guests[1].hp == def.rules.ghost.hp);
}

TEST_CASE("Each new night the killer sleeps until midnight again (T239)", "[night][royale][killer]")
{
    const NightDef def = NoMonsters();
    NightState state = BeforeWake(def, 6);
    NextRound(def, state);
    REQUIRE(state.killer.cell >= 0);
    while (state.night == 1 && !state.over)
        NextRound(def, state);
    if (!state.over)
        CHECK(state.killer.cell < 0);
}

TEST_CASE("Monsters grow likelier and more numerous with the hour (T240)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 1);
    const int chance = MonsterChance(def, state), most = MonsterMax(def, state);
    CHECK(chance == def.rules.monsters.chancePercent);
    CHECK(most == def.rules.monsters.max);
    state.round = 9; // six hours on
    CHECK(MonsterChance(def, state) == std::min(100, chance + 6 * def.rules.monsters.chancePerHour));
    CHECK(MonsterMax(def, state) == most + 6 / def.rules.monsters.maxPerHours);
}

// Stage 3: quests and powers (Night.md "Quests", "Powers").
namespace
{
    int CellOfRoom(const NightState &state, int room)
    {
        for (int c = 0; c < maxCells; ++c)
            if (state.cells[U(c)].opened && state.cells[U(c)].room == room)
                return c;
        return -1;
    }

    int QuestRow(const NightDef &def, const char *id) { return NightDef::Find(def.quests, id); }

    // Puts guest g in `cell` with moves to spare, as if it walked there now (the quest checks on entering).
    void StandIn(const NightDef &def, NightState &state, int g, int cell)
    {
        TurnOf(def, state, g);
        Guest &guest = state.guests[U(g)];
        int from = -1, dir = -1;
        for (int d = 0; d < 4 && from < 0; ++d)
            if (Passage(def, state, cell, d))
                from = Neighbour(def, cell, d), dir = (d + 2) % 4;
        REQUIRE(from >= 0);
        guest.cell = guest.origin = static_cast<std::int8_t>(from);
        guest.budget = guest.movesLeft = 5, guest.steps = 0, guest.visited.reset();
        REQUIRE(Apply(def, state, {ActionKind::Move, static_cast<std::int8_t>(dir)}));
    }
}

TEST_CASE("Every map holds its quests' rooms, and each carry quest's item lies in its first room (T241)", "[night][royale][quest]")
{
    const NightDef def = Data();
    REQUIRE(def.quests.size() >= 8);
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        const NightState state = NewNight(def, seed);
        for (const QuestDef &q : def.quests)
        {
            if (q.roomA >= 0)
                CHECK(CellOfRoom(state, q.roomA) >= 0);
            if (q.roomB >= 0)
                CHECK(CellOfRoom(state, q.roomB) >= 0);
            if (q.kind == QuestKind::Carry)
                CHECK(std::any_of(state.floor.begin(), state.floor.end(),
                                  [&](const FloorItem &f) { return f.item == q.item && state.cells[U(f.cell)].room == q.roomA; }));
        }
    }
}

TEST_CASE("A carry quest is done by bringing its item into its second room: a power and a stat; the next finisher gets nothing (T242)", "[night][royale][quest]")
{
    NightDef def = NoMonsters();
    NightState state = NewNight(def, 7);
    const int q = QuestRow(def, "body");
    REQUIRE(q >= 0);
    const QuestDef &quest = def.quests[U(q)];
    const int g = state.order[0], other = state.order[1];
    const int might = state.guests[U(g)].might;
    state.guests[U(g)].items[0] = static_cast<std::int8_t>(quest.item);
    StandIn(def, state, g, CellOfRoom(state, quest.roomB));
    CHECK(state.questBy[U(q)] == g + 1);
    CHECK(state.guests[U(g)].power == quest.power);
    CHECK(state.guests[U(g)].might == might + quest.amount);
    // The race is won: a second guest with the item gains nothing.
    state.guests[U(other)].items[0] = static_cast<std::int8_t>(quest.item);
    StandIn(def, state, other, CellOfRoom(state, quest.roomB));
    CHECK(state.questBy[U(q)] == g + 1);
    CHECK(state.guests[U(other)].power != quest.power);
}

TEST_CASE("A visit quest needs its rooms in order (T243)", "[night][royale][quest]")
{
    NightDef def = NoMonsters();
    NightState state = NewNight(def, 8);
    const int q = QuestRow(def, "pilgrim");
    const QuestDef &quest = def.quests[U(q)];
    const int g = state.order[0];
    StandIn(def, state, g, CellOfRoom(state, quest.roomB)); // the second room first: nothing yet
    CHECK(state.questBy[U(q)] == 0);
    StandIn(def, state, g, CellOfRoom(state, quest.roomA));
    StandIn(def, state, g, CellOfRoom(state, quest.roomB));
    CHECK(state.questBy[U(q)] == g + 1);
}

TEST_CASE("A guest's power: legal when ready, used on another guest, then cooling down (T244)", "[night][royale][power]")
{
    const NightDef def = NoMonsters();
    NightState state = FoyerNight(def, 9);
    const int g = Current(state);
    const int other = state.order[1];
    state.guests[U(g)].power = static_cast<std::int8_t>(NightDef::Find(def.powers, "area")); // everyone within its range
    const std::vector<Action> legal = LegalActions(def, state);
    const auto power = std::find_if(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Power; });
    REQUIRE(power != legal.end());
    const int hp = state.guests[U(other)].hp;
    REQUIRE(Apply(def, state, *power));
    CHECK(state.guests[U(other)].hp < hp);
    CHECK(state.guests[U(g)].hp == Stat(def, state, g, StatKind::MaxHp)); // never itself
    const std::vector<Action> after = LegalActions(def, state);
    CHECK(std::none_of(after.begin(), after.end(), [](const Action &a) { return a.kind == ActionKind::Power; }));
    CHECK(state.guests[U(g)].powerCooldown > 0);
}

TEST_CASE("Quests open again on a new night (T245)", "[night][royale][quest]")
{
    NightDef def = NoMonsters();
    def.rules.killer.wakeRound = 99;
    NightState state = NewNight(def, 10);
    state.questBy[0] = 1;
    state.guests[0].quest[5] = 1;
    while (state.night == 1 && !state.over)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    REQUIRE(state.night == 2);
    CHECK(state.questBy[0] == 0);
    CHECK(state.guests[0].quest[5] == 0);
}

// Stage 4: persuading (Night.md "Persuading") and natures.
namespace
{
    NightDef EasyTalk()
    {
        NightDef def = NoMonsters();
        for (QuestionDef &q : def.questions)
            q.physicalTarget = q.magicalTarget = -50; // every answer passes
        return def;
    }

    bool Legal(const NightDef &def, const NightState &state, const Action &a)
    {
        const std::vector<Action> legal = LegalActions(def, state);
        return std::find(legal.begin(), legal.end(), a) != legal.end();
    }

    // The current guest asks guest `o` for `request` (b: -1 spare, -2 an item, a guest: go after them) and passes.
    void Persuade(const NightDef &def, NightState &state, int o, int request)
    {
        REQUIRE(Apply(def, state, {ActionKind::Chat, static_cast<std::int8_t>(o), static_cast<std::int8_t>(request)}));
        REQUIRE(state.chat.active);
        REQUIRE(Apply(def, state, {ActionKind::Answer, 0}));
    }
}

TEST_CASE("Talking offers requests: spare me, an item (if they carry one), go after someone (T246)", "[night][royale][persuade]")
{
    const NightDef def = EasyTalk();
    NightState state = FoyerNight(def, 11);
    const int g = Current(state), o = state.order[1], v = state.order[2];
    CHECK(Legal(def, state, {ActionKind::Chat, static_cast<std::int8_t>(o), -1}));
    CHECK_FALSE(Legal(def, state, {ActionKind::Chat, static_cast<std::int8_t>(o), -2}));
    state.guests[U(o)].items[0] = static_cast<std::int8_t>(NightDef::Find(def.items, "bandage"));
    CHECK(Legal(def, state, {ActionKind::Chat, static_cast<std::int8_t>(o), -2}));
    CHECK(Legal(def, state, {ActionKind::Chat, static_cast<std::int8_t>(o), static_cast<std::int8_t>(v)}));
    CHECK_FALSE(Legal(def, state, {ActionKind::Chat, static_cast<std::int8_t>(o), static_cast<std::int8_t>(g)}));
}

TEST_CASE("A passed request is granted: a truce, an item on the floor, a guest hunted (T247)", "[night][royale][persuade]")
{
    const NightDef def = EasyTalk();
    SECTION("spare me")
    {
        NightState state = FoyerNight(def, 12);
        const int g = Current(state), o = state.order[1];
        Persuade(def, state, o, -1);
        CHECK(Truced(state, g, o));
        CHECK(Truced(state, o, g));
    }
    SECTION("an item")
    {
        NightState state = FoyerNight(def, 12);
        const int o = state.order[1];
        const std::int8_t bandage = static_cast<std::int8_t>(NightDef::Find(def.items, "bandage"));
        state.guests[U(o)].items[0] = bandage;
        Persuade(def, state, o, -2);
        CHECK(state.guests[U(o)].items[0] == -1);
        CHECK(std::any_of(state.floor.begin(), state.floor.end(), [&](const FloorItem &f) { return f.item == bandage && f.cell == state.foyer; }));
    }
    SECTION("go after someone")
    {
        NightState state = FoyerNight(def, 12);
        const int o = state.order[1], v = state.order[2];
        Persuade(def, state, o, v);
        CHECK(state.guests[U(o)].hunt == v);
        CHECK(state.guests[U(o)].huntUntil > state.round);
    }
}

TEST_CASE("Natures: a wicked guest is harder to persuade, a kind one easier (T248)", "[night][royale][persuade]")
{
    const NightDef def = NoMonsters();
    NightState state = FoyerNight(def, 13);
    const int g = Current(state);
    int kind = -1, wicked = -1;
    for (int o = 0; o < state.count; ++o)
        if (o != g)
            (def.guests[U(o)].nature == Nature::Kind ? kind : def.guests[U(o)].nature == Nature::Wicked ? wicked : o) = o;
    REQUIRE(kind >= 0);
    REQUIRE(wicked >= 0);
    REQUIRE(Apply(def, state, {ActionKind::Chat, static_cast<std::int8_t>(kind), -1}));
    const int easy = AnswerTarget(def, state, 0) - def.questions[U(state.chat.question)].physicalTarget;
    NightState other = FoyerNight(def, 13);
    REQUIRE(Apply(def, other, {ActionKind::Chat, static_cast<std::int8_t>(wicked), -1}));
    const int hard = AnswerTarget(def, other, 0) - def.questions[U(other.chat.question)].physicalTarget;
    CHECK(easy == def.rules.persuade.kindTarget);
    CHECK(hard == def.rules.persuade.wickedTarget);
}

TEST_CASE("A kind bot keeps its truce; a hunting bot goes after its target (T249)", "[night][royale][persuade]")
{
    const NightDef def = NoMonsters();
    NightState state = FoyerNight(def, 14);
    const int g = Current(state);
    // Make the bot overwhelmingly strong, so it would attack anyone it may.
    state.guests[U(g)].might = 30, state.guests[U(g)].sanity = 30;
    NightDef kind = def;
    kind.guests[U(g)].nature = Nature::Kind;
    kind.guests[U(g)].style = NightDef::Find(def.styles, "fighter"); // a fighter: the fight beats a quest
    const Action free = ChooseAction(kind, state, g); // no truce yet: it strikes
    CHECK((free.kind == ActionKind::Attack && free.b >= 0));
    for (int o = 0; o < state.count; ++o)
        if (o != g)
            state.truces[U(o)] = {static_cast<std::int8_t>(g), static_cast<std::int8_t>(o), 99};
    const Action choice = ChooseAction(kind, state, g);
    CHECK_FALSE((choice.kind == ActionKind::Attack && choice.b >= 0));
    // Told to go after someone: it heads for them when they are not here.
    NightState hunting = FoyerNight(def, 14);
    const int v = hunting.order[3];
    int far = -1;
    for (int c = 0; c < maxCells && far < 0; ++c)
        if (hunting.cells[U(c)].opened && Steps(def, hunting, hunting.foyer, c) == 3)
            far = c;
    REQUIRE(far >= 0);
    for (int o = 0; o < hunting.count; ++o)
        if (o != g)
            hunting.guests[U(o)].cell = static_cast<std::int8_t>(o == v ? far : 0);
    Closet(def, hunting, 0);
    hunting.guests[U(g)].hunt = static_cast<std::int8_t>(v), hunting.guests[U(g)].huntUntil = 99;
    const Action step = ChooseAction(def, hunting, g);
    REQUIRE(step.kind == ActionKind::Move);
    CHECK(Steps(def, hunting, Neighbour(def, hunting.foyer, step.a), far) == 2);
}
