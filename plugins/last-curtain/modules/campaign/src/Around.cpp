#include <CampaignCafe.hpp>

#include <algorithm>

namespace umbra::campaign
{
    PreStage PreStageFor(const match::MatchDef &def, const Cast &cast, const Growth &growth, const Save &save, const StageDef &stage, const std::vector<std::string> &wanted)
    {
        PreStage p;
        p.cap = stage.partyCap;
        // 1. The player first, then the wanted ones in the roster, up to the cap.
        p.party.push_back("player");
        for (const std::string &who : wanted)
            if (who != "player" && static_cast<int>(p.party.size()) < p.cap && std::ranges::any_of(save.roster, [&](const Member &m) { return m.id == who; }))
                p.party.push_back(who);
        // 2. Can anyone revive (an ability or a slot item)?
        bool revive = false;
        for (const std::string &who : p.party)
        {
            const auto m = std::ranges::find(save.roster, who, &Member::id);
            const match::Unit u = MakeUnit(def, cast, growth, who, m == save.roster.end() ? stage.level : m->level, 0, false);
            for (const int a : u.abilities)
                revive |= a >= 0 && def.abilities[static_cast<std::size_t>(a)].kind == match::AbilityClass::Revive;
            if (save.slots.contains(who))
                for (const std::string &item : save.slots.at(who))
                    for (const match::ItemDef &it : def.items)
                        revive |= it.id == item && it.kind == match::ItemClass::Revive;
        }
        p.reviveWarning = !revive;
        return p;
    }

    Result ResultOf(const Save &before, const Save &after)
    {
        Result r;
        for (const Member &m : after.roster)
            if (const auto b = std::ranges::find(before.roster, m.id, &Member::id); b != before.roster.end() && m.level > b->level)
                r.levels.push_back(m.id);
        for (const auto &[item, count] : after.storage)
            if (const int had = before.storage.contains(item) ? before.storage.at(item) : 0; count > had)
                r.found[item] = count - had;
        for (const auto &[key, value] : after.facts)
            if (key.starts_with("freed:") && !before.facts.contains(key))
                r.freed.push_back(key.substr(6));
        return r;
    }

    Over OverFor(const Save &save, const StageDef &stage, const match::MatchState &s)
    {
        Over o;
        o.reason = s.loss == match::Loss::Trapped ? "trapped" : s.loss == match::Loss::PlayerFell ? "playerFell" : "sideDown";
        o.retryOnly = stage.id == "t1";
        o.rite = s.loss == match::Loss::PlayerFell && save.riteUnlocked && !save.riteUsed && !o.retryOnly;
        return o;
    }

    std::vector<PartyMember> PartyOf(const Save &save, const std::vector<std::string> &who)
    {
        std::vector<PartyMember> out;
        for (const std::string &id : who)
        {
            const auto m = std::ranges::find(save.roster, id, &Member::id);
            out.push_back({id, m == save.roster.end() ? 1 : m->level, save.slots.contains(id) ? save.slots.at(id) : std::array<std::string, 4>{}});
        }
        return out;
    }

    void ApplySave(const match::MatchDef &def, const Recipes &recipes, const Save &save, BuiltStage &built)
    {
        const auto find = [&](const std::string &id) { return static_cast<int>(std::ranges::find(def.abilities, id, &match::AbilityDef::id) - def.abilities.begin()); };
        for (std::size_t i = 0; i < built.state.units.size() && i < built.who.size(); ++i)
        {
            match::Unit &u = built.state.units[i];
            if (u.faction != 0)
                continue;
            // 1. Versions in place of base abilities.
            for (auto &a : u.abilities)
                if (a >= 0)
                    if (const std::string chosen = AbilityFor(save, def.abilities[static_cast<std::size_t>(a)].id); chosen != def.abilities[static_cast<std::size_t>(a)].id)
                        a = static_cast<std::int8_t>(find(chosen));
            // 2. Boosts.
            const int hp = Boosted(save, recipes, built.who[i], "maxHp");
            u.maxHp = static_cast<std::int16_t>(u.maxHp + hp), u.hp = static_cast<std::int16_t>(u.hp + hp);
            u.attack = static_cast<std::int16_t>(u.attack + Boosted(save, recipes, built.who[i], "attack"));
        }
    }
}
