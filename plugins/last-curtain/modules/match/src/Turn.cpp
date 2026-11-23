#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>

// The turn: move, undo, act, end turn, and the end of the match (Match.md "A turn").
namespace umbra::match
{
    using namespace inner;

    namespace inner
    {
        int UnitAt(const MatchState &s, int cell)
        {
            for (std::size_t i = 0; i < s.units.size(); ++i)
                if (Covers(s.board, s.units[i], cell))
                    return static_cast<int>(i);
            return -1;
        }

        bool GuardianStands(const MatchDef &def, const MatchState &s, int faction)
        {
            return std::ranges::any_of(s.units, [&](const Unit &u) {
                return u.kind == 1 && u.faction == faction && u.hp > 0 && def.monsters[U(u.def)].tier == Tier::Guardian;
            });
        }

        void Log(MatchState &s, LogKind kind, int a, int b, int c)
        {
            s.log.push_back({kind, static_cast<std::int16_t>(a), static_cast<std::int16_t>(b), static_cast<std::int16_t>(c)});
        }

        int Roll(MatchState &s, int n)
        {
            Rng rng{s.rng};
            const int v = rng.Below(n);
            s.rng = rng.s;
            return v;
        }

        bool OnTurn(const MatchState &s)
        {
            return !s.over && !s.battle.on && std::ranges::none_of(s.units, [](const Unit &u) { return u.pending >= 0; });
        }

        // Standing on your own hub when the move is committed restores full HP.
        void Commit(MatchState &s)
        {
            if (s.turn < 0)
                return;
            Unit &u = s.units[U(s.order[U(s.turn)])];
            if (Up(u) && u.cell == u.home)
                u.hp = u.maxHp;
        }

        void Acted(const MatchDef &def, MatchState &s)
        {
            Commit(s);
            s.now.acted = true;
            if (def.options.autoEndTurn && !s.battle.on && !s.over)
                NextTurn(def, s);
        }
    }

    namespace
    {
        // 1. Most score wins; a tie goes to the side whose guardian alone still stands, then more kills.
        void EndMatch(const MatchDef &def, MatchState &s)
        {
            s.over = true;
            int kills[2] = {};
            for (const Unit &u : s.units)
                if (u.kind == 0)
                    kills[u.faction] += u.kills;
            const bool g0 = GuardianStands(def, s, 0), g1 = GuardianStands(def, s, 1);
            s.winner = s.score[0] != s.score[1] ? (s.score[0] > s.score[1] ? 0 : 1)
                     : g0 != g1                 ? (g0 ? 0 : 1)
                     : kills[0] != kills[1]     ? (kills[0] > kills[1] ? 0 : 1)
                                                : -1;
            Log(s, LogKind::Over, s.winner);
        }

        // A free square nearest the hub, for a character coming back.
        int ReturnSquare(const MatchState &s, int hub)
        {
            if (UnitAt(s, hub) < 0)
                return hub;
            int best = -1, bestSteps = 1 << 20;
            for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
                if (const int d = Steps(s.board, hub, c); d > 0 && d < bestSteps && UnitAt(s, c) < 0 && s.board.cells[U(c)].kind != CellKind::Room)
                    best = c, bestSteps = d;
            return best;
        }

        void BeginTurn(MatchState &s)
        {
            const Unit &u = s.units[U(s.order[U(s.turn)])];
            s.now = {u.cell, static_cast<std::int8_t>(u.move), false};
        }

        Why CheckMove(const MatchDef &def, const MatchState &s, const Unit &u, int dir)
        {
            if (s.now.acted)
                return Why::AlreadyActed;
            if (s.now.movesLeft <= 0)
                return Why::NoMovesLeft;
            const int n = dir >= 0 && dir < 4 ? Neighbour(s.board, u.cell, dir) : -1;
            if (n < 0)
                return Why::Blocked;
            // 2. Room for one more of my side; an enemy hub is shut while its guardian stands.
            if (!Room(def, s, n, u.faction))
                return Why::Blocked;
            const int enemy = 1 - u.faction;
            if (n == s.board.hub[U(enemy)] && GuardianStands(def, s, enemy))
                return Why::Blocked;
            return Why::Ok;
        }

