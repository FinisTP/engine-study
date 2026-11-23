#include "MatchTestData.hpp"

#include <Match.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

using namespace umbra::match;

namespace
{
    // Loads a broken copy and returns the first diagnostic's field ("" if it loaded).
    std::string FirstError(const std::filesystem::path &dir, std::string *code = nullptr)
    {
        MatchDef def;
        std::vector<umbra::core::Diagnostic> d;
        if (LoadMatch(dir, def, d))
            return "";
        REQUIRE_FALSE(d.empty());
        if (code)
            *code = d[0].code;
        return d[0].field;
    }

    int RowOf(const std::filesystem::path &file, const char *column, const char *value)
    {
        umbra::night::CsvTable t;
        std::string error;
        REQUIRE(umbra::night::ReadCsv(file, t, error));
        for (std::size_t r = 0; r < t.rows.size(); ++r)
            if (t.rows[r][static_cast<std::size_t>(t.Column(column))] == value)
                return static_cast<int>(r);
        return -1;
    }
}

TEST_CASE("The shipped match data loads (T274)", "[match][data]")
{
    const MatchDef def = Data();
    CHECK(def.options.rounds == 30);
    CHECK(def.options.side == 3);
    CHECK_FALSE(def.options.autoEndTurn);
    CHECK(def.options.bots == BotLevel::Medium);
    CHECK(def.characters.size() == 5);
    for (Role r : {Role::Attacker, Role::Defender, Role::Supporter, Role::Speedster, Role::AllRounder})
        CHECK(std::ranges::count(def.characters, r, &CharacterDef::role) == 1);
    const int gamble = MatchDef::Find(def.abilities, "gamble");
    REQUIRE(gamble >= 0);
    CHECK(def.abilities[static_cast<std::size_t>(gamble)].dice.count == 2);
    CHECK(def.abilities[static_cast<std::size_t>(gamble)].dice.sides == 6);
    CHECK(def.rules.board.squareCapacity == 2);
    CHECK(def.rules.board.stride == 2);
    CHECK(def.rules.levels.xp.size() == 4);
}

TEST_CASE("Bad match data names its file and field (T275)", "[match][data]")
{
    const auto dir = DataCopy("umbra-match-t275");
    SetCell(dir / "jobs.csv", 1, "ability", "nosuch");
    CHECK(FirstError(dir) == "jobs.csv:2:ability");

    const auto d2 = DataCopy("umbra-match-t275b");
    SetCell(d2 / "events.csv", 0, "chance1", "120");
    CHECK(FirstError(d2) == "events.csv:1:chance1");

    const auto d3 = DataCopy("umbra-match-t275c");
    SetCell(d3 / "abilities.csv", RowOf(d3 / "abilities.csv", "id", "gamble"), "dice", "2x6");
    CHECK(FirstError(d3).starts_with("abilities.csv:"));

    const auto d4 = DataCopy("umbra-match-t275d");
    SetCell(d4 / "options.csv", RowOf(d4 / "options.csv", "id", "rounds"), "value", "0");
    CHECK(FirstError(d4).starts_with("options.csv:"));

    const auto d5 = DataCopy("umbra-match-t275e");
    SetCell(d5 / "monsters.csv", 0, "behaviour", "dance");
    CHECK(FirstError(d5) == "monsters.csv:1:behaviour");
}

TEST_CASE("Every role starts with a free battle ability (T276)", "[match][data]")
{
    const auto dir = DataCopy("umbra-match-t276");
    // The Defender's free Strike is its first jobs.csv row.
    DropRow(dir / "jobs.csv", RowOf(dir / "jobs.csv", "role", "defender"));
    std::string code;
    CHECK_FALSE(FirstError(dir, &code).empty());
    CHECK(code == "match.jobs");
}

