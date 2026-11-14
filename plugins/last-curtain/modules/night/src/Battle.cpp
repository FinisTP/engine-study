#include "NightInner.hpp"

#include <algorithm>

// Battles, buffs, XP and monsters (Night.md "Battles").
namespace umbra::night
{
    using namespace inner;

    namespace
    {
        Fighter &At(NightState &state, int f) { return state.battle.fighters[U(f)]; }

        bool Standing(const NightState &state, const Fighter &f)
        {
            if (f.monster >= 0)
                return state.monsters[U(f.monster)].hp > 0;
            const Guest &g = state.guests[U(f.guest)];
            return !g.down && !g.dead;
        }

        Weapon AttackType(const NightDef &def, const NightState &state, const Fighter &f)
        {
            return f.monster >= 0 ? def.monsters[U(state.monsters[U(f.monster)].kind)].attack : def.guests[U(f.guest)].weapon;
        }

        int BuffOn(const NightDef &def, const Fighter &f, BuffStat stat)
        {
            int value = 0;
            for (std::size_t i = 0; i < f.buff.size(); ++i)
                if (f.buff[i] >= 0 && def.buffs[U(f.buff[i])].stat == stat)
                    value += def.buffs[U(f.buff[i])].amount;
            return value;
        }

        bool HasGuard(const NightDef &def, const Fighter &f)
        {
            return std::any_of(f.buff.begin(), f.buff.end(), [&](std::int8_t b) { return b >= 0 && def.buffs[U(b)].stat == BuffStat::Guard; });
        }

        int FighterStat(const NightDef &def, const NightState &state, const Fighter &f, StatKind kind)
        {
            if (f.guest >= 0)
                return Stat(def, state, f.guest, kind);
            const Monster &m = state.monsters[U(f.monster)];
            return (kind == StatKind::Might ? m.might : m.sanity) + BuffOn(def, f, kind == StatKind::Might ? BuffStat::Might : BuffStat::Sanity);
        }

        void AddBuff(const NightDef &def, Fighter &f, int buff)
        {
            if (buff < 0)
                return;
            auto slot = std::find(f.buff.begin(), f.buff.end(), I8(buff));
            if (slot == f.buff.end())
                slot = std::find(f.buff.begin(), f.buff.end(), std::int8_t(-1));
            if (slot == f.buff.end())
                return;
            *slot = I8(buff);
            f.buffRounds[U(static_cast<int>(slot - f.buff.begin()))] = I8(def.buffs[U(buff)].rounds);
        }

        int GuardBuff(const NightDef &def)
        {
            for (std::size_t b = 0; b < def.buffs.size(); ++b)
                if (def.buffs[b].stat == BuffStat::Guard)
                    return static_cast<int>(b);
            return -1;
        }

        bool SideStanding(const NightState &state, int side)
        {
            for (int f = 0; f < state.battle.count; ++f)
                if (state.battle.fighters[U(f)].side == side && Standing(state, state.battle.fighters[U(f)]))
                    return true;
            return false;
        }

        void GiveXp(const NightDef &def, NightState &state, int g, int xp)
        {
            Guest &guest = state.guests[U(g)];
            guest.xp = I16(guest.xp + xp);
            const auto &steps = def.rules.levels.xp;
            while (guest.level - 1 + guest.pending < static_cast<int>(steps.size()) && guest.xp >= steps[U(guest.level - 1 + guest.pending)])
                ++guest.pending;
        }

        void EndBattle(const NightDef &def, NightState &state)
        {
            // 1. Over: dead monsters go, buffs end; the owner's board turn is over.
            Battle &b = state.battle;
            b.active = false;
            const int owner = b.owner;
            state.monsters.erase(std::remove_if(state.monsters.begin(), state.monsters.end(), [](const Monster &m) { return m.hp <= 0; }),
                                 state.monsters.end());
            b = {};
            if (owner >= 0 && !state.over && state.order[U(state.turn)] == owner) // even if the owner fell
                EndTurn(def, state);
        }

