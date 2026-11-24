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
            if (s.turn < 0 || s.turn >= static_cast<int>(s.order.size()))
                return;
            Unit &u = s.units[U(s.order[U(s.turn)])];
            if (s.mode.respawn && u.kind == 0 && Up(u) && u.cell == u.home)
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
            // A move bonus waiting from a hasten is spent on this turn (Match.md "Hasten").
            Unit &u = s.units[U(s.order[U(s.turn)])];
            s.now = {u.cell, static_cast<std::int8_t>(u.move + u.bonusMove), false};
            u.bonusMove = 0;
            u.marked = 0, u.slow = 0; // a mark or a slow lasts until its next turn
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

        // A bide: only as the whole turn, before moving or acting (Match.md "Biding").
        Why CheckBide(const MatchState &s, const Unit &u)
        {
            if (!s.mode.biding)
                return Why::Off;
            return s.now.acted || s.now.apSpent > 0 || u.cell != s.now.origin || s.now.movesLeft != u.move ? Why::AlreadyActed : Why::Ok;
        }

        // An extra action: after this turn's action, one action point for another move and action.
        Why CheckExtra(const MatchState &s, const Unit &u)
        {
            if (!s.mode.biding)
                return Why::Off;
            if (!s.now.acted)
                return Why::NothingHere;
            return u.ap >= 1 && s.now.apSpent < s.mode.apPerTurn ? Why::Ok : Why::NoAp;
        }

        Why CheckPickUp(const MatchDef &def, const MatchState &s, const Unit &u, int index)
        {
            if (s.now.acted)
                return Why::AlreadyActed;
            if (index < 0 || index >= static_cast<int>(s.floor.size()) || s.floor[U(index)].cell != u.cell)
                return Why::NothingHere;
            const Floor &f = s.floor[U(index)];
            const int carried = static_cast<int>(std::count_if(u.items.begin(), u.items.end(), [](std::int8_t i) { return i >= 0; }));
            const bool forStorage = f.item >= 0 && def.items[U(f.item)].family != ItemFamily::Item; // never in a slot
            if (!f.treasure && f.item >= 0 && !forStorage && carried >= def.rules.carry)
                return Why::BagFull;
            return Why::Ok;
        }
    }

    namespace inner
    {
        // The round ends: the round limit, then spawns and rooms coming back, then the order again.
        bool NewRound(const MatchDef &def, MatchState &s)
        {
            if (++s.round > s.mode.roundLimit && s.mode.roundLimit > 0)
            {
                EndMatch(def, s);
                return false;
            }
            RunSpawns(def, s, true);
            RunRoomSpawns(def, s);
            RunArrivals(def, s);
            RunGimmicks(def, s);
            SortOrder(s);
            s.turn = -1;
            return true;
        }

        void NextTurn(const MatchDef &def, MatchState &s)
        {
            // 1. The next unit in the order that can act; after the last, a new round (Match.md "The order").
            Commit(s);
            for (std::size_t guard = 0; guard <= s.order.size() * 3 + 3; ++guard)
            {
                if (++s.turn >= static_cast<int>(s.order.size()) && !NewRound(def, s))
                    return;
                if (s.turn < 0)
                    continue;
                RunSpawns(def, s, false);
                const int who = s.order[U(s.turn)];
                Unit &u = s.units[U(who)];
                // 2. A downed faction character counts down at its place; when its count runs out it is back home, whole.
                if (u.kind == 0 && u.cell < 0 && u.back > 0)
                {
                    if (--u.back == 0)
                    {
                        u.cell = static_cast<std::int16_t>(ReturnSquare(s, u.home));
                        u.hp = u.maxHp;
                        u.from = static_cast<std::int16_t>(s.round + 1);
                        Log(s, LogKind::Back, who, u.cell);
                    }
                    continue;
                }
                // 3. Fallen, off the board or not yet joined: skipped. A monster acts by its behaviour, then the order goes on.
                if (!Up(u) || u.from > s.round)
                    continue;
                if (u.kind == 0)
                    return BeginTurn(s);
                MonsterAct(def, s, who);
                if (s.battle.on || s.over)
                    return; // a hunter's battle: the picks first, then EndBattle goes on with the order
            }
        }
    }

    int Current(const MatchState &s)
    {
        if (s.over)
            return RiteOffered(s) ? Player(s) : -1;
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
            return a.kind != ActionKind::Rite ? Why::Over : RiteOffered(s) ? Why::Ok : Why::NoRite;
        if (a.kind == ActionKind::Rite)
            return Why::NoRite;
        const int me = Current(s);
        const Unit &u = s.units[U(me)];
        // 1. A level choice, then a battle pick, come before anything else.
        if (u.pending >= 0)
            return a.kind == ActionKind::LevelUp ? CheckLevelUp(def, s, me, a.a) : Why::MustChoose;
        if (s.battle.on)
            return a.kind == ActionKind::Withdraw ? Why::Ok : a.kind == ActionKind::BattleAbility ? CheckBattleAbility(def, s, me, a.a, a.b) : Why::MustChoose;
        if (a.kind == ActionKind::Withdraw)
            return Why::NothingHere;
        if (!s.cards.empty()) // 1b. dealt treasure: pick a card first
            return a.kind != ActionKind::Card ? Why::MustChoose : a.a >= 0 && a.a < static_cast<int>(s.cards.size()) ? Why::Ok : Why::NothingHere;
        if (a.kind == ActionKind::Card)
            return Why::NothingHere;
        switch (a.kind)
        {
        case ActionKind::Attack: return CheckAttack(def, s, me, a.a);
        case ActionKind::BoardAbility: return CheckBoardAbility(def, s, me, a.a, a.b);
        case ActionKind::UseItem: return CheckUseItem(def, s, me, a.a, a.b);
        case ActionKind::Answer: return CheckAnswer(def, s, me, a.a);
        case ActionKind::Perform: return CheckPerform(def, s, me);
        case ActionKind::Move: return CheckMove(def, s, u, a.a);
        case ActionKind::Undo: return s.now.acted ? Why::AlreadyActed : u.cell == s.now.origin ? Why::NothingHere : Why::Ok;
        case ActionKind::PickUp: return CheckPickUp(def, s, u, a.a);
        case ActionKind::EndTurn: return Why::Ok;
        case ActionKind::Bide: return CheckBide(s, u);
        case ActionKind::Cut: return CheckCut(s, me, a.a);
        case ActionKind::Extra: return CheckExtra(s, u);
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
        {
            all.push_back({ActionKind::Attack, i8(u)});
            all.push_back({ActionKind::Cut, i8(u)});
        }
        all.push_back({ActionKind::Rite});
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
        {
            all.push_back({ActionKind::UseItem, i8(slot)});
            for (int u = 0; u < static_cast<int>(s.units.size()); ++u) // a revive item's targets
                all.push_back({ActionKind::UseItem, i8(slot), i8(u)});
        }
        for (int f = 0; f < static_cast<int>(s.floor.size()); ++f)
            all.push_back({ActionKind::PickUp, i8(f)});
        for (int c = 0; c < static_cast<int>(s.cards.size()); ++c)
            all.push_back({ActionKind::Card, i8(c)});
        for (ActionKind k : {ActionKind::Perform, ActionKind::EndTurn, ActionKind::Bide, ActionKind::Extra})
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
        if (a.kind == ActionKind::Rite)
        {
            Rite(def, s);
            return true;
        }
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
            if (f.item >= 0 && def.items[U(f.item)].family != ItemFamily::Item)
                s.found.push_back(f.item);
            else if (f.item >= 0)
                *std::find(u.items.begin(), u.items.end(), std::int8_t(-1)) = f.item;
            Log(s, LogKind::Item, me, f.item, f.essence);
            Acted(def, s);
            break;
        }
        case ActionKind::Attack: StartBattle(def, s, me, a.a); break;
        case ActionKind::BattleAbility:
            Pick(def, s, me, a.a, a.b);
            break;
        case ActionKind::Withdraw: Withdraw(def, s); break;
        case ActionKind::BoardAbility: BoardAbility(def, s, me, a.a, a.b); break;
        case ActionKind::UseItem: UseItem(def, s, me, a.a, a.b); break;
        case ActionKind::Answer: Answer(def, s, me, a.a); break;
        case ActionKind::LevelUp: LevelUp(def, s, me, a.a); break;
        case ActionKind::Perform: Perform(def, s, me); break;
        case ActionKind::EndTurn:
            // Footsteps: ending beside an ally with them gives a move next turn (Match.md "Effects").
            for (const Unit &o : s.units)
                if (&o != &u && Up(o) && o.faction == u.faction && Beside(s.board, u, o))
                    u.bonusMove = static_cast<std::int8_t>(u.bonusMove + Passive(def, o, AbilityEffect::Footsteps));
            NextTurn(def, s);
            break;
        case ActionKind::Bide:
            u.ap = static_cast<std::int8_t>(std::min<int>(u.ap + 1, s.mode.maxAp));
            Log(s, LogKind::Bided, me, u.ap);
            NextTurn(def, s);
            break;
        case ActionKind::Cut: Cut(def, s, me, a.a); break;
        case ActionKind::Extra:
            --u.ap;
            ++s.now.apSpent;
            s.now.acted = false;
            s.now.movesLeft = static_cast<std::int8_t>(u.move);
            s.now.origin = u.cell;
            break;
        case ActionKind::Card: TakeCard(def, s, me, a.a); break;
        default: return false;
        }
        return true;
    }
}
