#pragma once

#include <Campaign.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

// The campaign's cafe (CampaignCafe.md): storage, recipes, memories and boosts, as pure functions on the
// campaign save. Plan 7 writes the save to disk; here it lives in memory.
namespace umbra::campaign
{
    struct Save
    {
        std::map<std::string, int> storage;                      // items.csv id -> count (ingredients, keepsakes, items)
        std::set<std::string> recipes, taken, memories, talks, joined; // known recipes; take-once ids; memories back; talks seen; recruits
        Facts facts;
        std::map<std::string, std::string> versions;             // an upgrade's base ability -> the version chosen (final)
        std::map<std::string, std::map<std::string, int>> boosts; // character -> drink -> how many
    };

    struct Recipe
    {
        std::string id, name, kind, forWho, stat, text; // kind memory or general; forWho a recruit (memory drinks)
        std::map<std::string, int> needs;               // items.csv id -> count
        int amount = 0;                                 // a general drink: how much of `stat`
    };

    struct Recipes
    {
        std::vector<Recipe> list;
        [[nodiscard]] const Recipe *Find(const std::string &id) const;
    };

    bool LoadRecipes(const std::filesystem::path &file, const match::MatchDef &def, Recipes &out, std::vector<core::Diagnostic> &d);

    struct Missing
    {
        std::string item;
        int need = 0, have = 0;
        std::string where; // the stage titles that hold or drop it, "?" when none does
    };
    // What a recipe still lacks (by item id), with where each comes from.
    std::vector<Missing> Lacks(const Save &save, const Recipe &recipe, const std::vector<StageDef> &stages);
    bool Brew(Save &save, const Recipe &recipe); // spends the needs; false (and nothing spent) when lacking
    // A battle slot takes only items the fight or the board can use (never ingredients or keepsakes).
    bool SlotOk(const match::MatchDef &def, const std::string &item);
}
