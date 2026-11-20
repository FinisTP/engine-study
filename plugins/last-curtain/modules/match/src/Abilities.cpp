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
        int PowerOf(const MatchDef &def, const Unit &u, int slot) { return def.abilities[U(u.abilities[U(slot)])].power + u.boost[U(slot)]; }
        int CostOf(const MatchDef &def, const Unit &u, int slot) { return std::max(0, def.abilities[U(u.abilities[U(slot)])].hpCost - u.cut[U(slot)]); }

        void Deal(const MatchDef &def, MatchState &s, int unit)
        {
            // 1. What may be offered: the role's abilities of levels 2..now not yet known, slots that can be upgraded.
            const Unit &u = s.units[U(unit)];
            const Role role = def.characters[U(u.def)].role;
            std::vector<int> learn, slots;
            for (const JobLevel &j : def.jobs)
                if (j.role == role && j.level >= 2 && j.level <= u.level)
                    for (const int a : {j.ability, j.choice})
                        if (a >= 0 && std::ranges::find(u.abilities, a) == u.abilities.end() && std::ranges::find(learn, a) == learn.end())
                            learn.push_back(a);
            for (int slot = 0; slot < maxAbilities; ++slot)
                if (u.abilities[U(slot)] >= 0 && def.abilities[U(u.abilities[U(slot)])].use != Use::Passive)
                    slots.push_back(slot);
            // 2. Cards drawn by weight until there are enough, never the same card twice.
            int total = 0;
            for (const LevelCardDef &c : def.levelCards)
                total += c.weight;
            s.offer.clear();
            for (int tries = 0; tries < 64 && static_cast<int>(s.offer.size()) < def.rules.levels.cards; ++tries)
            {
                int r = Roll(s, total), row = 0;
                while (r >= def.levelCards[U(row)].weight)
                    r -= def.levelCards[U(row++)].weight;
                const LevelCardDef &c = def.levelCards[U(row)];
                LevelCard card;
                if (c.kind == CardKind::Learn && !learn.empty())
                    card = {0, static_cast<std::int8_t>(learn[U(Roll(s, static_cast<int>(learn.size())))]), 0};
                else if (c.kind == CardKind::Stat)
                    card = {1, static_cast<std::int8_t>(c.what), static_cast<std::int8_t>(c.amount)};
                else if (c.kind == CardKind::Upgrade && !slots.empty())
                {
                    const int slot = slots[U(Roll(s, static_cast<int>(slots.size())))];
                    if (c.what == Stat::Cost && CostOf(def, u, slot) == 0)
                        continue;
                    card = {2, static_cast<std::int8_t>(slot), static_cast<std::int8_t>(c.what == Stat::Cost ? 1 : 0)};
                }
                else
                    continue;
                if (std::ranges::find(s.offer, card) == s.offer.end())
                    s.offer.push_back(card);
            }
        }

        void GainXp(const MatchDef &def, MatchState &s, int unit, int amount)
        {
            Unit &u = s.units[U(unit)];
            if (u.kind != 0)
                return;
            u.xp = static_cast<std::int16_t>(u.xp + amount);
            // 1. The next XP step passed: a level, more HP, and its cards (one level at a time; LevelUp asks again).
            const auto &steps = def.rules.levels.xp;
            if (u.pending < 0 && s.offer.empty() && u.level - 1 < static_cast<int>(steps.size()) && u.xp >= steps[U(u.level - 1)])
            {
                ++u.level;
                u.maxHp = static_cast<std::int16_t>(u.maxHp + def.rules.levels.hp);
                u.hp = static_cast<std::int16_t>(u.hp + def.rules.levels.hp);
                Log(s, LogKind::Level, unit, u.level);
                Deal(def, s, unit);
                if (!s.offer.empty())
                    u.pending = 0;
            }
        }

        Why CheckLevelUp(const MatchDef &, const MatchState &s, int me, int choice)
        {
            return s.units[U(me)].pending >= 0 && choice >= 0 && choice < static_cast<int>(s.offer.size()) ? Why::Ok : Why::NothingHere;
        }

        void LevelUp(const MatchDef &def, MatchState &s, int me, int choice)
        {
            // 1. The card: learn, a stat, or an upgrade of one ability.
            Unit &u = s.units[U(me)];
            const LevelCard c = s.offer[U(choice)];
            if (c.kind == 0)
                Learn(u, c.a);
            else if (c.kind == 1)
            {
                std::int16_t *stat[] = {&u.maxHp, &u.attack, &u.defence, &u.speed, &u.move};
                *stat[U(c.a)] = static_cast<std::int16_t>(*stat[U(c.a)] + c.b);
                if (c.a == static_cast<int>(Stat::Hp))
                    u.hp = static_cast<std::int16_t>(u.hp + c.b);
            }
            else if (c.b == 0)
                u.boost[U(c.a)] = static_cast<std::int8_t>(u.boost[U(c.a)] + def.rules.levels.upgradePower);
            else
                u.cut[U(c.a)] = static_cast<std::int8_t>(u.cut[U(c.a)] + def.rules.levels.upgradeCost);
            u.pending = -1;
            s.offer.clear();
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
            if (u.hp <= CostOf(def, u, slot))
                return Why::TooCostly;
            return Why::Ok;
        }

        void BoardAbility(const MatchDef &def, MatchState &s, int me, int slot, int target)
        {
            Unit &u = s.units[U(me)];
            const int ability = u.abilities[U(slot)];
            const AbilityDef &a = def.abilities[U(ability)];
            u.hp = static_cast<std::int16_t>(u.hp - CostOf(def, u, slot));
            Unit &t = s.units[U(target)];
            if (a.kind == AbilityClass::Heal)
            {
                const int before = t.hp;
                t.hp = static_cast<std::int16_t>(std::min<int>(t.maxHp, t.hp + PowerOf(def, u, slot)));
                Log(s, LogKind::Healed, me, target, t.hp - before);
            }
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
            GainXp(def, s, me, def.rules.levels.abilityXp); // using an ability teaches (the user, 2026-11-20)
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
            RoomBack(def, s, s.units[U(me)].cell, 0);
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
