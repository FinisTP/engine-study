#include "MatchTestData.hpp"

#include <MatchPlay.hpp>

#include <fstream>

using namespace umbra::match;

namespace
{
    nlohmann::json Words()
    {
        std::ifstream in(std::filesystem::path(UMBRA_LASTCURTAIN_ASSETS) / "match-words.json");
        return nlohmann::json::parse(in);
    }

    umbra::night::GameOptions Pace()
    {
        umbra::night::GameOptions o;
        o.turnBannerSeconds = 1.0F, o.beatSeconds = 0.5F, o.plainBeatSeconds = 0.2F;
        return o;
    }

    MatchPlay Make(const MatchDef &def, std::uint64_t seed = 7)
    {
        MatchPlay p(def, seed, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")}, BotLevel::Medium);
        p.SetWords(Words());
        p.SetOptions(Pace());
        return p;
    }
}

TEST_CASE("Every log kind and every Why has words (T322)", "[match][play]")
{
    const nlohmann::json w = Words();
    for (int k = 0; k <= static_cast<int>(LogKind::Over); ++k)
    {
        const std::string name = MatchPlay::LogName(static_cast<LogKind>(k));
        INFO(name);
        CHECK_FALSE(name.empty());
        CHECK(w["log"].value(name, std::string()) != "");
    }
    for (int k = 0; k <= static_cast<int>(Why::Over); ++k)
    {
        const std::string name = MatchPlay::WhyName(static_cast<Why>(k));
        INFO(name);
        CHECK_FALSE(name.empty());
        CHECK(w["why"].value(name, std::string()) != "");
    }
    // 1. A whole match told in words leaves no blank to fill.
    const MatchDef def = Data();
    MatchPlay p = Make(def, 3);
    for (int i = 0; i < 200000 && !p.State().over; ++i)
    {
        if (p.PlayerTurn())
            REQUIRE(p.Do(umbra::match::ChooseAction(def, p.State(), BotLevel::Medium)));
        p.Update(0.25F);
    }
    REQUIRE(p.State().over);
    for (const auto &line : p.UiData()["log"])
        CHECK(line.get<std::string>().find('{') == std::string::npos);
}

TEST_CASE("The order strip runs on past the round's end (T323)", "[match][play]")
{
    const MatchDef def = Data();
    MatchPlay p = Make(def);
    const auto &order = p.State().order;
    nlohmann::json strip = p.UiData()["order"];
    REQUIRE(strip.size() == order.size() + 1);
    CHECK(strip[0]["unit"] == order[0]);
    CHECK(strip[0]["current"] == true);
    CHECK(strip[order.size()]["marker"] == true);
    // 1. Once the first has acted, it comes after the marker.
    for (int i = 0; i < 20000 && p.State().turn == 0; ++i)
    {
        if (p.PlayerTurn())
            REQUIRE(p.Do({ActionKind::EndTurn}));
        p.Update(0.25F);
    }
    REQUIRE(p.State().turn == 1);
    strip = p.UiData()["order"];
    CHECK(strip[0]["unit"] == order[1]);
    CHECK(strip[order.size() - 1]["marker"] == true);
    CHECK(strip[order.size()]["unit"] == order[0]);
    CHECK(strip[order.size()]["acted"] == true);
    CHECK(p.UiData()["unit"].contains("hp"));
}

TEST_CASE("Bots and monsters play one beat at a time and stop at the player's turn; input is refused meanwhile (T324)", "[match][play]")
{
    const MatchDef def = Data();
    MatchPlay p = Make(def);
    // 1. The round opens with its banner; nothing moves and input is refused until it is gone.
    CHECK(p.Screen() == "banner");
    CHECK(p.UiData()["banner"]["title"] == "Turn 1");
    CHECK_FALSE(p.PlayerTurn());
    CHECK_FALSE(p.Do({ActionKind::EndTurn}));
    const std::size_t logBefore = p.State().log.size();
    p.Update(0.9F);
    CHECK(p.State().log.size() == logBefore);
    p.Update(0.2F);
    CHECK(p.Screen() == "");

    // 2. Each beat adds at most one line to the log the player sees.
    std::size_t lines = p.UiData()["log"].size();
    for (int i = 0; i < 400 && !p.PlayerTurn(); ++i)
    {
        CHECK_FALSE(p.Do({ActionKind::EndTurn}));
        p.Update(0.5F);
        const std::size_t now = p.UiData()["log"].size();
        CHECK(now <= lines + 1);
        lines = now;
    }
    REQUIRE(p.PlayerTurn());

    // 3. At the player's turn nothing more happens however long it waits.
    const std::size_t log = p.State().log.size();
    for (int i = 0; i < 20; ++i)
        p.Update(5.0F);
    CHECK(p.State().log.size() == log);
    CHECK(p.PlayerTurn());

    // 4. Skip runs the bots and monsters to the player's next turn at once.
    REQUIRE(p.Do({ActionKind::EndTurn}));
    CHECK(p.HandleUi("", "skip", ""));
    CHECK((p.PlayerTurn() || p.Screen() == "banner" || p.State().over));
}

namespace
{
    // A match at a player's turn, the state set up by `edit` first, the banner played.
    template <class F> MatchPlay AtPlayer(const MatchDef &def, F edit)
    {
        MatchState s = NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
        while (s.units[static_cast<std::size_t>(Current(s))].faction != 0)
            REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
        edit(s);
        MatchPlay p(def, s, BotLevel::Medium);
        p.SetWords(Words());
        p.SetOptions(Pace());
        p.Update(2.0F);
        REQUIRE(p.PlayerTurn());
        return p;
    }

