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

        bool GroupStanding(const MatchState &s, int group) { return Standing(s, group * 2) || Standing(s, group * 2 + 1); }

        // The round's order: first strike first, then speed, the attackers first on a tie.
        void OrderRound(const MatchDef &def, MatchState &s)
        {
            std::vector<int> places;
            for (int p = 0; p < 4; ++p)
                if (Standing(s, p))
                    places.push_back(p);
            std::ranges::stable_sort(places, [&](int x, int y) {
                const Unit &a = s.units[U(s.battle.fighters[U(x)])], &b = s.units[U(s.battle.fighters[U(y)])];
                return std::tuple{!HasFirstStrike(def, a), -a.speed, x / 2} < std::tuple{!HasFirstStrike(def, b), -b.speed, y / 2};
            });
            s.battle.order.fill(-1);
            for (std::size_t i = 0; i < places.size(); ++i)
                s.battle.order[i] = static_cast<std::int8_t>(places[i]);
            s.battle.next = 0;
        }

        void EndBattle(const MatchDef &def, MatchState &s)
        {
            s.battle.on = false;
            if (s.over)
                return;
            // 1. The turn belongs to whoever holds it in order: a monster's (a hunter's battle) or one that
            //    went down passes on; else the character's action is spent.
            if (const Unit &holder = s.units[U(s.order[U(s.turn)])]; holder.kind == 1 || !Up(holder))
                NextTurn(def, s);
            else
                Acted(def, s);
        }

        // Monsters answer at once, so Current never waits on one.
        void MonstersPick(const MatchDef &def, MatchState &s)
        {
            while (s.battle.on)
            {
                const int next = Picker(s);
                if (s.units[U(next)].kind != 1)
                    return;
                Pick(def, s, next, MonsterPick(def, s, next), MonsterTarget(def, s, next));
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

        bool Beside(const Board &b, const Unit &a, const Unit &c)
        {
            for (int x : Squares(b, a))
                for (int y : Squares(b, c))
                    if (Adjacent(b, x, y))
                        return true;
            return false;
        }

        int StepsTo(const Board &b, int from, const Unit &t)
        {
            int best = -1;
            for (int c : Squares(b, t))
                if (const int d = Steps(b, from, c); d >= 0 && (best < 0 || d < best))
                    best = d;
            return best;
        }

        bool Room(const MatchDef &def, const MatchState &s, int cell, int faction)
        {
            int there = 0;
            for (const Unit &o : s.units)
                if (Covers(s.board, o, cell))
                {
                    if (o.faction != faction)
                        return false; // never with a foe (wild monsters are their own side)
                    ++there;
                }
            return there < (cell == s.stage ? 1 : def.rules.board.squareCapacity);
        }

        int StraightTo(const Board &b, int from, const Unit &t)
        {
            // Squares counted straight, walls ignored (Manhattan), to the nearest square it covers.
            int best = -1;
            for (int c : Squares(b, t))
                if (const int d = std::abs(c % b.width - from % b.width) + std::abs(c / b.width - from / b.width); best < 0 || d < best)
                    best = d;
            return best;
        }

        Why CheckAttack(const MatchDef &, const MatchState &s, int me, int target)
        {
            if (s.now.acted)
                return Why::AlreadyActed;
            if (target < 0 || target >= static_cast<int>(s.units.size()))
                return Why::NothingHere;
            const Unit &u = s.units[U(me)], &t = s.units[U(target)];
            if (Fallen(t))
                return Why::Fallen;
            if (!Up(t))
                return Why::NothingHere;
            if (!Beside(s.board, u, t))
                return Why::OutOfRange;
            if (t.faction == u.faction)
                return Why::OwnSide;
            return Why::Ok;
        }

        // A unit's place among the fighters, -1 if it is not one; standing: still in the fight.
        int Place(const MatchState &s, int unit)
        {
            const auto it = std::ranges::find(s.battle.fighters, unit);
            return unit >= 0 && it != s.battle.fighters.end() ? static_cast<int>(it - s.battle.fighters.begin()) : -1;
        }
        bool Standing(const MatchState &s, int place)
        {
            const int f = s.battle.fighters[U(place)];
            return f >= 0 && Up(s.units[U(f)]);
        }
        int Picker(const MatchState &s) { return s.battle.fighters[U(s.battle.order[U(s.battle.next)])]; }

        int FirstFoe(const MatchState &s, int me)
        {
            const int group = 1 - Place(s, me) / 2;
            for (int p : {group * 2, group * 2 + 1})
                if (Standing(s, p))
                    return s.battle.fighters[U(p)];
            return -1;
        }

        Why CheckBattleAbility(const MatchDef &def, const MatchState &s, int me, int slot, int target)
        {
            const AbilityDef *a = Slot(def, s.units[U(me)], slot);
            if (!a || a->use != Use::Battle)
                return Why::NothingHere;
            // A strike goes at a standing fighter of the other group (none named: the first one standing).
            if (a->kind == AbilityClass::Strike && target >= 0 && (Place(s, target) < 0 || Place(s, target) / 2 == Place(s, me) / 2 || !Standing(s, Place(s, target))))
                return Why::OwnSide;
            if (s.units[U(me)].hp <= CostOf(def, s.units[U(me)], slot))
                return Why::TooCostly;
            return Why::Ok;
        }

        void StartBattle(const MatchDef &def, MatchState &s, int me, int target)
        {
            // 1. Each group: the one and whoever of its side shares its tile (up to two).
            s.battle = Battle{};
            s.battle.on = true, s.battle.attacker = static_cast<std::int8_t>(me), s.battle.defender = static_cast<std::int8_t>(target), s.battle.round = 1;
            for (int group = 0; group < 2; ++group)
            {
                const int lead = group == 0 ? me : target;
                s.battle.fighters[U(group * 2)] = static_cast<std::int8_t>(lead);
                for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
                    if (const Unit &o = s.units[U(i)]; i != lead && o.hp > 0 && o.faction == s.units[U(lead)].faction &&
                                                       std::ranges::any_of(Squares(s.board, s.units[U(lead)]), [&](int c) { return Covers(s.board, o, c); }))
                    {
                        s.battle.fighters[U(group * 2 + 1)] = static_cast<std::int8_t>(i);
                        break;
                    }
            }
            Log(s, LogKind::Battle, me, target);
            OrderRound(def, s);
            MonstersPick(def, s);
        }

        void Pick(const MatchDef &def, MatchState &s, int me, int slot, int target)
        {
            const AbilityDef &a = *Slot(def, s.units[U(me)], slot);
            Unit &u = s.units[U(me)];
            const int foe = target >= 0 ? target : FirstFoe(s, me);
            u.hp = static_cast<std::int16_t>(u.hp - CostOf(def, u, slot));
            // 1. The ability: a blow, a shield for the rest of the battle, or healing.
            if (a.kind == AbilityClass::Strike)
            {
                const int roll = a.dice.count > 0 ? [&] { int r = 0; for (int i = 0; i < a.dice.count; ++i) r += 1 + Roll(s, a.dice.sides); return r; }() : 0;
                const int dmg = Damage(def, s, u.abilities[U(slot)], me, foe, roll);
                s.units[U(foe)].hp = static_cast<std::int16_t>(std::max(0, s.units[U(foe)].hp - dmg));
                Log(s, LogKind::Blow, me, foe, dmg);
                if (s.units[U(foe)].hp <= 0)
                    Down(def, s, foe, me);
                if (!s.battle.on)
                    return;
            }
            else if (a.kind == AbilityClass::Shield)
                s.battle.shield[U(Place(s, me))] = static_cast<std::int16_t>(s.battle.shield[U(Place(s, me))] + PowerOf(def, u, slot));
            else if (a.kind == AbilityClass::Heal)
                u.hp = static_cast<std::int16_t>(std::min<int>(u.maxHp, u.hp + PowerOf(def, u, slot)));
            // 2. The next standing fighter in the round's order; after the last, the next round; after the last round, the end.
            do
                ++s.battle.next;
            while (s.battle.next < 4 && s.battle.order[U(s.battle.next)] >= 0 && !Standing(s, s.battle.order[U(s.battle.next)]));
            if (s.battle.next >= 4 || s.battle.order[U(s.battle.next)] < 0)
            {
                if (++s.battle.round > def.rules.battle.rounds)
                {
                    s.battle.round = static_cast<std::int8_t>(def.rules.battle.rounds); // the counter never shows past the last round
                    return EndBattle(def, s);
                }
                OrderRound(def, s);
            }
            MonstersPick(def, s);
        }

        void Down(const MatchDef &def, MatchState &s, int victim, int killer)
        {
            Unit &v = s.units[U(victim)];
            Unit *k = killer >= 0 ? &s.units[U(killer)] : nullptr;
            const bool byCharacter = k && k->kind == 0;
            v.hp = 0;
            Log(s, LogKind::Down, victim, killer, v.essence);
            // 1. Essence: to the character who downed it, else dropped where it fell (Mode::essence).
            if (s.mode.essence && byCharacter)
                k->essence += v.essence;
            else if (s.mode.essence && v.kind == 0 && v.essence > 0)
                s.floor.push_back({v.cell, -1, v.essence});
            if (v.kind == 0)
            {
                v.essence = 0;
                v.back = static_cast<std::int8_t>(s.mode.respawn ? def.rules.respawn.turns : 0);
                if (byCharacter)
                    ++k->kills, GainXp(def, s, killer, def.rules.levels.killXp);
            }
            else
            {
                const MonsterDef &m = def.monsters[U(v.def)];
                if (byCharacter)
                    GainXp(def, s, killer, m.xp);
                v.essence = 0;
                if (s.mode.respawn && ZoneOf(s.board, v.home) >= 0)
                    ScheduleSpawn(def, s, v.home);
            }
            // 2. Falls (Mode::fallsStay): it keeps its square and loses its action points; else it leaves the board.
            v.ap = 0;
            if (!s.mode.fallsStay)
                v.cell = -1;
            CheckEnd(def, s);
            if (s.battle.on && Place(s, victim) >= 0 && !GroupStanding(s, Place(s, victim) / 2)) // a whole group down: the battle is over
                EndBattle(def, s);
        }
    }

    int Damage(const MatchDef &def, const MatchState &s, int ability, int from, int to, int roll)
    {
        const AbilityDef &a = def.abilities[U(ability)];
        const int shield = s.battle.on && Place(s, to) >= 0 ? s.battle.shield[U(Place(s, to))] : 0;
        // An upgraded ability adds the unit's boost for the slot that holds it.
        const Unit &f = s.units[U(from)];
        const auto slot = std::ranges::find(f.abilities, ability);
        const int boost = slot != f.abilities.end() ? f.boost[U(static_cast<int>(slot - f.abilities.begin()))] : 0;
        const int power = (a.dice.count > 0 ? roll : a.power) + boost;
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
        f.cost = CostOf(def, u, act.a);
        const int target = act.kind == ActionKind::BattleAbility ? (!s.battle.on ? -1 : act.b >= 0 ? act.b : FirstFoe(s, me)) : act.b;
        if (a->kind == AbilityClass::Heal)
            f.healed = PowerOf(def, u, act.a);
        else if ((a->kind == AbilityClass::Strike || a->kind == AbilityClass::Hit) && target >= 0)
        {
            const int ability = u.abilities[U(act.a)];
            f.dealtMin = Damage(def, s, ability, me, target, a->dice.count);
            f.dealtMax = Damage(def, s, ability, me, target, a->dice.count * a->dice.sides);
        }
        return f;
    }
}
