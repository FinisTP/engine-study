#include <CampaignCafe.hpp>

#include <algorithm>
#include <fstream>

namespace umbra::campaign
{
    const Recipe *Recipes::Find(const std::string &id) const
    {
        const auto it = std::ranges::find(list, id, &Recipe::id);
        return it == list.end() ? nullptr : &*it;
    }

    bool LoadRecipes(const std::filesystem::path &file, const match::MatchDef &def, Recipes &out, std::vector<core::Diagnostic> &d)
    {
        const std::size_t before = d.size();
        const auto error = [&](const std::string &field, const std::string &message) { d.push_back({core::Severity::Error, "campaign.recipes", file.string(), field, message}); };
        std::ifstream in(file);
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (!j.is_object() || j.value("format", "") != "umbra.lc-recipes" || j.value("version", 0) != 1)
            return error("format", "expected format umbra.lc-recipes, version 1"), false;
        const nlohmann::json list = j.value("recipes", nlohmann::json::array());
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            const nlohmann::json &r = list[i];
            const std::string at = "recipes[" + std::to_string(i) + "]";
            Recipe recipe{r.value("id", ""), r.value("name", ""), r.value("kind", ""), r.value("for", ""), r.value("stat", ""), r.value("text", ""), {}};
            recipe.amount = r.value("amount", 0);
            if (recipe.kind != "memory" && recipe.kind != "general")
                error(at + ".kind", "expected memory or general");
            const nlohmann::json needs = r.value("needs", nlohmann::json::object());
            for (const auto &[item, count] : needs.items())
                if (std::ranges::none_of(def.items, [&](const match::ItemDef &it) { return it.id == item; }))
                    error(at + ".needs." + item, "\"" + item + "\" is not in items.csv");
                else
                    recipe.needs[item] = count.get<int>();
            out.list.push_back(recipe);
        }
        return d.size() == before;
    }

    std::vector<Missing> Lacks(const Save &save, const Recipe &recipe, const std::vector<StageDef> &stages)
    {
        std::vector<Missing> out;
        for (const auto &[item, need] : recipe.needs)
        {
            const int have = save.storage.contains(item) ? save.storage.at(item) : 0;
            if (have >= need)
                continue;
            // 1. Where: stages with it on a square, as a unit's drop, or from a window.
            std::string where;
            for (const StageDef &s : stages)
            {
                bool here = std::ranges::any_of(s.items, [&](const StageItem &i) { return i.item == item; });
                for (const StageUnit &u : s.units)
                    here |= std::ranges::any_of(u.drops, [&](const auto &drop) { return drop.first == item; });
                if (s.gimmicks.contains("windows"))
                    for (const auto &drop : s.gimmicks["windows"].value("drops", nlohmann::json::array()))
                        here |= drop.value("item", "") == item;
                if (here && where.find(s.title) == std::string::npos)
                    where += (where.empty() ? "" : ", ") + s.title;
            }
            out.push_back({item, need, have, where.empty() ? "?" : where});
        }
        return out;
    }

    bool Brew(Save &save, const Recipe &recipe)
    {
        for (const auto &[item, need] : recipe.needs)
            if ((save.storage.contains(item) ? save.storage.at(item) : 0) < need)
                return false;
        for (const auto &[item, need] : recipe.needs)
            save.storage[item] -= need;
        return true;
    }

    bool SlotOk(const match::MatchDef &def, const std::string &item)
    {
        const auto it = std::ranges::find(def.items, item, &match::ItemDef::id);
        return it != def.items.end() && it->family == match::ItemFamily::Item && (it->use & (match::useBoard | match::useFight)) != 0;
    }

    bool LoadMemories(const std::filesystem::path &file, const std::filesystem::path &scenes, const Recipes &recipes, const match::MatchDef &def,
                      Memories &out, std::vector<core::Diagnostic> &d)
    {
        const std::size_t before = d.size();
        const auto error = [&](const std::string &field, const std::string &message) { d.push_back({core::Severity::Error, "campaign.memories", file.string(), field, message}); };
        std::ifstream in(file);
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (!j.is_object() || j.value("format", "") != "umbra.lc-memories" || j.value("version", 0) != 1)
            return error("format", "expected format umbra.lc-memories, version 1"), false;
        const nlohmann::json recruits = j.value("recruits", nlohmann::json::object());
        for (const auto &[recruit, list] : recruits.items())
            for (std::size_t i = 0; i < list.size(); ++i)
            {
                const nlohmann::json &m = list[i];
                const std::string at = "recruits." + recruit + "[" + std::to_string(i) + "]";
                Memory memory{recruit, m.value("id", ""), m.value("name", ""), m.value("source", ""), m.value("from", ""), m.value("scene", ""),
                              m.value("upgrade", ""), m.value("fact", ""), {}, m.value("joins", false), m.value("locked", false)};
                const auto ability = [&](const std::string &id) { return std::ranges::any_of(def.abilities, [&](const match::AbilityDef &a) { return a.id == id; }); };
                if (memory.source != "talk" && memory.source != "recipe" && memory.source != "keepsake")
                    error(at + ".source", "expected talk, recipe or keepsake");
                if (!std::filesystem::exists(scenes / (memory.scene + ".json")))
                    error(at + ".scene", "no scene file " + memory.scene + ".json");
                if (memory.source == "recipe" && !memory.locked && !recipes.Find(memory.from))
                    error(at + ".from", "\"" + memory.from + "\" is not in recipes.json");
                if (memory.source == "keepsake" && std::ranges::none_of(def.items, [&](const match::ItemDef &it) { return it.id == memory.from; }))
                    error(at + ".from", "\"" + memory.from + "\" is not in items.csv");
                if (!memory.upgrade.empty() && !ability(memory.upgrade))
                    error(at + ".upgrade", "\"" + memory.upgrade + "\" is not in abilities.csv");
                const nlohmann::json versions = m.value("versions", nlohmann::json::object());
                for (const auto &[answer, version] : versions.items())
                    if (!ability(version.get<std::string>()))
                        error(at + ".versions." + answer, "\"" + version.get<std::string>() + "\" is not in abilities.csv");
                    else
                        memory.versions[answer] = version.get<std::string>();
                out.byRecruit[recruit].push_back(memory);
            }
        return d.size() == before;
    }

    const Memory *NextMemory(const Memories &m, const Save &save, const std::string &recruit, const std::string &source, const std::string &from)
    {
        if (!m.byRecruit.contains(recruit))
            return nullptr;
        for (const Memory &memory : m.byRecruit.at(recruit))
            if (!memory.locked && !save.memories.contains(memory.id) && memory.source == source && (source == "talk" || memory.from == from))
                return &memory;
        return nullptr;
    }

    bool Remember(Save &save, const Memory &memory, const Facts &facts)
    {
        if (memory.locked || save.memories.contains(memory.id))
            return false;
        save.memories.insert(memory.id);
        if (memory.joins)
            save.joined.insert(memory.recruit);
        // The answer picks the version; a version once chosen stays (P8).
        if (const auto answer = facts.find(memory.fact); answer != facts.end() && memory.versions.contains(answer->second) && !save.versions.contains(memory.upgrade))
            save.versions[memory.upgrade] = memory.versions.at(answer->second);
        return true;
    }

    std::string AbilityFor(const Save &save, const std::string &base)
    {
        return save.versions.contains(base) ? save.versions.at(base) : base;
    }

    bool Boost(Save &save, const Recipes &recipes, const Recipe &drink, const std::string &character, int cap)
    {
        if (drink.kind != "general")
            return false;
        int drunk = 0; // drinks of this stat already
        if (save.boosts.contains(character))
            for (const auto &[id, count] : save.boosts.at(character))
                if (const Recipe *r = recipes.Find(id); r && r->stat == drink.stat)
                    drunk += count;
        if (drunk >= cap || !Brew(save, drink))
            return false;
        ++save.boosts[character][drink.id];
        return true;
    }

    int Boosted(const Save &save, const Recipes &recipes, const std::string &character, const std::string &stat)
    {
        int total = 0;
        if (save.boosts.contains(character))
            for (const auto &[id, count] : save.boosts.at(character))
                if (const Recipe *r = recipes.Find(id); r && r->stat == stat)
                    total += count * r->amount;
        return total;
    }

    bool LoadTalks(const std::filesystem::path &folder, const std::filesystem::path &scenes, const Cast &cast, Talks &out, std::vector<core::Diagnostic> &d)
    {
        const std::size_t before = d.size();
        std::vector<std::filesystem::path> files;
        for (const auto &f : std::filesystem::directory_iterator(folder))
            if (f.path().extension() == ".json")
                files.push_back(f.path());
        std::ranges::sort(files);
        for (const auto &file : files)
        {
            const auto error = [&](const std::string &field, const std::string &message) { d.push_back({core::Severity::Error, "campaign.talk", file.string(), field, message}); };
            std::ifstream in(file);
            const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            if (!j.is_object() || j.value("format", "") != "umbra.lc-talk" || j.value("version", 0) != 1)
            {
                error("format", "expected format umbra.lc-talk, version 1");
                continue;
            }
            Talk t{j.value("id", ""), j.value("who", ""), j.value("scene", ""), j.value("after", ""), j.value("memory", ""), {}, {}, {}, j.value("repeat", false)};
            const nlohmann::json opens = j.value("opens", nlohmann::json::object());
            t.all = opens.value("all", std::vector<std::string>{}), t.none = opens.value("none", std::vector<std::string>{});
            t.sets = j.value("sets", Facts{});
            if (!cast.Find(t.who))
                error("who", "\"" + t.who + "\" is not in cast.csv");
            if (!std::filesystem::exists(scenes / (t.scene + ".json")))
                error("scene", "no scene file " + t.scene + ".json");
            out.list.push_back(t);
        }
        return d.size() == before;
    }

    namespace
    {
        // "key" holds when set; "key=value" when it has that value.
        bool Holds(const Facts &facts, const std::string &fact)
        {
            const std::size_t eq = fact.find('=');
            const auto it = facts.find(fact.substr(0, eq));
            return it != facts.end() && (eq == std::string::npos || it->second == fact.substr(eq + 1));
        }
    }

    std::vector<const Talk *> OpenTalks(const Talks &talks, const Save &save, int limit)
    {
        std::vector<const Talk *> out;
        for (const Talk &t : talks.list)
        {
            const bool open = (t.after.empty() || Holds(save.facts, "won:" + t.after)) && std::ranges::all_of(t.all, [&](const std::string &f) { return Holds(save.facts, f); }) &&
                              std::ranges::none_of(t.none, [&](const std::string &f) { return Holds(save.facts, f); }) && (t.repeat || !save.talks.contains(t.id));
            if (open && static_cast<int>(out.size()) < limit)
                out.push_back(&t);
        }
        return out;
    }

    void Talked(Save &save, const Talk &talk)
    {
        save.talks.insert(talk.id);
        for (const auto &[key, value] : talk.sets)
            save.facts[key] = value;
    }

    void RecordFight(const match::MatchDef &def, Save &save, const std::string &stage, const match::MatchState &s, const std::vector<std::string> &who)
    {
        const auto name = [&](int unit) { return unit >= 0 && static_cast<std::size_t>(unit) < who.size() ? who[static_cast<std::size_t>(unit)] : std::string(); };
        if (s.over && s.winner == 0)
            save.facts["won:" + stage] = "1";
        for (const match::LogEntry &e : s.log)
            if (e.kind == match::LogKind::Down && e.a >= 0 && s.units[static_cast<std::size_t>(e.a)].faction == 0)
                save.facts["fell:" + name(e.a)] = "1";
        for (const match::Stolen &st : s.stash)
            save.facts["ratStole:" + def.items[static_cast<std::size_t>(st.item)].id] = "1";
    }
}
