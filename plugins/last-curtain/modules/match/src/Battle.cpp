#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>

// Battles, the forecast, knock-outs (Match.md "Battles").
namespace umbra::match
{
    using namespace inner;

    namespace
    {
        const AbilityDef *Slot(const MatchDef &def, const Unit &u, int slot)
        {
            if (slot < 0 || slot >= maxAbilities || u.abilities[U(slot)] < 0)
                return nullptr;
            return &def.abilities[U(u.abilities[U(slot)])];
        }

        bool HasFirstStrike(const MatchDef &def, const Unit &u)
        {
            return std::ranges::any_of(u.abilities, [&](std::int8_t a) { return a >= 0 && def.abilities[U(a)].kind == AbilityClass::FirstStrike; });
        }

        int Side(const MatchState &s, int unit) { return unit == s.battle.attacker ? 0 : 1; }
        int Foe(const MatchState &s, int unit) { return unit == s.battle.attacker ? s.battle.defender : s.battle.attacker; }

        // A monster picks its first affordable Strike, or its first ability.
        int MonsterSlot(const MatchDef &def, const Unit &u)
        {
            for (int slot = 0; slot < maxAbilities; ++slot)
                if (const AbilityDef *a = Slot(def, u, slot); a && a->kind == AbilityClass::Strike && u.hp > a->hpCost)
                    return slot;
            return 0;
        }

        void EndBattle(const MatchDef &def, MatchState &s)
        {
            s.battle.on = false;
            // 1. The turn belongs to whoever holds it in order: passed on if it went down, else its action is spent.
            if (s.units[U(s.order[U(s.turn)])].cell < 0)
                NextTurn(def, s);
            else
                Acted(def, s);
        }

        // Monsters answer at once, so Current never waits on one.
        void MonstersPick(const MatchDef &def, MatchState &s)
        {
            while (s.battle.on)
            {
                const int next = s.battle.next == 0 ? s.battle.attacker : s.battle.defender;
                if (s.units[U(next)].kind != 1)
                    return;
                Pick(def, s, next, MonsterSlot(def, s.units[U(next)]));
            }
        }
    }

    namespace inner
    {
        bool Adjacent(const Board &b, int a, int c)
        {
            for (int d = 0; d < 4; ++d)
                if (a >= 0 && Neighbour(b, a, d) == c)
                    return true;
            return false;
        }

        Why CheckAttack(const MatchDef &, const MatchState &s, int me, int target)
        {
            if (s.now.acted)
                return Why::AlreadyActed;
            if (target < 0 || target >= static_cast<int>(s.units.size()) || s.units[U(target)].hp <= 0)
                return Why::NothingHere;
            const Unit &u = s.units[U(me)], &t = s.units[U(target)];
            if (!Adjacent(s.board, u.cell, t.cell))
                return Why::OutOfRange;
            if (t.faction == u.faction)
                return Why::OwnSide;
            return Why::Ok;
        }

        Why CheckBattleAbility(const MatchDef &def, const MatchState &s, int me, int slot)
        {
            const AbilityDef *a = Slot(def, s.units[U(me)], slot);
            if (!a || a->use != Use::Battle)
                return Why::NothingHere;
            if (s.units[U(me)].hp <= a->hpCost)
                return Why::TooCostly;
            return Why::Ok;
        }

        void StartBattle(const MatchDef &def, MatchState &s, int me, int target)
        {
            const bool turned = HasFirstStrike(def, s.units[U(target)]) && !HasFirstStrike(def, s.units[U(me)]);
            s.battle = {true, static_cast<std::int8_t>(me), static_cast<std::int8_t>(target), 1, static_cast<std::int8_t>(turned), static_cast<std::int8_t>(turned), {}};
            Log(s, LogKind::Battle, me, target);
            MonstersPick(def, s);
        }