        void Strike(const NightDef &def, NightState &state, int from, int to)
        {
            Fighter &attacker = At(state, from), &target = At(state, to);
            const int damage = Damage(def, state, attacker, target);
            // 0. A reflecting Taken turns the blow back on a guest who strikes it.
            if (target.taken && state.taken.reflect && attacker.guest >= 0)
            {
                state.taken.reflect = false;
                Hurt(def, state, attacker.guest, damage, target.guest);
                Log(state, LogKind::Hit, target.guest, attacker.guest, damage, state.guests[U(attacker.guest)].hp);
                return;
            }
            // 1. The guard is spent by the hit.
            for (std::size_t i = 0; i < target.buff.size(); ++i)
                if (target.buff[i] >= 0 && def.buffs[U(target.buff[i])].stat == BuffStat::Guard)
                    target.buff[i] = -1;
            // 2. The hit, and what it leaves: a monster's debuff, a kill's XP.
            if (target.guest >= 0)
            {
                Hurt(def, state, target.guest, damage, attacker.guest);
                if (attacker.monster >= 0)
                    AddBuff(def, target, def.monsters[U(state.monsters[U(attacker.monster)].kind)].debuff);
            }
            else
            {
                Monster &m = state.monsters[U(target.monster)];
                m.hp = I16(std::max(0, m.hp - damage));
                if (m.hp == 0)
                {
                    Log(state, LogKind::Monster, attacker.guest, -1, m.kind, 0);
                    for (int f = 0; f < state.battle.count; ++f)
                    {
                        const Fighter &winner = state.battle.fighters[U(f)];
                        if (winner.side != target.side && winner.guest >= 0 && Standing(state, winner))
                            GiveXp(def, state, winner.guest, def.monsters[U(m.kind)].xp);
                    }
                }
            }
            const int hp = target.guest >= 0 ? state.guests[U(target.guest)].hp : state.monsters[U(target.monster)].hp;
            Log(state, LogKind::Hit, attacker.guest, target.guest, damage, hp);
        }

        // The weakest standing enemy of a fighter (first in order on a tie).
        int Weakest(const NightState &state, int from)
        {
            int best = -1, bestHp = 1 << 30;
            for (int f = 0; f < state.battle.count; ++f)
            {
                const Fighter &t = state.battle.fighters[U(f)];
                if (t.side == state.battle.fighters[U(from)].side || !Standing(state, t))
                    continue;
                const int hp = t.monster >= 0 ? state.monsters[U(t.monster)].hp : state.guests[U(t.guest)].hp;
                if (hp < bestHp)
                    best = f, bestHp = hp;
            }
            return best;
        }

        // Next fighter; monsters act by themselves; the battle ends after its rounds or with a side down.
        void Advance(const NightDef &def, NightState &state)
        {
            Battle &b = state.battle;
            for (;;)
            {
                if (!b.active)
                    return; // the night ended mid-battle
                if (!SideStanding(state, 0) || !SideStanding(state, 1))
                    return EndBattle(def, state);
                if (++b.turn >= b.count)
                {
                    // End of a round: timed buffs count down.
                    for (int f = 0; f < b.count; ++f)
                        for (std::size_t i = 0; i < 4; ++i)
                            if (At(state, f).buff[i] >= 0 && At(state, f).buffRounds[i] > 0 && --At(state, f).buffRounds[i] == 0)
                                At(state, f).buff[i] = -1;
                    b.turn = 0;
                    if (++b.round > def.rules.battle.rounds)
                        return EndBattle(def, state);
                }
                const Fighter &f = At(state, b.turn);
                if (!Standing(state, f))
                    continue;
                if (f.monster < 0)
                    return; // a guest's choice
                const int target = Weakest(state, b.turn);
                if (target >= 0)
                    Strike(def, state, b.turn, target);
            }
        }
    }

