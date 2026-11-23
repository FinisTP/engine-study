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

    struct Memory
    {
        std::string recruit, id, name, source, from, scene, upgrade, fact; // source talk, recipe or keepsake; from: its recipe or item id
        std::map<std::string, std::string> versions;           // the answer's value (fact) -> the version's ability id
        bool joins = false, locked = false;
    };

    struct Memories
    {
        std::map<std::string, std::vector<Memory>> byRecruit; // in order
    };

    bool LoadMemories(const std::filesystem::path &file, const std::filesystem::path &scenes, const Recipes &recipes, const match::MatchDef &def,
                      Memories &out, std::vector<core::Diagnostic> &d);
    // The recruit's first memory not yet back that `source`/`from` brings (a talk needs no from); null if none.
    const Memory *NextMemory(const Memories &m, const Save &save, const std::string &recruit, const std::string &source, const std::string &from);
    // After its scene: the memory is back, a joining memory adds the recruit, the answer (facts[fact]) picks
    // the upgrade's version once and for good. False when locked or already back.
    bool Remember(Save &save, const Memory &memory, const Facts &facts);
    // The ability a kit's base ability has become (its chosen version), else the base.
    std::string AbilityFor(const Save &save, const std::string &base);

    // A general drink on one character: spends its needs, refused (nothing spent) at `cap` drinks for that
    // stat; Boosted is what the drinks add to a stat (maxHp, attack...).
    bool Boost(Save &save, const Recipes &recipes, const Recipe &drink, const std::string &character, int cap);
    int Boosted(const Save &save, const Recipes &recipes, const std::string &character, const std::string &stat);

    // A talk in the cafe (talks/<id>.json): who, its scene, when it opens (a won stage, facts all/none),
    // facts it sets, a memory it brings back, and whether it repeats.
    struct Talk
    {
        std::string id, who, scene, after, memory;
        std::vector<std::string> all, none; // facts "key" (any value) or "key=value"
        Facts sets;
        bool repeat = false;
    };

    struct Talks
    {
        std::vector<Talk> list; // by file name
    };

    bool LoadTalks(const std::filesystem::path &folder, const std::filesystem::path &scenes, const Cast &cast, Talks &out, std::vector<core::Diagnostic> &d);
    std::vector<const Talk *> OpenTalks(const Talks &talks, const Save &save, int limit); // new talks first, at most `limit`
    void Talked(Save &save, const Talk &talk);
    // What a fight leaves in the save's facts: won:<stage>, fell:<cast id>, ratStole:<item> (who: each unit's cast id).
    void RecordFight(const match::MatchDef &def, Save &save, const std::string &stage, const match::MatchState &s, const std::vector<std::string> &who);
}
