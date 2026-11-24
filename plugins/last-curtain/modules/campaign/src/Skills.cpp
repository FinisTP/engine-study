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
        // 2. Only known, unlocked, once each, at most maxCarried.
        std::vector<std::string> out;
        for (const std::string &id : wanted)
            if (out.size() < maxCarried && std::ranges::find(known, id) != known.end() && UnlockLevel(skills, id) <= level && std::ranges::find(out, id) == out.end())
                out.push_back(id);
        return out;
    }

    bool Equip(const Skills &skills, Save &save, const std::string &who, const std::vector<std::string> &known, int level, const std::string &ability)
    {
        std::vector<std::string> now = Loadout(skills, save, who, known, level);
        if (now.size() >= maxCarried || std::ranges::find(now, ability) != now.end() || std::ranges::find(known, ability) == known.end() || UnlockLevel(skills, ability) > level)
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
}
