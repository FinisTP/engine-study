#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>

// How a monster picks its battle ability (Match.md "The monsters' phase").
namespace umbra::match
{
    using namespace inner;

    namespace
    {
        // A strike's damage, with dice at their average.
        int Blow(const MatchDef &def, const MatchState &s, int ability, int from, int to)
        {
            const Dice &d = def.abilities[U(ability)].dice;
            return Damage(def, s, ability, from, to, d.count * (d.sides + 1) / 2);
        }

        // The hardest blow `from` can deal `to` next.
        int Best(const MatchDef &def, const MatchState &s, int from, int to)
        {
            int best = 0;
            for (std::int8_t a : s.units[U(from)].abilities)
                if (a >= 0 && def.abilities[U(a)].use == Use::Battle && def.abilities[U(a)].kind == AbilityClass::Strike &&
                    s.units[U(from)].hp > def.abilities[U(a)].hpCost)
                    best = std::max(best, Blow(def, s, a, from, to));
            return best;
        }
    }

    int MonsterPick(const MatchDef &def, const MatchState &s, int unit)
    {
        const Unit &u = s.units[U(unit)];
        const int foe = unit == s.battle.attacker ? s.battle.defender : s.battle.attacker;
        const bool threatened = Best(def, s, foe, unit) >= u.hp, hurt = u.hp * 2 < u.maxHp;
        // 1. Each usable ability scores: a strike its damage (a kill above all), a shield or heal only when it helps.
        int pick = 0, bestValue = -1;
        for (int slot = 0; slot < maxAbilities; ++slot)
        {
            const int id = u.abilities[U(slot)];
            if (id < 0 || def.abilities[U(id)].use != Use::Battle || u.hp <= def.abilities[U(id)].hpCost)
                continue;
            const AbilityDef &a = def.abilities[U(id)];
            int value = 0;
            if (a.kind == AbilityClass::Strike)
                value = Blow(def, s, id, unit, foe) >= s.units[U(foe)].hp ? 1000 : Blow(def, s, id, unit, foe);
            else if (a.kind == AbilityClass::Shield && threatened)
                value = 50 + a.power;
            else if (a.kind == AbilityClass::Heal && (hurt || threatened))
                value = 50 + std::min(a.power, u.maxHp - u.hp);
            if (value > bestValue)
                pick = slot, bestValue = value;
        }
        return pick;
    }
}