    const nlohmann::json *Find(const nlohmann::json &list, const std::string &id)
    {
        for (const auto &a : list)
            if (a["id"] == id)
                return &a;
        return nullptr;
    }

    int FreeBeside(const MatchState &s, int cell)
    {
        for (int d = 0; d < 4; ++d)
            if (const int n = Neighbour(s.board, cell, d);
                n >= 0 && std::ranges::none_of(s.units, [&](const Unit &u) { return Covers(s.board, u, n); }))
                return n;
        return -1;
    }
}

TEST_CASE("The move range and path match the rules; staying is a click on oneself (T325)", "[match][play]")
{
    const MatchDef def = Data();
    MatchPlay p = AtPlayer(def, [](MatchState &) {});
    const int me = Current(p.State()), from = p.State().units[static_cast<std::size_t>(me)].cell;
    const nlohmann::json reach = p.UiData()["reach"];
    REQUIRE_FALSE(reach.empty());
    CHECK_FALSE(p.UiData().contains("actions"));
    for (const auto &c : reach)
    {
        // 1. Every lit square is within the unit's steps, and the path to it ends there.
        const int cell = c.get<int>();
        CHECK(Steps(p.State().board, from, cell) <= p.State().now.movesLeft);
        MatchPlay q = p;
        q.Hover(cell);
        const nlohmann::json path = q.UiData()["path"];
        REQUIRE_FALSE(path.empty());
        CHECK(path.back() == cell);
        q.Click(cell);
        CHECK(q.State().units[static_cast<std::size_t>(me)].cell == cell);
        CHECK(q.UiData().contains("actions"));
    }
    // 2. A square out of reach does nothing; a click on oneself stays.
    MatchPlay q = p;
    q.Click(p.State().board.hub[1]);
    CHECK(q.State().units[static_cast<std::size_t>(me)].cell == from);
    CHECK_FALSE(q.UiData().contains("actions"));
    q.Click(from);
    CHECK(q.UiData().contains("actions"));
    // 3. Back returns to the move range.
    CHECK(q.HandleUi("", "act", "undo"));
    CHECK_FALSE(q.UiData().contains("actions"));
}

TEST_CASE("Actions offer only what applies, each refusal with its reason (T326)", "[match][play]")
{
    const MatchDef def = Data();
    const nlohmann::json w = Words();
    // 1. No one beside: Attack is off and says why; there is no Perform off the stage.
    MatchPlay p = AtPlayer(def, [](MatchState &) {});
    p.Click(p.State().units[static_cast<std::size_t>(Current(p.State()))].cell);
    nlohmann::json actions = p.UiData()["actions"];
    const nlohmann::json *attack = Find(actions, "attack");
    REQUIRE(attack);
    CHECK((*attack)["enabled"] == false);
    CHECK((*attack)["why"] == w["why"]["NoTarget"]);
    CHECK_FALSE(Find(actions, "perform"));
    CHECK(Find(actions, "end"));
    CHECK((*Find(actions, "undo"))["label"] == w["actions"]["back"]);

    // 2. On the stage with essence: Perform; an enemy beside: Attack arms and lists it; abilities too dear say so.
    int enemy = -1;
    MatchPlay q = AtPlayer(def, [&](MatchState &s) {
        const int me = Current(s);
        s.units[static_cast<std::size_t>(me)].cell = s.now.origin = s.stage;
        s.units[static_cast<std::size_t>(me)].essence = 5;
        s.units[static_cast<std::size_t>(me)].hp = 1;
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 1)
                enemy = i;
        s.units[static_cast<std::size_t>(enemy)].cell = static_cast<std::int16_t>(FreeBeside(s, s.stage));
    });
    q.Click(q.State().stage);
    actions = q.UiData()["actions"];
    REQUIRE(Find(actions, "perform"));
    CHECK((*Find(actions, "perform"))["enabled"] == true);
    CHECK((*Find(actions, "attack"))["enabled"] == true);
    bool dear = false;
    for (const auto &a : actions)
        if (a["id"].get<std::string>().starts_with("ability:"))
            dear = dear || (a["enabled"] == false && a["why"] == w["why"]["TooCostly"]);
    CHECK(dear);
    CHECK(q.HandleUi("", "act", "attack"));
    const nlohmann::json targets = q.UiData()["targets"];
    CHECK(std::ranges::find(targets, nlohmann::json(q.State().units[static_cast<std::size_t>(enemy)].cell)) != targets.end());
    CHECK(q.UiData()["unit"].contains("attack"));
    q.Click(q.State().units[static_cast<std::size_t>(enemy)].cell);
    CHECK(q.State().battle.on);
}

