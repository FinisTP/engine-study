#pragma once

#include <Campaign.hpp>

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

// The campaign's cafe (CampaignCafe.md): storage, recipes, memories and boosts, as pure functions on the
// campaign save. Plan 7 writes the save to disk; here it lives in memory.
namespace umbra::campaign
{
    struct Member // one character in the roster
    {
        std::string id;
        int level = 1, xp = 0;
    };

    struct Room // a room on the theatre map
    {
        bool cleared = false;
        int refill = 0;                 // a skirmish's refills so far (its twist)
        std::vector<std::string> taken; // take-once items picked up there
    };

    struct Save
    {
        Speaker player;              // name and form of address
        std::vector<Member> roster;  // who can fight, with levels
        std::map<std::string, Room> rooms;
        int chapter = 0;
        bool riteUnlocked = false, riteUsed = false; // the rite: once in the whole game
        std::map<std::string, int> storage;                      // items.csv id -> count (ingredients, keepsakes, items)
        std::set<std::string> recipes, taken, memories, talks, joined; // known recipes; take-once ids; memories back; talks seen; recruits
        Facts facts;
        std::map<std::string, std::string> versions;             // an upgrade's base ability -> the version chosen (final)
        std::map<std::string, std::map<std::string, int>> boosts; // character -> drink -> how many
        std::map<std::string, std::array<std::string, 4>> slots;  // character -> its 4 battle slots (items.csv ids, "" empty)
        std::vector<std::string> party;                           // who goes to the next fight ("Ra trận")
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
        std::vector<std::string> teaches; // recipe ids learnt
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

    // The save file (CampaignCafe.md "The save"): JSON version 2, written to a temp file then renamed over the
    // old one, so a write that stops halfway leaves the last good save. A bad file is reported, never removed.
    bool SaveCampaign(const std::filesystem::path &file, const Save &save, std::string &error);
    bool LoadCampaign(const std::filesystem::path &file, Save &save, std::string &error);

    // Progress (CafeHub.md "Progress"): 100 XP a level, up to 15.
    constexpr int xpPerLevel = 100, levelCap = 15;
    void AddXp(Member &m, int xp, int capLevel); // no XP while at or past capLevel
    // A won stage: shared XP (a skirmish's stops at the next story stage's level), finds to storage, take-once
    // items, the room cleared, the fight's facts, freed recruits; a story win refills the cleared skirmishes.
    void WinStage(const match::MatchDef &def, Save &save, const StageDef &stage, const BuiltStage &fight, const std::vector<StageDef> &stages);
    // A lost stage: the save from before it, except that the rite stays used.
    Save LoseStage(const Save &before, const BuiltStage &fight);

    // The theatre map (map.json): tickets on the floor plan; a story or skirmish stage opens after another is won.
    struct Ticket
    {
        std::string id, name, kind, stage, opensAfter, lock; // kind story, skirmish or unknown
        float x = 0.0F, y = 0.0F;                             // 0..1 across and down the plan
    };
    struct TheatreMap
    {
        std::string plan;
        std::vector<Ticket> tickets;
    };
    bool LoadMap(const std::filesystem::path &file, const std::vector<StageDef> &stages, TheatreMap &out, std::vector<core::Diagnostic> &d);
    struct TicketView
    {
        std::string id, name, mark, detail; // mark: cleared, next, skirmish, locked, unknown; detail: a twist or a lock line
        float x = 0.0F, y = 0.0F;
        bool open = false;
    };
    std::vector<TicketView> MapView(const TheatreMap &map, const Save &save, const std::vector<StageDef> &stages);
    // The ticket nearest `from` in a direction (u, d, l, r), "" none.
    std::string Nearest(const TheatreMap &map, const std::string &from, char direction);
}