TEST_CASE("The new columns load (T310)", "[match][data]")
{
    const MatchDef def = Data();
    auto ability = [&](const char *id) { return def.abilities[static_cast<std::size_t>(MatchDef::Find(def.abilities, id))]; };
    CHECK(ability("jab").target == Target::Enemy);
    CHECK(ability("brace").target == Target::Self);
    CHECK(ability("tend").target == Target::Ally);
    CHECK(ability("snipe").target == Target::Enemy);

    auto monster = [&](const char *id) { return def.monsters[static_cast<std::size_t>(MatchDef::Find(def.monsters, id))]; };
    CHECK(monster("usherette").behaviour == Behaviour::Hunt);
    CHECK(monster("usherette").level == 2);
    CHECK(monster("usherette").size == 1);
    CHECK(monster("usherette").sight == 6); // tiles: three squares
    CHECK(monster("ghost-chorus").size == 2);
    CHECK(monster("the-spider").size == 3);
    CHECK(monster("the-spider").essence > monster("ghost-chorus").essence);
    CHECK(def.rules.spawn.warn == 1);

    // 1. Spawn rows come sorted by zone, then by first round.
    REQUIRE(def.spawns.size() >= 3);
    CHECK(std::ranges::is_sorted(def.spawns, {}, [](const SpawnRow &s) { return std::pair{s.zone, s.from}; }));
    const auto centre = std::ranges::find(def.spawns, 2, &SpawnRow::zone);
    REQUIRE(centre != def.spawns.end());
    CHECK(def.monsters[static_cast<std::size_t>(centre->monster)].id == "usherette");
    CHECK(centre->from == 1);
    CHECK(centre->every == 0); // the centre spawns once
}

TEST_CASE("Bad spawn or size data is refused (T311)", "[match][data]")
{
    const auto d1 = DataCopy("umbra-match-t311a");
    SetCell(d1 / "monsters.csv", RowOf(d1 / "monsters.csv", "id", "the-spider"), "size", "4");
    CHECK(FirstError(d1) == "monsters.csv:" + std::to_string(RowOf(d1 / "monsters.csv", "id", "the-spider") + 1) + ":size");

    const auto d2 = DataCopy("umbra-match-t311b");
    SetCell(d2 / "spawns.csv", 0, "zone", "attic");
    CHECK(FirstError(d2) == "spawns.csv:1:zone");

    const auto d3 = DataCopy("umbra-match-t311c");
    SetCell(d3 / "spawns.csv", 0, "monster", "usher");
    CHECK(FirstError(d3) == "spawns.csv:1:monster");

    const auto d4 = DataCopy("umbra-match-t311d");
    SetCell(d4 / "spawns.csv", 0, "every", "-1");
    CHECK(FirstError(d4) == "spawns.csv:1:every");
}

TEST_CASE("rules.json modes load, and a broken mode is refused (campaign plan 2)", "[match][data]")
{
    const MatchDef def = Data();
    REQUIRE(def.modes.count("faction") == 1);
    REQUIRE(def.modes.count("campaign") == 1);
    const Mode &f = def.modes.at("faction"), &c = def.modes.at("campaign");
    CHECK((f.essence && f.perform && f.respawn && f.fightXp));
    CHECK_FALSE((f.biding || f.fallsStay || f.playerLoss || f.mustFree));
    CHECK(f.win == WinBy::Score);
    CHECK_FALSE((c.essence || c.perform || c.respawn || c.fightXp));
    CHECK((c.biding && c.fallsStay && c.playerLoss));
    CHECK(c.win == WinBy::Boss);
    CHECK(c.roundLimit == 0);
    CHECK(c.maxAp == 2);
    CHECK(c.apPerTurn == 2);
    // The faction match's round limit is options.csv's rounds when rules.json leaves it out.
    CHECK(NewMatch(def, 3, {0, 1, 2}).mode.roundLimit == def.options.rounds);

    const auto dir = DataCopy("umbra-match-modes");
    std::ifstream in(dir / "rules.json");
    nlohmann::json rules = nlohmann::json::parse(in);
    in.close();
    rules["modes"]["campaign"]["win"] = "most";
    std::ofstream(dir / "rules.json") << rules.dump();
    CHECK(FirstError(dir) == "rules.json:modes.campaign.win");
    rules["modes"]["campaign"]["win"] = "boss";
    rules["modes"]["campaign"]["fly"] = true;
    std::ofstream(dir / "rules.json") << rules.dump();
    CHECK(FirstError(dir) == "rules.json:modes.campaign.fly");
    rules["modes"].erase("campaign");
    std::ofstream(dir / "rules.json") << rules.dump();
    CHECK(FirstError(dir) == "rules.json:modes.campaign");
}
