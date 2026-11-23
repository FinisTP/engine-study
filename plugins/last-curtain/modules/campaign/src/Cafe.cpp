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
}