    int inner::BuffBonus(const NightDef &def, const NightState &state, int guest, StatKind kind)
    {
        if (!state.battle.active || (kind != StatKind::Might && kind != StatKind::Sanity))
            return 0;
        for (int f = 0; f < state.battle.count; ++f)
            if (state.battle.fighters[U(f)].guest == guest)
                return BuffOn(def, state.battle.fighters[U(f)], kind == StatKind::Might ? BuffStat::Might : BuffStat::Sanity);
        return 0;
    }

    int Damage(const NightDef &def, const NightState &state, const Fighter &attacker, const Fighter &defender)
    {
        const auto &rules = def.rules.battle;
        const Weapon type = AttackType(def, state, attacker);
        const StatKind stat = type == Weapon::Physical ? StatKind::Might : StatKind::Sanity;
        // 1. base + attack - defense / divisor, at least minDamage.
        int damage = std::max(rules.minDamage, rules.base + FighterStat(def, state, attacker, stat) - FighterStat(def, state, defender, stat) / rules.defenseDivisor);
        // 2. Halved by a resisting monster; less by worn armour against physical; divided when guarded.
        if (defender.monster >= 0 && def.monsters[U(state.monsters[U(defender.monster)].kind)].resist == static_cast<int>(type))
            damage /= 2;
        if (defender.guest >= 0 && type == Weapon::Physical)
            for (std::int8_t i : state.guests[U(defender.guest)].items)
                if (i >= 0 && def.items[U(i)].effect == Effect::Armor)
                    damage -= def.items[U(i)].amount;
        if (HasGuard(def, defender))
            damage /= rules.guardDivisor;
        return std::max(rules.minDamage, damage);
    }

    std::vector<Fighter> inner::Side(const NightState &state, int guest)
    {
        std::vector<Fighter> side{Fighter{I8(guest)}};
        const Guest &me = state.guests[U(guest)];
        for (int o = 0; o < state.count; ++o)
        {
            const Guest &ally = state.guests[U(o)];
            if (o != guest && me.group >= 0 && ally.group == me.group && ally.cell == me.cell && !ally.down && !ally.dead)
                side.push_back(Fighter{I8(o)});
        }
        return side;
    }

    void inner::BeginRounds(const NightDef &def, NightState &state)
    {
        state.battle.turn = -1;
        Advance(def, state);
    }

    void inner::StartBattle(const NightDef &def, NightState &state, int cell, const std::vector<Fighter> &starters, const std::vector<Fighter> &others,
                            std::uint8_t asking)
    {
        Battle &b = state.battle;
        b = {};
        b.active = true;
        b.cell = I8(cell);
        b.owner = I8(BoardGuest(state));
        for (Fighter f : starters)
            f.side = 0, b.fighters[U(b.count++)] = f;
        for (Fighter f : others)
            if (b.count < 8)
                f.side = 1, b.fighters[U(b.count++)] = f;
        Log(state, LogKind::Battle, starters.front().guest, others.front().guest, cell);
        // Bystanders choose first; then the first fighter (perhaps a monster) acts.
        b.asking = asking;
        if (!asking)
            BeginRounds(def, state);
    }

    void inner::BattleLegal(const NightDef &def, const NightState &state, std::vector<Action> &legal)
    {
        const Battle &b = state.battle;
        if (b.asking)
        {
            legal.push_back({ActionKind::Join});
            legal.push_back({ActionKind::EndTurn});
            return;
        }
        const Fighter &me = b.fighters[U(b.turn)];
        for (int f = 0; f < b.count; ++f)
            if (b.fighters[U(f)].side != me.side && Standing(state, b.fighters[U(f)]))
                legal.push_back({ActionKind::Strike, I8(f)});
        const Guest &guest = state.guests[U(me.guest)];
        for (std::int8_t s = 0; s < maxCarry; ++s)
            if (guest.items[U(s)] >= 0)
            {
                const ItemKind kind = def.items[U(guest.items[U(s)])].kind;
                if (kind == ItemKind::Heal || kind == ItemKind::Boost)
                    legal.push_back({ActionKind::UseItem, s});
            }
        if (GuardBuff(def) >= 0)
            legal.push_back({ActionKind::Guard});
        if (b.round > 1)
            legal.push_back({ActionKind::Leave});
    }

