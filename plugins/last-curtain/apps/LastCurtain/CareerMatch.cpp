#include "CareerMatch.hpp"

namespace umbra::night
{
    using namespace umbra::match;

    void ApplyCareer(const MatchDef &def, MatchState &s, const cafe::Career &career)
    {
        for (Unit &u : s.units)
        {
            if (u.kind != 0 || u.faction != 0)
                continue;
            const cafe::CharacterProgress *p = career.Find(def.characters[static_cast<std::size_t>(u.def)].id);
            if (!p)
                continue;
            // 1. Level and stats as they were (0 keeps the data's), whole at the start.
            const auto set = [](std::int16_t &field, int value) { if (value > 0) field = static_cast<std::int16_t>(value); };
            u.level = static_cast<std::int8_t>(p->level), u.xp = static_cast<std::int16_t>(p->xp);
            set(u.maxHp, p->maxHp), set(u.attack, p->attack), set(u.defence, p->defence), set(u.speed, p->speed), set(u.move, p->move);
            u.hp = u.maxHp;
            // 2. Abilities by id, in slot order (an id the data no longer has is skipped), and the upgrades.
            if (!p->abilities.empty())
            {
                u.abilities.fill(-1);
                std::size_t slot = 0;
                for (const std::string &id : p->abilities)
                    if (const int a = MatchDef::Find(def.abilities, id); a >= 0 && slot < u.abilities.size())
                        u.abilities[slot++] = static_cast<std::int8_t>(a);
            }
            for (std::size_t i = 0; i < u.boost.size(); ++i)
                u.boost[i] = static_cast<std::int8_t>(p->boost[i]), u.cut[i] = static_cast<std::int8_t>(p->cut[i]);
        }
        if (s.now.origin >= 0 && Current(s) >= 0) // the first turn's moves follow a changed Move
            s.now.movesLeft = static_cast<std::int8_t>(s.units[static_cast<std::size_t>(Current(s))].move);
    }

    int RecordMatch(const MatchDef &def, const MatchState &s, cafe::Career &career, int moneyPerEssence)
    {
        for (const Unit &u : s.units)
        {
            if (u.kind != 0 || u.faction != 0)
                continue;
            const std::string &id = def.characters[static_cast<std::size_t>(u.def)].id;
            cafe::CharacterProgress *p = career.Find(id);
            if (!p)
                p = &career.characters.emplace_back(), p->id = id;
            p->level = u.level, p->xp = u.xp, p->maxHp = u.maxHp, p->attack = u.attack, p->defence = u.defence, p->speed = u.speed, p->move = u.move;
            p->abilities.clear();
            for (std::int8_t a : u.abilities)
                if (a >= 0)
                    p->abilities.push_back(def.abilities[static_cast<std::size_t>(a)].id);
            for (std::size_t i = 0; i < u.boost.size(); ++i)
                p->boost[i] = u.boost[i], p->cut[i] = u.cut[i];
        }
        const int earned = std::max(0, s.score[0]) * moneyPerEssence;
        career.money += earned, ++career.day;
        return earned;
    }
}
