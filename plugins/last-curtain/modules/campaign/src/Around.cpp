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
}
