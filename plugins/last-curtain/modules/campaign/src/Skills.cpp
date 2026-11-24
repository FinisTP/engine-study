#include <CampaignCafe.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace umbra::campaign
{
    bool LoadSkills(const std::filesystem::path &file, const match::MatchDef &def, Skills &out, std::vector<core::Diagnostic> &d)
    {
        const std::size_t before = d.size();
        const auto error = [&](const std::string &field, const std::string &message) { d.push_back({core::Severity::Error, "campaign.skills", file.string(), field, message}); };
        const auto known = [&](const std::string &id) { return std::ranges::any_of(def.abilities, [&](const match::AbilityDef &a) { return a.id == id; }); };
        std::ifstream in(file);
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (!j.is_object() || j.value("format", "") != "umbra.lc-skills" || j.value("version", 0) != 1)
            return error("format", "expected format umbra.lc-skills, version 1"), false;
        // 1. Each ability: its unlock level (1 or more) and its story.
        Skills skills;
        const nlohmann::json list = j.value("skills", nlohmann::json::object());
        for (const auto &[id, s] : list.items())
        {
            if (!known(id))
                error("skills." + id, "\"" + id + "\" is not in abilities.csv");
            if (const int level = s.value("unlock", 1); level < 1)
                error("skills." + id + ".unlock", "expected a level of 1 or more");
            else if (level > 1)
                skills.unlock[id] = level;
            skills.lore[id] = s.value("lore", "");
        }
        // 2. Starting loadouts: known abilities, at most maxCarried.
        const nlohmann::json starts = j.value("starts", nlohmann::json::object());
        for (const auto &[who, ids] : starts.items())
        {
            std::vector<std::string> carried = ids.get<std::vector<std::string>>();
            if (carried.size() > maxCarried)
                error("starts." + who, "at most " + std::to_string(maxCarried) + " abilities");
            for (const std::string &id : carried)
                if (!known(id))
                    error("starts." + who, "\"" + id + "\" is not in abilities.csv");
            skills.starts[who] = std::move(carried);
        }
        // 2b. Gear: its slot, the abilities it brings, its numbers; what each wears at the start.
        static const char *slots[] = {"weapon", "armour", "accessory"};
        const nlohmann::json gear = j.value("gear", nlohmann::json::object());
        for (const auto &[id, g] : gear.items())
        {
            Gear one{static_cast<int>(std::ranges::find(slots, g.value("slot", std::string()), [](const char *x) { return std::string(x); }) - std::begin(slots)),
                     g.value("skill", ""), g.value("passive", ""), g.value("hp", 0), g.value("attack", 0), g.value("defence", 0), g.value("speed", 0)};
            if (one.slot > 2)
                error("gear." + id + ".slot", "expected weapon, armour or accessory");
            for (const std::string &a : {one.skill, one.passive})
                if (!a.empty() && !known(a))
                    error("gear." + id, "\"" + a + "\" is not in abilities.csv");
            if (std::ranges::none_of(def.items, [&](const match::ItemDef &it) { return it.id == id; }))
                error("gear." + id, "\"" + id + "\" is not in items.csv");
            skills.gear[id] = one;
        }
        const nlohmann::json gearStarts = j.value("gearStarts", nlohmann::json::object());
        for (const auto &[who, worn] : gearStarts.items())
            skills.gearStarts[who] = worn.get<std::array<std::string, 3>>();
        // 3. Each character's life: years in order, each linked to a known skill or none.
        const nlohmann::json lives = j.value("timeline", nlohmann::json::object());
        for (const auto &[who, list] : lives.items())
            for (const nlohmann::json &m : list)
            {
                Moment moment{m.value("year", 0), m.value("skill", "")};
                if (!moment.skill.empty() && !known(moment.skill))
                    error("timeline." + who, "\"" + moment.skill + "\" is not in abilities.csv");
                if (!skills.timeline[who].empty() && moment.year < skills.timeline[who].back().year)
                    error("timeline." + who, "expected years in order");
                skills.timeline[who].push_back(moment);
            }
        if (d.size() != before)
            return false;
        out = std::move(skills);
        return true;
    }

    int UnlockLevel(const Skills &skills, const std::string &ability)
    {
        const auto it = skills.unlock.find(ability);
        return it == skills.unlock.end() ? 1 : it->second;
    }

    std::vector<std::string> Loadout(const Skills &skills, const Save &save, const std::string &who, const std::vector<std::string> &known, int level)
    {
        // 1. The save's choice, else the start, else everything known.
        const std::vector<std::string> &wanted = save.skills.contains(who) ? save.skills.at(who) : skills.starts.contains(who) ? skills.starts.at(who) : known;
        // 2. Only known, unlocked, once each, at most maxCarried; a weapon's skill comes with the weapon, not here.
        const auto gearSkill = [&](const std::string &id) { return std::ranges::any_of(skills.gear, [&](const auto &g) { return g.second.skill == id; }); };
        std::vector<std::string> out;
        for (const std::string &id : wanted)
            if (out.size() < maxCarried && std::ranges::find(known, id) != known.end() && UnlockLevel(skills, id) <= level && std::ranges::find(out, id) == out.end() && !gearSkill(id))
                out.push_back(id);
        return out;
    }

    bool Equip(const Skills &skills, Save &save, const std::string &who, const std::vector<std::string> &known, int level, const std::string &ability)
    {
        std::vector<std::string> now = Loadout(skills, save, who, known, level);
        const bool gearSkill = std::ranges::any_of(skills.gear, [&](const auto &g) { return g.second.skill == ability; });
        if (gearSkill || now.size() >= maxCarried || std::ranges::find(now, ability) != now.end() || std::ranges::find(known, ability) == known.end() || UnlockLevel(skills, ability) > level)
            return false;
        now.push_back(ability);
        save.skills[who] = std::move(now);
        return true;
    }

    bool Unequip(const Skills &skills, Save &save, const std::string &who, const std::vector<std::string> &known, int level, const std::string &ability)
    {
        // Something always stays: the last one cannot be put away.
        std::vector<std::string> now = Loadout(skills, save, who, known, level);
        const auto it = std::ranges::find(now, ability);
        if (it == now.end() || now.size() <= 1)
            return false;
        now.erase(it);
        save.skills[who] = std::move(now);
        return true;
    }

    std::array<std::string, 3> GearOf(const Skills &skills, const Save &save, const std::string &who)
    {
        if (const auto it = save.gear.find(who); it != save.gear.end())
            return it->second;
        const auto start = skills.gearStarts.find(who);
        return start == skills.gearStarts.end() ? std::array<std::string, 3>{} : start->second;
    }

    namespace
    {
        // The save's own copy before a change; a starting piece joins the storage then, so it can move.
        std::array<std::string, 3> &Owned(const Skills &skills, Save &save, const std::string &who)
        {
            if (!save.gear.contains(who))
            {
                save.gear[who] = GearOf(skills, save, who);
                for (const std::string &item : save.gear[who])
                    if (!item.empty())
                        ++save.storage[item];
            }
            return save.gear[who];
        }
    }

    bool Wear(const match::MatchDef &def, const Skills &skills, Save &save, const std::string &who, const std::string &item)
    {
        // 1. Gear, and one in storage not worn by anyone (worn gear stays counted in storage).
        const auto g = skills.gear.find(item);
        if (g == skills.gear.end() || std::ranges::none_of(def.items, [&](const match::ItemDef &it) { return it.id == item; }))
            return false;
        std::array<std::string, 3> &mine = Owned(skills, save, who);
        int worn = 0;
        for (const auto &[other, list] : save.gear)
            worn += static_cast<int>(std::ranges::count(list, item));
        if ((save.storage.contains(item) ? save.storage.at(item) : 0) - worn <= 0)
            return false;
        // 2. Into its slot (what was there goes back to storage, which it never left).
        mine[static_cast<std::size_t>(g->second.slot)] = item;
        return true;
    }

    bool TakeOff(const Skills &skills, Save &save, const std::string &who, int slot)
    {
        if (slot < 0 || slot > 2)
            return false;
        std::array<std::string, 3> &mine = Owned(skills, save, who);
        if (mine[static_cast<std::size_t>(slot)].empty())
            return false;
        mine[static_cast<std::size_t>(slot)].clear();
        return true;
    }
}