        Why CheckPickUp(const MatchDef &def, const MatchState &s, const Unit &u, int index)
        {
            if (s.now.acted)
                return Why::AlreadyActed;
            if (index < 0 || index >= static_cast<int>(s.floor.size()) || s.floor[U(index)].cell != u.cell)
                return Why::NothingHere;
            const Floor &f = s.floor[U(index)];
            const int carried = static_cast<int>(std::count_if(u.items.begin(), u.items.end(), [](std::int8_t i) { return i >= 0; }));
            if (!f.treasure && f.item >= 0 && carried >= def.rules.carry)
                return Why::BagFull;
            return Why::Ok;
        }
    }

    namespace inner
    {
        void NewRound(const MatchDef &def, MatchState &s)
        {
            s.monsterTurn = -1;
            if (++s.round > def.options.rounds)
                return EndMatch(def, s);
            RunSpawns(def, s, true);
            RunRoomSpawns(def, s);
            s.turn = -1;
            NextTurn(def, s);
        }

        void ResumeMonsters(const MatchDef &def, MatchState &s)
        {
            ++s.monsterTurn;
            if (MonsterPhase(def, s))
                NewRound(def, s);
        }

        void NextTurn(const MatchDef &def, MatchState &s)
        {
            // 1. The next character in order; after the last, the monsters act and a new round begins.
            Commit(s);
            for (std::size_t guard = 0; guard <= s.order.size() * 4; ++guard)
            {
                if (++s.turn >= static_cast<int>(s.order.size()))
                {
                    // 1a. The monsters' phase; a hunter's battle pauses it until the character has picked.
                    s.turn = static_cast<int>(s.order.size()) - 1;
                    s.monsterTurn = 0;
                    if (MonsterPhase(def, s))
                        NewRound(def, s);
                    return;
                }
                RunSpawns(def, s, false);
                // 2. A downed character misses this turn; when its count runs out it is back home, whole.
                Unit &u = s.units[U(s.order[U(s.turn)])];
                if (Up(u))
                    return BeginTurn(s);
                if (u.back > 0 && --u.back == 0)
                {
                    u.cell = static_cast<std::int16_t>(ReturnSquare(s, u.home));
                    u.hp = u.maxHp;
                    Log(s, LogKind::Back, s.order[U(s.turn)], u.cell);
                }
            }
        }
    }

    int Current(const MatchState &s)
    {
        if (s.over)
            return -1;
        for (std::size_t i = 0; i < s.units.size(); ++i)
            if (s.units[i].pending >= 0)
                return static_cast<int>(i);
        if (s.battle.on)
            return Picker(s);
        return s.order[U(s.turn)];
    }

    Why Check(const MatchDef &def, const MatchState &s, const Action &a)
    {
        if (s.over)
            return Why::Over;
        const int me = Current(s);
        const Unit &u = s.units[U(me)];
        // 1. A level choice, then a battle pick, come before anything else.
        if (u.pending >= 0)
            return a.kind == ActionKind::LevelUp ? CheckLevelUp(def, s, me, a.a) : Why::MustChoose;
        if (s.battle.on)
            return a.kind == ActionKind::BattleAbility ? CheckBattleAbility(def, s, me, a.a, a.b) : Why::MustChoose;
        if (!s.cards.empty()) // 1b. dealt treasure: pick a card first
            return a.kind != ActionKind::Card ? Why::MustChoose : a.a >= 0 && a.a < static_cast<int>(s.cards.size()) ? Why::Ok : Why::NothingHere;
        if (a.kind == ActionKind::Card)
            return Why::NothingHere;
        switch (a.kind)
        {
        case ActionKind::Attack: return CheckAttack(def, s, me, a.a);
        case ActionKind::BoardAbility: return CheckBoardAbility(def, s, me, a.a, a.b);
        case ActionKind::UseItem: return CheckUseItem(def, s, me, a.a);
        case ActionKind::Answer: return CheckAnswer(def, s, me, a.a);
        case ActionKind::Perform: return CheckPerform(def, s, me);
        case ActionKind::Move: return CheckMove(def, s, u, a.a);
        case ActionKind::Undo: return s.now.acted ? Why::AlreadyActed : u.cell == s.now.origin ? Why::NothingHere : Why::Ok;
        case ActionKind::PickUp: return CheckPickUp(def, s, u, a.a);
        case ActionKind::EndTurn: return Why::Ok;
        default: return Why::Blocked;
        }
    }

