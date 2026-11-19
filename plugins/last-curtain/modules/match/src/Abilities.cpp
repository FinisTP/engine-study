#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>

// Levels, board abilities, items and events (Match.md "Levels, abilities, items, events").
namespace umbra::match
{
    using namespace inner;

    namespace
    {
        void Learn(Unit &u, int ability)
        {
            if (std::ranges::find(u.abilities, ability) != u.abilities.end())
                return;
            if (auto free = std::ranges::find(u.abilities, std::int8_t(-1)); free != u.abilities.end())
                *free = static_cast<std::int8_t>(ability);
        }

        void Take(const MatchDef &def, MatchState &s, int me, const Outcome &o)
        {
            Unit &u = s.units[U(me)];
            switch (o.effect)
            {
            case Effect::Hp:
                u.hp = static_cast<std::int16_t>(std::clamp<int>(u.hp + o.amount, 0, u.maxHp));
                if (u.hp == 0)
                    Down(def, s, me, -1);
                break;
            case Effect::Essence: u.essence = std::max(0, u.essence + o.amount); break;
            case Effect::Xp: GainXp(def, s, me, o.amount); break;
            case Effect::Item:
                if (auto free = std::ranges::find(u.items, std::int8_t(-1)); free != u.items.end() && free - u.items.begin() < def.rules.carry)
                    *free = static_cast<std::int8_t>(o.amount);
                break;
            case Effect::None: break;
            }
        }

        bool Ready(const MatchState &s, int me) { return !s.now.acted && s.units[U(me)].cell >= 0; }
    }

    namespace inner
    {
        void GainXp(const MatchDef &def, MatchState &s, int unit, int amount)
        {
            Unit &u = s.units[U(unit)];
            if (u.kind != 0)
                return;
            u.xp = static_cast<std::int16_t>(u.xp + amount);
            // 1. Each XP step passed: a level, more HP, and the job's ability for that level (or a choice).
            const auto &steps = def.rules.levels.xp;
            while (u.pending < 0 && u.level - 1 < static_cast<int>(steps.size()) && u.xp >= steps[U(u.level - 1)])
            {
                ++u.level;
                u.maxHp = static_cast<std::int16_t>(u.maxHp + def.rules.levels.hp);
                u.hp = static_cast<std::int16_t>(u.hp + def.rules.levels.hp);
                Log(s, LogKind::Level, unit, u.level);
                const Role role = def.characters[U(u.def)].role;
                for (int j = 0; j < static_cast<int>(def.jobs.size()); ++j)
                    if (def.jobs[U(j)].role == role && def.jobs[U(j)].level == u.level)
                    {
                        if (def.jobs[U(j)].choice >= 0 && u.pending < 0)
                            u.pending = static_cast<std::int8_t>(j);
                        else
                            Learn(u, def.jobs[U(j)].ability);
                    }
            }
        }

        Why CheckLevelUp(const MatchDef &, const MatchState &s, int me, int choice)
        {
            return s.units[U(me)].pending >= 0 && (choice == 0 || choice == 1) ? Why::Ok : Why::NothingHere;
        }

        void LevelUp(const MatchDef &def, MatchState &s, int me, int choice)
        {
            Unit &u = s.units[U(me)];
            const JobLevel &j = def.jobs[U(u.pending)];
            Learn(u, choice == 0 ? j.ability : j.choice);
            u.pending = -1;
            GainXp(def, s, me, 0); // a later level may be waiting too
        }

        Why CheckBoardAbility(const MatchDef &def, const MatchState &s, int me, int slot, int target)
        {
            const Unit &u = s.units[U(me)];
            if (!Ready(s, me))
                return Why::AlreadyActed;
            if (slot < 0 || slot >= maxAbilities || u.abilities[U(slot)] < 0)
                return Why::NothingHere;
            const AbilityDef &a = def.abilities[U(u.abilities[U(slot)])];
            if (a.use != Use::Board)
                return Why::NothingHere;
            if (target < 0 || target >= static_cast<int>(s.units.size()) || s.units[U(target)].cell < 0 || s.units[U(target)].hp <= 0)
                return Why::NothingHere;
            const Unit &t = s.units[U(target)];
            // 1. Hit an enemy, heal your own side (yourself included), within range.
            if ((a.kind == AbilityClass::Hit) == (t.faction == u.faction) || (a.kind == AbilityClass::Heal && t.kind != 0))
                return Why::OwnSide;
            const int d = StepsTo(s.board, u.cell, t);
            if (d < 0 || d > a.range)
                return Why::OutOfRange;
            if (u.hp <= a.hpCost)
                return Why::TooCostly;
            return Why::Ok;
        }

        void BoardAbility(const MatchDef &def, MatchState &s, int me, int slot, int target)
        {
            Unit &u = s.units[U(me)];
            const int ability = u.abilities[U(slot)];
            const AbilityDef &a = def.abilities[U(ability)];
            u.hp = static_cast<std::int16_t>(u.hp - a.hpCost);
            Unit &t = s.units[U(target)];
            if (a.kind == AbilityClass::Heal)
                t.hp = static_cast<std::int16_t>(std::min<int>(t.maxHp, t.hp + a.power));
            else
            {
                int roll = 0;
                for (int i = 0; i < a.dice.count; ++i)
                    roll += 1 + Roll(s, a.dice.sides);
                const int dmg = Damage(def, s, ability, me, target, roll);
                t.hp = static_cast<std::int16_t>(std::max(0, t.hp - dmg));
                Log(s, LogKind::Blow, me, target, dmg);
                if (t.hp == 0)
                    Down(def, s, target, me);
            }
            Acted(def, s);
        }

        Why CheckUseItem(const MatchDef &, const MatchState &s, int me, int slot)
        {
            if (!Ready(s, me))
                return Why::AlreadyActed;
            return slot >= 0 && slot < maxItems && s.units[U(me)].items[U(slot)] >= 0 ? Why::Ok : Why::NothingHere;
        }

        void UseItem(const MatchDef &def, MatchState &s, int me, int slot)
        {
            Unit &u = s.units[U(me)];
            const ItemDef &item = def.items[U(u.items[U(slot)])];
            Log(s, LogKind::Item, me, u.items[U(slot)]); // b: the items.csv row, as when picked up
            u.items[U(slot)] = -1;
            if (item.kind == ItemClass::HealItem)
                u.hp = static_cast<std::int16_t>(std::min<int>(u.maxHp, u.hp + item.amount));
            else
                GainXp(def, s, me, item.amount);
            Acted(def, s);
        }

        Why CheckAnswer(const MatchDef &, const MatchState &s, int me, int choice)
        {
            if (!Ready(s, me))
                return Why::AlreadyActed;
            const Cell &c = s.board.cells[U(s.units[U(me)].cell)];
            return c.event >= 0 && !c.resolved && (choice == 0 || choice == 1) ? Why::Ok : Why::NothingHere;
        }

        void Answer(const MatchDef &def, MatchState &s, int me, int choice)
        {
            Cell &c = s.board.cells[U(s.units[U(me)].cell)];
            const Choice &ch = def.events[U(c.event)].choices[U(choice)];
            c.resolved = true;
            // 1. Roll the chance (0-99 under it succeeds), then the outcome.
            const bool passed = Roll(s, 100) < ch.chance;
            Log(s, LogKind::Event, me, choice, passed);
            Take(def, s, me, passed ? ch.success : ch.failure);
            if (s.units[U(me)].cell >= 0)
                Acted(def, s);
            else if (!s.over)
                NextTurn(def, s);
        }
    }
}