        void Pick(const MatchDef &def, MatchState &s, int me, int slot)
        {
            const AbilityDef &a = *Slot(def, s.units[U(me)], slot);
            Unit &u = s.units[U(me)];
            const int foe = Foe(s, me);
            u.hp = static_cast<std::int16_t>(u.hp - a.hpCost);
            // 1. The ability: a blow, a shield for the rest of the battle, or healing.
            if (a.kind == AbilityClass::Strike)
            {
                const int roll = a.dice.count > 0 ? [&] { int r = 0; for (int i = 0; i < a.dice.count; ++i) r += 1 + Roll(s, a.dice.sides); return r; }() : 0;
                const int dmg = Damage(def, s, u.abilities[U(slot)], me, foe, roll);
                s.units[U(foe)].hp = static_cast<std::int16_t>(std::max(0, s.units[U(foe)].hp - dmg));
                Log(s, LogKind::Blow, me, foe, dmg);
                if (s.units[U(foe)].hp <= 0)
                    return Down(def, s, foe, me);
            }
            else if (a.kind == AbilityClass::Shield)
                s.battle.shield[U(Side(s, me))] = static_cast<std::int16_t>(s.battle.shield[U(Side(s, me))] + a.power);
            else if (a.kind == AbilityClass::Heal)
                u.hp = static_cast<std::int16_t>(std::min<int>(u.maxHp, u.hp + a.power));
            // 2. The other side picks next; after both, the next round; after the last round, the end.
            if (s.battle.next == s.battle.first)
                s.battle.next = static_cast<std::int8_t>(1 - s.battle.first);
            else if (++s.battle.round > def.rules.battle.rounds)
                return EndBattle(def, s);
            else
                s.battle.next = s.battle.first;
            MonstersPick(def, s);
        }

        void Down(const MatchDef &def, MatchState &s, int victim, int killer)
        {
            Unit &v = s.units[U(victim)];
            Unit *k = killer >= 0 ? &s.units[U(killer)] : nullptr;
            const bool byCharacter = k && k->kind == 0;
            v.hp = 0;
            Log(s, LogKind::Down, victim, killer, v.essence);
            // 1. Essence: to the character who downed it, else dropped where it fell.
            if (byCharacter)
                k->essence += v.essence;
            else if (v.kind == 0 && v.essence > 0)
                s.floor.push_back({v.cell, -1, v.essence});
            if (v.kind == 0)
            {
                v.essence = 0;
                v.back = static_cast<std::int8_t>(def.rules.respawn.turns);
                if (byCharacter)
                    ++k->kills, GainXp(def, s, killer, def.rules.levels.killXp);
            }
            else
            {
                const MonsterDef &m = def.monsters[U(v.def)];
                if (byCharacter)
                    GainXp(def, s, killer, m.xp);
                v.essence = 0;
                if (ZoneOf(s.board, v.home) >= 0)
                    ScheduleSpawn(def, s, v.home);
            }
            v.cell = -1;
            if (s.battle.on && (victim == s.battle.attacker || victim == s.battle.defender))
                EndBattle(def, s);
        }
    }

    int Damage(const MatchDef &def, const MatchState &s, int ability, int from, int to, int roll)
    {
        const AbilityDef &a = def.abilities[U(ability)];
        const int shield = s.battle.on && (to == s.battle.attacker || to == s.battle.defender) ? s.battle.shield[U(Side(s, to))] : 0;
        const int power = a.dice.count > 0 ? roll : a.power;
        return std::max(def.rules.battle.minDamage, power + s.units[U(from)].attack - (s.units[U(to)].defence + shield));
    }

    Forecast Preview(const MatchDef &def, const MatchState &s, const Action &act)
    {
        const int me = Current(s);
        if (me < 0)
            return {};
        const Unit &u = s.units[U(me)];
        Forecast f;
        if (act.kind == ActionKind::UseItem)
        {
            if (act.a >= 0 && act.a < maxItems && u.items[U(act.a)] >= 0 && def.items[U(u.items[U(act.a)])].kind == ItemClass::HealItem)
                f.healed = def.items[U(u.items[U(act.a)])].amount;
            return f;
        }
        if (act.kind != ActionKind::BattleAbility && act.kind != ActionKind::BoardAbility)
            return f;
        const AbilityDef *a = Slot(def, u, act.a);
        if (!a)
            return f;
        f.cost = a->hpCost;
        const int target = act.kind == ActionKind::BattleAbility ? (s.battle.on ? Foe(s, me) : -1) : act.b;
        if (a->kind == AbilityClass::Heal)
            f.healed = a->power;
        else if ((a->kind == AbilityClass::Strike || a->kind == AbilityClass::Hit) && target >= 0)
        {
            const int ability = u.abilities[U(act.a)];
            f.dealtMin = Damage(def, s, ability, me, target, a->dice.count);
            f.dealtMax = Damage(def, s, ability, me, target, a->dice.count * a->dice.sides);
        }
        return f;
    }
}