    void inner::BattleApply(const NightDef &def, NightState &state, const Action &action)
    {
        Battle &b = state.battle;
        if (b.asking)
        {
            // A bystander joins the defender's side or stays out; neither uses their own turn.
            const int g = Current(state);
            if (action.kind == ActionKind::Join && b.count < 8)
            {
                Fighter f{I8(g)};
                f.side = 1;
                b.fighters[U(b.count++)] = f;
            }
            b.asking = static_cast<std::uint8_t>(b.asking & ~(1u << g));
            if (!b.asking)
                BeginRounds(def, state);
            return;
        }
        Fighter &me = At(state, state.battle.turn);
        Guest &guest = state.guests[U(me.guest)];
        switch (action.kind)
        {
        case ActionKind::Strike:
            Strike(def, state, state.battle.turn, action.a);
            break;
        case ActionKind::Guard:
            AddBuff(def, me, GuardBuff(def));
            break;
        case ActionKind::UseItem:
        {
            const ItemDef &item = def.items[U(guest.items[U(action.a)])];
            if (item.kind == ItemKind::Boost)
                AddBuff(def, me, item.buff);
            else
                for (int f = 0; f < state.battle.count; ++f) // a heal: self, or the whole side for a room heal
                {
                    const Fighter &o = state.battle.fighters[U(f)];
                    if (o.guest >= 0 && (o.guest == me.guest || (item.effect == Effect::HpRoom && o.side == me.side)) && Standing(state, o))
                        HealGuest(def, state, o.guest, item.amount);
                }
            guest.items[U(action.a)] = -1;
            break;
        }
        case ActionKind::Leave:
            return EndBattle(def, state);
        default:
            return;
        }
        Advance(def, state);
    }

    void inner::RoundStart(const NightDef &def, NightState &state)
    {
        // 0. Water and seals that have run their rounds are gone.
        for (Mark &m : state.flooded)
            if (m.cell >= 0 && m.untilRound < state.round)
                m = {};
        for (Mark &m : state.sealed)
            if (m.cell >= 0 && m.untilRound < state.round)
                m = {};
        // 1. Monsters whose time is up leave.
        state.monsters.erase(std::remove_if(state.monsters.begin(), state.monsters.end(),
                                            [&](const Monster &m) { return m.leavesRound <= state.round || m.hp <= 0; }),
                             state.monsters.end());
        // 2. Perhaps one appears, grown with the hour, in an opened room without a guest.
        const auto &rules = def.rules.monsters;
        if (state.round < rules.fromRound || static_cast<int>(state.monsters.size()) >= rules.max || def.monsters.empty() ||
            Below(state, 100) >= rules.chancePercent)
            return;
        std::vector<int> cells;
        for (int c = 0; c < maxCells; ++c)
            if (state.cells[U(c)].opened && MonsterAt(state, c) < 0 &&
                std::none_of(state.guests.begin(), state.guests.begin() + state.count, [&](const Guest &g) { return g.cell == c; }))
                cells.push_back(c);
        if (cells.empty())
            return;
        const int kind = Below(state, static_cast<int>(def.monsters.size()));
        const MonsterDef &m = def.monsters[U(kind)];
        const int hours = (state.round - 1) * 30 / 60;
        const int cell = cells[U(Below(state, static_cast<int>(cells.size())))];
        state.monsters.push_back({I8(kind), I8(cell), I8(state.round + rules.stayRounds), I16(m.hp + m.hpPerHour * hours),
                                  I16(m.might + m.statPerHour * hours), I16(m.sanity + m.statPerHour * hours)});
        Log(state, LogKind::Monster, -1, -1, kind, cell);
    }
}