    std::vector<Action> Legal(const MatchDef &def, const MatchState &s)
    {
        // 1. Every action that could apply, kept when Check allows it.
        std::vector<Action> all;
        auto i8 = [](int v) { return static_cast<std::int8_t>(v); };
        for (int d = 0; d < 4; ++d)
            all.push_back({ActionKind::Move, i8(d)});
        all.push_back({ActionKind::Undo});
        for (int u = 0; u < static_cast<int>(s.units.size()); ++u)
            all.push_back({ActionKind::Attack, i8(u)});
        for (int slot = 0; slot < maxAbilities; ++slot)
        {
            all.push_back({ActionKind::BattleAbility, i8(slot)});
            for (std::int8_t f : s.battle.fighters) // a battle pick at each fighter (Check keeps the foes)
                if (s.battle.on && f >= 0)
                    all.push_back({ActionKind::BattleAbility, i8(slot), f});
            for (int u = 0; u < static_cast<int>(s.units.size()); ++u)
                all.push_back({ActionKind::BoardAbility, i8(slot), i8(u)});
        }
        for (int slot = 0; slot < maxItems; ++slot)
            all.push_back({ActionKind::UseItem, i8(slot)});
        for (int f = 0; f < static_cast<int>(s.floor.size()); ++f)
            all.push_back({ActionKind::PickUp, i8(f)});
        for (int c = 0; c < static_cast<int>(s.cards.size()); ++c)
            all.push_back({ActionKind::Card, i8(c)});
        for (ActionKind k : {ActionKind::Perform, ActionKind::EndTurn})
            all.push_back({k});
        for (int c = 0; c < 2; ++c)
            all.push_back({ActionKind::Answer, i8(c)});
        for (int c = 0; c < static_cast<int>(s.offer.size()); ++c)
            all.push_back({ActionKind::LevelUp, i8(c)});
        std::vector<Action> out;
        for (const Action &a : all)
            if (Check(def, s, a) == Why::Ok)
                out.push_back(a);
        return out;
    }

    bool Apply(const MatchDef &def, MatchState &s, const Action &a)
    {
        if (Check(def, s, a) != Why::Ok)
            return false;
        const int me = Current(s);
        Unit &u = s.units[U(me)];
        switch (a.kind)
        {
        case ActionKind::Move:
            u.cell = static_cast<std::int16_t>(Neighbour(s.board, u.cell, a.a));
            --s.now.movesLeft;
            Log(s, LogKind::Moved, me, u.cell);
            break;
        case ActionKind::Undo:
            u.cell = s.now.origin;
            s.now.movesLeft = static_cast<std::int8_t>(u.move);
            Log(s, LogKind::Moved, me, u.cell);
            break;
        case ActionKind::PickUp:
        {
            const Floor f = s.floor[U(a.a)];
            s.floor.erase(s.floor.begin() + a.a);
            if (f.treasure)
            {
                RoomBack(def, s, f.cell, 1);
                DealCards(def, s);
                break;
            }
            u.essence += f.essence;
            if (f.item >= 0)
                *std::find(u.items.begin(), u.items.end(), std::int8_t(-1)) = f.item;
            Log(s, LogKind::Item, me, f.item, f.essence);
            Acted(def, s);
            break;
        }
        case ActionKind::Attack: StartBattle(def, s, me, a.a); break;
        case ActionKind::BattleAbility:
            Pick(def, s, me, a.a, a.b);
            if (!s.battle.on && s.monsterTurn >= 0)
                ResumeMonsters(def, s);
            break;
        case ActionKind::BoardAbility: BoardAbility(def, s, me, a.a, a.b); break;
        case ActionKind::UseItem: UseItem(def, s, me, a.a); break;
        case ActionKind::Answer: Answer(def, s, me, a.a); break;
        case ActionKind::LevelUp: LevelUp(def, s, me, a.a); break;
        case ActionKind::Perform: Perform(def, s, me); break;
        case ActionKind::EndTurn: NextTurn(def, s); break;
        case ActionKind::Card: TakeCard(def, s, me, a.a); break;
        default: return false;
        }
        return true;
    }
}