TEST_CASE("The forecast covers targets, the stage, spawns and units (T327)", "[match][play]")
{
    const MatchDef def = Data();
    int enemy = -1, den = -1;
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        const int me = Current(s);
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 1)
                enemy = i;
        s.units[static_cast<std::size_t>(enemy)].cell = static_cast<std::int16_t>(FreeBeside(s, s.units[static_cast<std::size_t>(me)].cell));
        den = s.board.weakDens[0];
        for (Unit &u : s.units)
            if (u.home == den && u.kind == 1)
                u.cell = -1, u.hp = 0;
        s.spawns.push_back({static_cast<std::int16_t>(den), static_cast<std::int8_t>(MatchDef::Find(def.monsters, "moths")), s.round + 1});
    });
    const MatchState &s = p.State();
    p.Click(s.units[static_cast<std::size_t>(Current(s))].cell);
    REQUIRE(p.HandleUi("", "act", "attack"));
    // 1. An armed target: its numbers and the first blow.
    p.Hover(s.units[static_cast<std::size_t>(enemy)].cell);
    nlohmann::json f = p.UiData()["forecast"];
    CHECK(f["kind"] == "target");
    CHECK(f["hp"] == s.units[static_cast<std::size_t>(enemy)].hp);
    CHECK(f["dealtMin"].get<int>() >= 1);
    // 2. The stage, an announced spawn, any unit.
    p.Hover(s.stage);
    CHECK(p.UiData()["forecast"]["kind"] == "stage");
    p.Hover(den);
    f = p.UiData()["forecast"];
    CHECK(f["kind"] == "spawn");
    CHECK(f["level"] == def.monsters[static_cast<std::size_t>(MatchDef::Find(def.monsters, "moths"))].level);
    CHECK(f["round"] == s.round + 1);
    p.Hover(s.board.door[0]);
    CHECK(p.UiData()["forecast"]["kind"] == "unit");
}
