#include "MatchTestData.hpp"

#include <CareerMatch.hpp>
#include <MatchPlay.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

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
    const nlohmann::json log = p.UiData()["log"];
    for (const auto &line : log)
        CHECK(line.get<std::string>().find('{') == std::string::npos);
}

TEST_CASE("The order strip runs on past the round's end, twelve portraits long (T323)", "[match][play]")
{
    const MatchDef def = Data();
    MatchPlay p = Make(def);
    const auto marker = [](const nlohmann::json &strip) {
        for (std::size_t i = 0; i < strip.size(); ++i)
            if (strip[i].value("marker", false))
                return static_cast<int>(i);
        return -1;
    };
    nlohmann::json strip = p.UiData()["order"];
    REQUIRE(strip.size() == 12 + 1); // twelve portraits and the marker (options orderLength)
    CHECK_FALSE(strip[0]["tip"].get<std::string>().empty());
    CHECK(strip[0]["unit"] == p.State().order[static_cast<std::size_t>(p.State().turn)]);
    CHECK(strip[0]["current"] == true);
    const int m = marker(strip);
    REQUIRE(m >= 1);
    for (std::size_t i = static_cast<std::size_t>(m) + 1; i < strip.size(); ++i)
        CHECK(strip[i]["acted"] == true); // after the marker: the next round, in the same order
    // 1. Once the first has acted, it comes after the marker.
    const int first = strip[0]["unit"];
    for (int i = 0; i < 20000 && p.State().order[static_cast<std::size_t>(p.State().turn)] == first; ++i)
    {
        if (p.PlayerTurn())
            REQUIRE(p.Do({ActionKind::EndTurn}));
        p.Update(0.25F);
    }
    strip = p.UiData()["order"];
    CHECK(strip[0]["unit"] != first);
    // It is not before the marker any more; after it, if the next round fits in the twelve.
    const int m2 = marker(strip);
    bool before = false, after = false;
    for (int i = 0; i < static_cast<int>(strip.size()); ++i)
        (i < m2 ? before : after) |= strip[static_cast<std::size_t>(i)].value("unit", -1) == first;
    CHECK_FALSE(before);
    std::size_t up = 0;
    for (const Unit &u : p.State().units)
        up += Up(u) ? 1 : 0;
    if (static_cast<std::size_t>(m2) + up < strip.size())
        CHECK(after);
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
    REQUIRE(p.MenuKey(1)); // Di chuyen: the squares light
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

TEST_CASE("The battle screen follows the rules' battle, a hunter's included (T328)", "[match][play]")
{
    const MatchDef def = Data();
    int enemy = -1;
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        const int me = Current(s);
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 1)
                enemy = i;
        s.units[static_cast<std::size_t>(enemy)].cell = static_cast<std::int16_t>(FreeBeside(s, s.units[static_cast<std::size_t>(me)].cell));
        s.units[static_cast<std::size_t>(me)].hp = s.units[static_cast<std::size_t>(enemy)].hp = 90; // no one falls
        s.units[static_cast<std::size_t>(me)].maxHp = s.units[static_cast<std::size_t>(enemy)].maxHp = 90;
    });
    const int me = Current(p.State());
    p.Click(p.State().units[static_cast<std::size_t>(me)].cell);
    REQUIRE(p.HandleUi("", "act", "attack"));
    p.Click(p.State().units[static_cast<std::size_t>(enemy)].cell);
    for (int i = 0; i < 40 && !p.PlayerTurn(); ++i) // the attack is told in beats (T357)
        p.Update(0.25F);
    // 1. The player picks first: both fighters, the round, the picks with damage.
    REQUIRE(p.Screen() == "battle");
    nlohmann::json b = p.UiData()["battle"];
    CHECK(b["fighters"][0]["unit"] == me);
    CHECK(b["fighters"][1]["unit"] == enemy);
    CHECK(b["round"] == 1);
    REQUIRE_FALSE(b["picks"].empty());
    CHECK(b["picks"][0]["dealtMin"].get<int>() >= 1);
    // 1b. Each pick reads as one number (or a range when the roll varies), its cost only when it costs (note 9),
    //     and what it does as its tip (note 14).
    for (const auto &k : b["picks"])
    {
        const int lo = k["dealtMin"], hi = k["dealtMax"], cost = k["cost"];
        CHECK(k["dealtText"] == (lo == hi ? std::to_string(hi) : std::to_string(lo) + "\u2013" + std::to_string(hi)));
        CHECK(k["costText"] == (cost > 0 ? "-" + std::to_string(cost) + " HP" : ""));
        CHECK_FALSE(k["label"].get<std::string>().find("cost") != std::string::npos);
        if (!k["off"].get<bool>())
            CHECK_FALSE(k["tip"].get<std::string>().empty());
    }
    REQUIRE(p.HandleUi("battle", "pick", "0"));
    // 2. The bot answers on a beat; the blow's line shows.
    CHECK_FALSE(p.PlayerTurn());
    for (int i = 0; i < 20 && !p.PlayerTurn(); ++i)
        p.Update(0.5F);
    REQUIRE(p.PlayerTurn());
    b = p.UiData()["battle"];
    CHECK(b["round"] == 2);
    CHECK_FALSE(b["line"].get<std::string>().empty());
    while (p.Screen() == "battle" && p.State().battle.on && p.PlayerTurn()) // the screen outlives the battle while it is staged (T350)
    {
        REQUIRE(p.HandleUi("battle", "pick", "0"));
        for (int i = 0; i < 20 && !p.PlayerTurn() && p.State().battle.on; ++i)
            p.Update(0.5F);
    }
    CHECK_FALSE(p.State().battle.on);

    // 3. A hunter attacking the player's character: the hunter is the first fighter, the player picks.
    MatchPlay h = AtPlayer(def, [&](MatchState &s) {
        const int mine = Current(s);
        int hunter = -1;
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[static_cast<std::size_t>(i)].kind == 1 && def.monsters[static_cast<std::size_t>(s.units[static_cast<std::size_t>(i)].def)].behaviour == Behaviour::Hunt)
                hunter = i;
        s.battle = Battle{};
        s.battle.on = true, s.battle.attacker = static_cast<std::int8_t>(hunter), s.battle.defender = static_cast<std::int8_t>(mine), s.battle.round = 1;
        s.battle.fighters = {static_cast<std::int8_t>(hunter), -1, static_cast<std::int8_t>(mine), -1};
        s.battle.order = {2, 0, -1, -1}; // the hunter has struck: the player's pick
    });
    REQUIRE(h.Screen() == "battle");
    CHECK(def.monsters[static_cast<std::size_t>(h.State().units[static_cast<std::size_t>(h.UiData()["battle"]["fighters"][0]["unit"].get<int>())].def)].behaviour == Behaviour::Hunt);
    CHECK_FALSE(h.UiData()["battle"]["picks"].empty());
}

TEST_CASE("Level choices and events are their own screens (T329)", "[match][play]")
{
    const MatchDef def = Data();
    // 1. A level choice: the two abilities; picking one learns it.
    const int cleave = MatchDef::Find(def.abilities, "cleave");
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        s.units[static_cast<std::size_t>(Current(s))].pending = 0;
        s.offer = {{1, 1, 1}, {0, static_cast<std::int8_t>(cleave), 0}, {1, 0, 4}};
    });
    REQUIRE(p.Screen() == "level");
    const nlohmann::json level = p.UiData()["level"];
    REQUIRE(level["choices"].size() == 3);
    CHECK(level["choices"][1]["name"].get<std::string>().find(def.abilities[static_cast<std::size_t>(cleave)].name) != std::string::npos);
    CHECK(level["choices"][0]["kind"] == "stat");
    REQUIRE(p.HandleUi("level", "pick", "1"));
    CHECK(p.Screen() == "");
    const auto &abilities = p.State().units[static_cast<std::size_t>(Current(p.State()))].abilities;
    CHECK(std::ranges::find(abilities, cleave) != abilities.end());

    // 2. An event: its card, a choice, the result, then back to the board.
    MatchPlay q = AtPlayer(def, [&](MatchState &s) {
        for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
            if (s.board.cells[static_cast<std::size_t>(c)].event >= 0 && std::ranges::none_of(s.units, [&](const Unit &u) { return u.cell == c; }))
            {
                s.units[static_cast<std::size_t>(Current(s))].cell = s.now.origin = static_cast<std::int16_t>(c);
                break;
            }
    });
    q.Click(q.State().units[static_cast<std::size_t>(Current(q.State()))].cell);
    REQUIRE(q.HandleUi("", "act", "answer"));
    REQUIRE(q.Screen() == "event");
    nlohmann::json e = q.UiData()["event"];
    CHECK_FALSE(e["name"].get<std::string>().empty());
    REQUIRE(e["choices"].size() == 2);
    CHECK(e["choices"][0].contains("chance"));
    REQUIRE(q.HandleUi("event", "choose", "0"));
    e = q.UiData()["event"];
    CHECK_FALSE(e["result"].get<std::string>().empty());
    REQUIRE(q.HandleUi("event", "close", ""));
    CHECK(q.Screen() == "");
}

TEST_CASE("The end screen's numbers come from the state (T330)", "[match][play]")
{
    const MatchDef def = Data();
    MatchPlay p = Make(def, 5);
    for (int i = 0; i < 2000 && !p.State().over; ++i)
    {
        if (p.PlayerTurn())
            REQUIRE(p.Do(ChooseAction(def, p.State(), BotLevel::Medium)));
        p.HandleUi("", "skip", "");
        p.Update(5.0F);
    }
    REQUIRE(p.State().over);
    REQUIRE(p.Screen() == "end");
    const nlohmann::json end = p.UiData()["end"];
    const MatchState &s = p.State();
    CHECK(end["winner"] == s.winner);
    CHECK(end["score"] == nlohmann::json(s.score));
    int kills[2] = {}, performed[2] = {};
    for (const Unit &u : s.units)
        if (u.kind == 0)
            kills[u.faction] += u.kills;
    for (const LogEntry &e : s.log)
        if (e.kind == LogKind::Performed)
            performed[s.units[static_cast<std::size_t>(e.a)].faction] += e.b;
    CHECK(end["kills"][0] == kills[0]);
    CHECK(end["kills"][1] == kills[1]);
    CHECK(end["performed"][0] == performed[0]);
    CHECK(end["performed"][1] == performed[1]);
    CHECK(end.contains("monsters"));
}

TEST_CASE("The pick screen starts only with a full side (T331)", "[match][play]")
{
    const MatchDef def = Data();
    std::vector<int> picks;
    nlohmann::json pick = MatchPlay::PickData(def, picks, false, BotLevel::Medium);
    REQUIRE(pick["characters"].size() == def.characters.size());
    CHECK(pick["canStart"] == false);
    CHECK(pick["canContinue"] == false);
    // 1. Picking toggles; a full side refuses one more; Start lights only when full.
    CHECK(MatchPlay::TogglePick(def, picks, 0));
    CHECK(MatchPlay::TogglePick(def, picks, 1));
    CHECK(MatchPlay::PickData(def, picks, true, BotLevel::Medium)["canStart"] == false);
    CHECK(MatchPlay::TogglePick(def, picks, 2));
    pick = MatchPlay::PickData(def, picks, true, BotLevel::Hard);
    CHECK(pick["canStart"] == true);
    CHECK(pick["canContinue"] == true);
    CHECK(pick["characters"][2]["picked"] == true);
    CHECK(pick["level"] == "hard");
    CHECK_FALSE(MatchPlay::TogglePick(def, picks, 3));
    CHECK(picks.size() == static_cast<std::size_t>(def.options.side));
    CHECK(MatchPlay::TogglePick(def, picks, 1)); // un-picks
    CHECK(MatchPlay::PickData(def, picks, false, BotLevel::Easy)["canStart"] == false);
    CHECK_FALSE(MatchPlay::TogglePick(def, picks, 99));
}

// Not a check: writes assets/ui/match-sample.json, the data `UmbraTool ui-preview --data` shows (MatchPlay.md).
TEST_CASE("Write the match screens' sample data", "[.sample]")
{
    const MatchDef def = Data();
    nlohmann::json sample;
    std::vector<int> picks{0, 1};
    sample["pick"] = MatchPlay::PickData(def, picks, true, BotLevel::Medium);
    // 1. A turn: armed, a target under the mouse, the log with a few lines.
    int enemy = -1;
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        const int me = Current(s);
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 1)
                enemy = i;
        s.units[static_cast<std::size_t>(enemy)].cell = static_cast<std::int16_t>(FreeBeside(s, s.units[static_cast<std::size_t>(me)].cell));
        s.units[static_cast<std::size_t>(me)].essence = 6;
    });
    p.Click(p.State().units[static_cast<std::size_t>(Current(p.State()))].cell);
    p.HandleUi("", "act", "attack");
    p.Hover(p.State().units[static_cast<std::size_t>(enemy)].cell);
    const nlohmann::json turn = p.UiData();
    for (const auto &[k, v] : turn.items())
        sample[k] = v;
    sample["log"] = {"Hoang Gia Huy walks.", "Tran Ngoc Lan attacks the Moth swarm.", "Tran Ngoc Lan hits the Moth swarm for 5."};
    sample["tip"] = "No one in reach to aim at.";
    // 2. The battle, the level choice, the event and the end, from their own moments.
    p.Click(p.State().units[static_cast<std::size_t>(enemy)].cell);
    sample["battle"] = p.UiData()["battle"];
    {
        MatchPlay lv = AtPlayer(def, [&](MatchState &st) {
            st.units[static_cast<std::size_t>(Current(st))].pending = 0;
            st.offer = {{0, static_cast<std::int8_t>(MatchDef::Find(def.abilities, "cleave")), 0}, {1, 1, 1}, {2, 0, 0}};
        });
        sample["level"] = lv.UiData()["level"];
    }
    const EventDef &e = def.events[0];
    sample["event"] = {{"name", e.name}, {"text", e.text}, {"choosing", true}, {"result", ""},
                       {"choices", {{{"label", e.choices[0].label}, {"chance", e.choices[0].chance}}, {{"label", e.choices[1].label}, {"chance", e.choices[1].chance}}}}};
    MatchPlay q = Make(def, 5);
    for (int i = 0; i < 400 && !q.State().over; ++i)
    {
        if (q.PlayerTurn())
            REQUIRE(q.Do(ChooseAction(def, q.State(), BotLevel::Medium)));
        q.HandleUi("", "skip", "");
        q.Update(5.0F);
    }
    sample["end"] = q.UiData()["end"];
    sample["banner"] = {{"title", "Turn 4"}, {"text", "27 turns left"}};
    std::ofstream(std::filesystem::path(UMBRA_LASTCURTAIN_ASSETS) / "ui/match-sample.json") << sample.dump(1) << "\n";
}

TEST_CASE("Each told line brings its motions and its sound (T342)", "[match][play]")
{
    using P = std::vector<std::pair<int, std::string>>;
    CHECK(MatchPlay::Moves({LogKind::Blow, 2, 7, 5}) == P{{2, "attack"}, {7, "hit"}});
    CHECK(MatchPlay::Moves({LogKind::Down, 7, 2, 0}) == P{{7, "down"}});
    CHECK(MatchPlay::Moves({LogKind::Back, 7, 30, 0}) == P{{7, "revive"}});
    CHECK(MatchPlay::Moves({LogKind::Level, 2, 3, 0}) == P{{2, "levelup"}});
    CHECK(MatchPlay::Sound({LogKind::Blow, 2, 7, 5}) == "hit");
    CHECK(MatchPlay::Sound({LogKind::Moved, 2, 30, 0}) == "step");
    CHECK(MatchPlay::Sound({LogKind::Over, 0, -1, 0}) == "bell");
    // 1. Playing on beats queues them for the window to take; the banner opens with its sound.
    const MatchDef def = Data();
    MatchPlay p = Make(def);
    p.Update(0.1F);
    std::vector<std::string> sounds = p.TakeSounds();
    CHECK(std::ranges::find(sounds, "turn") != sounds.end());
    for (int i = 0; i < 200 && p.TakeMoves().empty(); ++i)
    {
        if (p.PlayerTurn())
            REQUIRE(p.Do({ActionKind::EndTurn}));
        p.Update(0.5F);
    }
    CHECK(p.TakeMoves().empty()); // taken once
}

TEST_CASE("A blow or a fall puts a short line in a bubble by its speaker; the same character on both sides is told apart (T343)", "[match][play]")
{
    const MatchDef def = Data();
    int enemy = -1;
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        const int me = Current(s);
        // The enemy is the same character as mine: its name is tagged.
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 1)
                enemy = i;
        s.units[static_cast<std::size_t>(enemy)].def = s.units[static_cast<std::size_t>(me)].def;
        s.units[static_cast<std::size_t>(enemy)].cell = static_cast<std::int16_t>(FreeBeside(s, s.units[static_cast<std::size_t>(me)].cell));
        s.units[static_cast<std::size_t>(me)].hp = s.units[static_cast<std::size_t>(enemy)].hp = 90;
    });
    const int me = Current(p.State());
    p.Click(p.State().units[static_cast<std::size_t>(me)].cell);
    REQUIRE(p.HandleUi("", "act", "attack"));
    p.Click(p.State().units[static_cast<std::size_t>(enemy)].cell);
    for (int i = 0; i < 40 && !p.PlayerTurn(); ++i) // the attack is told in beats (T357)
        p.Update(0.25F);
    REQUIRE(p.HandleUi("battle", "pick", "0"));
    // 1. Once the blow is told, its speaker has a bubble with one of the written lines.
    for (int i = 0; i < 10 && p.UiData()["bubbles"].empty(); ++i)
        p.Update(0.1F);
    const nlohmann::json bubbles = p.UiData()["bubbles"];
    REQUIRE_FALSE(bubbles.empty());
    CHECK(bubbles[0]["unit"] == me);
    const nlohmann::json lines = Words()["lines"]["Blow"];
    CHECK(std::ranges::find(lines, bubbles[0]["text"]) != lines.end());
    // 2. The log tells the two apart.
    const std::string theirs = Words()["misc"]["theirs"].get<std::string>();
    bool tagged = false;
    const nlohmann::json log = p.UiData()["log"];
    for (const auto &line : log)
        tagged = tagged || line.get<std::string>().find(theirs.substr(theirs.find('('))) != std::string::npos;
    CHECK(tagged);
    // 3. Bubbles fade after a while.
    for (int i = 0; i < 20 && !p.UiData()["bubbles"].empty(); ++i)
        p.Update(1.0F);
    CHECK(p.UiData()["bubbles"].empty());
}

TEST_CASE("A bot's level choice never opens the player's level screen (T344)", "[match][play]")
{
    const MatchDef def = Data();
    MatchState s = NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    int bot = -1;
    for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
        if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 1)
            bot = i;
    s.units[static_cast<std::size_t>(bot)].pending = 0;
    s.offer = {{1, 1, 1}, {1, 2, 1}, {1, 0, 4}};
    REQUIRE(s.units[static_cast<std::size_t>(bot)].pending >= 0);
    MatchPlay p(def, s, BotLevel::Medium);
    p.SetWords(Words());
    p.SetOptions(Pace());
    p.Update(2.0F);
    CHECK(Current(p.State()) == bot);
    CHECK(p.Screen() != "level");
    p.Update(1.0F); // the bot chooses on its beat
    CHECK(p.State().units[static_cast<std::size_t>(bot)].pending == -1);
}

TEST_CASE("Treasure is its own screen: face-down cards, a pick, the reveal (T346)", "[match][play]")
{
    const MatchDef def = Data();
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        for (const Floor &f : s.floor)
            if (f.treasure)
            {
                s.units[static_cast<std::size_t>(Current(s))].cell = s.now.origin = f.cell;
                break;
            }
    });
    p.Click(p.State().units[static_cast<std::size_t>(Current(p.State()))].cell);
    REQUIRE(p.HandleUi("", "act", "pickup"));
    REQUIRE(p.Screen() == "treasure");
    nlohmann::json t = p.UiData()["treasure"];
    REQUIRE(t["cards"].size() == p.State().cards.size());
    CHECK(t["result"] == "");
    REQUIRE(p.HandleUi("treasure", "pick", "1"));
    t = p.UiData()["treasure"];
    CHECK_FALSE(t["result"].get<std::string>().empty());
    CHECK(p.Screen() == "treasure");
    REQUIRE(p.HandleUi("treasure", "close", ""));
    CHECK(p.Screen() != "treasure"); // a card of XP may open the level choice next
}

TEST_CASE("An event's roll shows two dice against a target that matches its chance, landing as the rules decided (T349)", "[match][play]")
{
    CHECK(MatchPlay::DiceTarget(100) == 2);
    CHECK(MatchPlay::DiceTarget(58) == 7);
    CHECK(MatchPlay::DiceTarget(50) == 7);
    CHECK(MatchPlay::DiceTarget(17) == 10);
    CHECK(MatchPlay::DiceTarget(0) == 13);
    const MatchDef def = Data();
    for (std::uint64_t seed = 1; seed <= 12; ++seed)
    {
        MatchPlay q = AtPlayer(def, [&](MatchState &s) {
            s.rng ^= seed * 0x9E3779B97F4A7C15ULL;
            for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
                if (s.board.cells[static_cast<std::size_t>(c)].event >= 0 && std::ranges::none_of(s.units, [&](const Unit &u) { return u.cell == c; }))
                {
                    s.units[static_cast<std::size_t>(Current(s))].cell = s.now.origin = static_cast<std::int16_t>(c);
                    break;
                }
        });
        q.Click(q.State().units[static_cast<std::size_t>(Current(q.State()))].cell);
        REQUIRE(q.HandleUi("", "act", "answer"));
        const int chance = q.UiData()["event"]["choices"][0]["chance"];
        REQUIRE(q.HandleUi("event", "choose", "0"));
        const nlohmann::json d = q.UiData()["event"]["dice"];
        CHECK(d["target"] == MatchPlay::DiceTarget(chance));
        const int a = d["a"], b = d["b"];
        CHECK(a >= 1); CHECK(a <= 6); CHECK(b >= 1); CHECK(b <= 6);
        const bool passed = std::ranges::any_of(q.State().log, [](const LogEntry &e) { return e.kind == LogKind::Event && e.c; });
        CHECK((a + b >= d["target"].get<int>()) == passed);
        CHECK(d["passed"] == passed);
    }
}

TEST_CASE("A battle is staged while it lasts and a little after its last blow is told (T350)", "[match][play]")
{
    const MatchDef def = Data();
    int enemy = -1;
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        const int me = Current(s);
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 1)
                enemy = i;
        s.units[static_cast<std::size_t>(enemy)].cell = static_cast<std::int16_t>(FreeBeside(s, s.units[static_cast<std::size_t>(me)].cell));
        s.units[static_cast<std::size_t>(me)].hp = s.units[static_cast<std::size_t>(enemy)].hp = 90;
        s.units[static_cast<std::size_t>(me)].maxHp = s.units[static_cast<std::size_t>(enemy)].maxHp = 90;
    });
    const int me = Current(p.State());
    CHECK_FALSE(p.UiData().contains("fight")); // 1. No battle, nothing staged.
    p.Click(p.State().units[static_cast<std::size_t>(me)].cell);
    REQUIRE(p.HandleUi("", "act", "attack"));
    p.Click(p.State().units[static_cast<std::size_t>(enemy)].cell);
    // 2. The fighters, attacker first, while the battle runs.
    REQUIRE(p.UiData().contains("fight"));
    CHECK(p.UiData()["fight"] == nlohmann::json{me, -1, enemy, -1}); // by place: two a group
    while (p.State().battle.on)
    {
        if (p.PlayerTurn())
            REQUIRE(p.HandleUi("battle", "pick", "0"));
        p.Update(0.5F);
    }
    // 3. It stays staged until its last lines are told and a beat has passed, then the board comes back.
    CHECK(p.UiData().contains("fight"));
    for (int i = 0; i < 40 && p.UiData().contains("fight"); ++i)
        p.Update(0.25F);
    CHECK_FALSE(p.UiData().contains("fight"));
}

TEST_CASE("Whoever a told line is about is in focus for the camera, for as long as the line stays (T352)", "[match][play]")
{
    const MatchDef def = Data();
    int enemy = -1;
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        const int me = Current(s);
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 1)
                enemy = i;
        s.units[static_cast<std::size_t>(enemy)].cell = static_cast<std::int16_t>(FreeBeside(s, s.units[static_cast<std::size_t>(me)].cell));
        s.units[static_cast<std::size_t>(me)].hp = s.units[static_cast<std::size_t>(enemy)].hp = 90;
        s.units[static_cast<std::size_t>(me)].maxHp = s.units[static_cast<std::size_t>(enemy)].maxHp = 90;
    });
    const int me = Current(p.State());
    CHECK_FALSE(p.UiData().contains("focus"));
    p.Click(p.State().units[static_cast<std::size_t>(me)].cell);
    REQUIRE(p.HandleUi("", "act", "attack"));
    p.Click(p.State().units[static_cast<std::size_t>(enemy)].cell);
    for (int i = 0; i < 40 && !p.PlayerTurn(); ++i) // the attack is told in beats (T357)
        p.Update(0.25F);
    REQUIRE(p.HandleUi("battle", "pick", "0"));
    p.Update(0.25F); // the blow is told a beat after the pick
    // 1. The blow just told: striker and struck.
    REQUIRE(p.UiData().contains("focus"));
    const std::vector<int> focus = p.UiData()["focus"].get<std::vector<int>>();
    CHECK(std::ranges::find(focus, me) != focus.end());
    CHECK(std::ranges::find(focus, enemy) != focus.end());
}

TEST_CASE("A battle is told blow by blow: cards follow the told lines, the fallen stand until their fall (T357)", "[match][play]")
{
    const MatchDef def = Data();
    int enemy = -1;
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        const int me = Current(s);
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 1)
                enemy = i;
        s.units[static_cast<std::size_t>(enemy)].cell = static_cast<std::int16_t>(FreeBeside(s, s.units[static_cast<std::size_t>(me)].cell));
        s.units[static_cast<std::size_t>(me)].hp = s.units[static_cast<std::size_t>(me)].maxHp = 90;
        s.units[static_cast<std::size_t>(enemy)].hp = 1; // the first blow downs it
        s.units[static_cast<std::size_t>(enemy)].speed = 0; // and I strike first
    });
    const int me = Current(p.State());
    p.Click(p.State().units[static_cast<std::size_t>(me)].cell);
    REQUIRE(p.HandleUi("", "act", "attack"));
    p.Click(p.State().units[static_cast<std::size_t>(enemy)].cell);
    for (int i = 0; i < 20 && !p.PlayerTurn(); ++i)
        p.Update(0.5F);
    REQUIRE(p.Screen() == "battle");
    REQUIRE(p.HandleUi("battle", "pick", "0"));
    // 1. The rules have downed it, but nothing is told yet: its card still shows 1 HP and it still stands.
    REQUIRE(p.State().units[static_cast<std::size_t>(enemy)].cell < 0);
    nlohmann::json ui = p.UiData();
    CHECK(ui["battle"]["fighters"][1]["hp"] == 1);
    REQUIRE(ui.contains("standing"));
    const std::vector<int> standing = ui["standing"].get<std::vector<int>>();
    CHECK(std::ranges::find(standing, enemy) != standing.end());
    CHECK_FALSE(p.PlayerTurn()); // the fight is still being told
    // 2. A beat later the blow is told: 0 HP; then the fall: no longer standing.
    for (int i = 0; i < 40 && p.UiData().contains("standing"); ++i)
        p.Update(0.25F);
    CHECK_FALSE(p.UiData().contains("standing"));
    CHECK(p.Screen() == "battle"); // still staged a beat after its last line
    CHECK(p.UiData()["battle"]["fighters"][1]["hp"] == 0);
}

TEST_CASE("A match starts the player's characters from the career and writes them back with the takings (T364)", "[match][play]")
{
    const MatchDef def = Data();
    const int mai = MatchDef::Find(def.characters, "mai");
    // 1. A career where Mai is level 3 with Bloom learned and Tap upgraded.
    umbra::cafe::Career career;
    umbra::cafe::CharacterProgress p;
    p.id = "mai", p.level = 3, p.xp = 8, p.maxHp = 26, p.attack = 4, p.abilities = {"tap", "tend", "bloom"};
    p.boost[0] = 2;
    career.characters.push_back(p);
    MatchState s = NewMatch(def, 7, {mai, MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang")});
    umbra::night::ApplyCareer(def, s, career);
    int unit = -1;
    for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
        if (s.units[static_cast<std::size_t>(i)].kind == 0 && s.units[static_cast<std::size_t>(i)].faction == 0 && s.units[static_cast<std::size_t>(i)].def == mai)
            unit = i;
    REQUIRE(unit >= 0);
    const Unit &u = s.units[static_cast<std::size_t>(unit)];
    CHECK(u.level == 3);
    CHECK(u.maxHp == 26);
    CHECK(u.hp == 26);
    CHECK(u.attack == 4);
    CHECK(u.abilities[2] == MatchDef::Find(def.abilities, "bloom"));
    CHECK(u.boost[0] == 2);
    // The other side's Mai is untouched.
    for (const Unit &o : s.units)
        if (o.kind == 0 && o.faction == 1 && o.def == mai)
            CHECK(o.level == 1);
    // 2. After the match: progress written back, the takings added, a new day.
    s.units[static_cast<std::size_t>(unit)].level = 4;
    s.score[0] = 30;
    const int earned = umbra::night::RecordMatch(def, s, career, 2);
    CHECK(earned == 60);
    CHECK(career.money == 60);
    CHECK(career.day == 2);
    CHECK(career.Find("mai")->level == 4);
    CHECK(career.Find("bao") != nullptr);
}

TEST_CASE("A brain set from outside plays the other side (campaign plan 4)", "[match][play]")
{
    const MatchDef def = Data();
    MatchState s = NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    while (s.units[static_cast<std::size_t>(Current(s))].faction == 0)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    MatchPlay p(def, s, BotLevel::Medium);
    p.SetWords(Words());
    p.SetOptions(Pace());
    int asked = 0;
    p.SetBrain([&](const MatchState &) {
        ++asked;
        return Action{ActionKind::EndTurn};
    });
    for (int i = 0; i < 40 && !p.PlayerTurn(); ++i)
        p.Update(2.0F);
    CHECK(asked > 0);
    CHECK(p.PlayerTurn());
}

TEST_CASE("A paused fight stands still; it is quiet only between told beats (campaign plan 4)", "[match][play]")
{
    const MatchDef def = Data();
    MatchState s = NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    while (s.units[static_cast<std::size_t>(Current(s))].faction == 0)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    MatchPlay p(def, s, BotLevel::Medium);
    p.SetWords(Words());
    p.SetOptions(Pace());
    // 1. Paused: nothing moves however long it waits.
    p.Pause(true);
    const std::size_t lines = p.State().log.size();
    const int turn = p.State().turn;
    for (int i = 0; i < 20; ++i)
        p.Update(2.0F);
    CHECK(p.State().log.size() == lines);
    CHECK(p.State().turn == turn);
    // 2. Resumed: the bots play; whenever a battle is on the table, it is not quiet.
    p.Pause(false);
    bool sawBattle = false;
    for (int i = 0; i < 400 && !p.PlayerTurn(); ++i)
    {
        p.Update(0.1F);
        if (p.Screen() == "battle")
        {
            sawBattle = true;
            CHECK_FALSE(p.Quiet());
        }
        CHECK(p.Told() <= p.State().log.size());
    }
    CHECK(p.PlayerTurn());
    for (int i = 0; i < 100 && !p.Quiet(); ++i)
        p.Update(0.5F);
    CHECK(p.Quiet());
    (void)sawBattle;
}

namespace
{
    // Every "text" in a screen file, walked through its children.
    void Texts(const nlohmann::json &node, std::vector<std::string> &out)
    {
        if (node.is_object())
        {
            if (node.contains("text") && node["text"].is_string())
                out.push_back(node["text"].get<std::string>());
            for (const auto &[key, v] : node.items())
                Texts(v, out);
        }
        else if (node.is_array())
            for (const auto &v : node)
                Texts(v, out);
    }

    std::set<std::string> Keys(const nlohmann::json &j, const std::string &at = "")
    {
        std::set<std::string> out;
        for (const auto &[key, v] : j.items())
        {
            out.insert(at + key);
            if (v.is_object())
                out.merge(Keys(v, at + key + "."));
        }
        return out;
    }
}

TEST_CASE("The fight's screens carry no typed words; both words files have the same keys (campaign plan 5)", "[match][play][words]")
{
    const std::filesystem::path ui = std::filesystem::path(UMBRA_LASTCURTAIN_ASSETS) / "ui";
    for (const auto &file : std::filesystem::directory_iterator(ui))
    {
        const std::string name = file.path().filename().string();
        if (!name.ends_with(".ui.json") || !(name.starts_with("match-") || name.starts_with("dialog") || name.starts_with("campaign-")))
            continue;
        std::vector<std::string> texts;
        Texts(nlohmann::json::parse(std::ifstream(file.path())), texts);
        for (const std::string &t : texts)
        {
            INFO(name << ": " << t);
            const bool letters = std::ranges::any_of(t, [](char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; });
            CHECK((!letters || t.find('{') != std::string::npos));
        }
    }
    const nlohmann::json en = Words(), vi = nlohmann::json::parse(std::ifstream(std::filesystem::path(UMBRA_LASTCURTAIN_ASSETS) / "match-words.vi.json"));
    std::set<std::string> a = Keys(en), b = Keys(vi);
    b.erase("note");
    CHECK(a == b);
    CHECK(en.contains("screens"));
}

TEST_CASE("A campaign fight shows cast names; the HUD has goals, this round's strip, a hint and the last log line (campaign plan 5)", "[match][play][hud]")
{
    const MatchDef def = Data();
    MatchState s = NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    std::vector<std::string> names;
    for (std::size_t i = 0; i < s.units.size(); ++i)
        names.push_back("Unit" + std::to_string(i));
    // Unit 3 is the boss, unit 4 a strung recruit; the stage asks to free it.
    const int boss = 3, recruit = 4;
    s.units[boss].flags = flag::Boss;
    s.units[recruit].flags = flag::Recruit | flag::Strung;
    s.mode.mustFree = true;
    MatchPlay p(def, s, BotLevel::Medium);
    p.SetWords(Words());
    p.SetOptions(Pace());
    p.SetCast(names, {});
    p.SetStage("Test room");
    // 1. Names everywhere.
    CHECK(p.UiData()["order"][0]["name"] == names[static_cast<std::size_t>(Current(p.State()))]);
    // 2. Goals: down the boss, free the recruit.
    nlohmann::json hud = p.HudData();
    CHECK(hud["goals"]["title"] == "Test room");
    REQUIRE(hud["goals"]["lines"].size() == 2);
    CHECK(hud["goals"]["lines"][0]["text"] == "Defeat Unit3");
    CHECK(hud["goals"]["lines"][0]["done"] == false);
    CHECK(hud["goals"]["lines"][1]["text"] == "Cut Unit4 free");
    // 3. The strip: this round from the one acting, the acting first and marked.
    REQUIRE_FALSE(hud["strip"].empty());
    CHECK(hud["strip"][0]["unit"] == Current(p.State()));
    CHECK(hud["strip"][0]["acting"] == true);
    CHECK(hud["strip"].size() <= p.State().order.size());
    // 4. Hint and log line exist; an enemy's turn says whose.
    CHECK_FALSE(hud["hint"].get<std::string>().empty());
    CHECK(hud.contains("logLine"));
    for (int i = 0; i < 400 && p.State().units[static_cast<std::size_t>(Current(p.State()))].faction == 0; ++i)
        if (!p.PlayerTurn() || !p.Do({ActionKind::EndTurn}))
            p.Update(0.5F);
    CHECK(p.HudData()["hint"].get<std::string>().find(names[static_cast<std::size_t>(Current(p.State()))]) != std::string::npos);
}

TEST_CASE("Goals tick and fail: the boss down, the recruit freed, a trapped loss; many bosses are one line (campaign plan 5)", "[match][play][hud]")
{
    const MatchDef def = Data();
    MatchState s = NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    for (std::size_t i = 3; i < 6 && i < s.units.size(); ++i)
        s.units[i].flags = flag::Boss;
    MatchPlay many(def, s, BotLevel::Medium);
    many.SetWords(Words());
    REQUIRE(many.HudData()["goals"]["lines"].size() == 1);
    CHECK(many.HudData()["goals"]["lines"][0]["text"] == "Defeat every enemy");

    s.units[4].flags = flag::Recruit | flag::Freed;
    s.units[5].flags = 0;
    s.units[3].hp = 0;
    s.mode.mustFree = true;
    s.over = true, s.winner = 1, s.loss = Loss::Trapped;
    MatchPlay done(def, s, BotLevel::Medium);
    done.SetWords(Words());
    const nlohmann::json lines = done.HudData()["goals"]["lines"];
    REQUIRE(lines.size() == 2);
    CHECK(lines[0]["done"] == true);
    CHECK(lines[1]["done"] == true);
    CHECK(done.HudData()["goals"]["failed"] == true);
}

namespace
{
    std::vector<std::string> RowIds(const MatchPlay &p)
    {
        std::vector<std::string> ids;
        const nlohmann::json menu = p.Menu(); // kept: the loop must not outlive it
        for (const auto &r : menu["rows"])
            ids.push_back(r["id"]);
        return ids;
    }
}

TEST_CASE("The command menu: rows by situation, number keys, sub-menus and the back stack (campaign plan 5)", "[match][play][menu]")
{
    const MatchDef def = Data();
    MatchPlay p = AtPlayer(def, [](MatchState &) {});
    const int me = Current(p.State()), from = p.State().units[static_cast<std::size_t>(me)].cell;
    // 1. Before moving: move, stay or end. Move hides the menu (the squares under it can be clicked); Back
    //    brings it back; key 2 stays.
    CHECK(RowIds(p) == std::vector<std::string>{"move", "stay", "end"});
    CHECK(p.Menu()["rows"][0]["key"] == "1");
    REQUIRE(p.MenuKey(1));
    CHECK(RowIds(p).empty());
    CHECK(p.Back());
    CHECK(RowIds(p) == std::vector<std::string>{"move", "stay", "end"});
    REQUIRE(p.MenuKey(2));
    // 2. The main menu: move, attack, skills, items, end (cut only beside a fallen strung one).
    std::vector<std::string> ids = RowIds(p);
    CHECK(ids.front() == "undo");
    CHECK(std::ranges::find(ids, "attack") != ids.end());
    CHECK(std::ranges::find(ids, "skills") != ids.end());
    CHECK(std::ranges::find(ids, "cut") == ids.end());
    CHECK(ids.back() == "end");
    // 3. Skills opens a sub-menu; Back closes it, then Back returns to walking.
    const int skills = static_cast<int>(std::ranges::find(ids, "skills") - ids.begin()) + 1;
    if (!p.Menu()["rows"][skills - 1]["off"].get<bool>())
    {
        REQUIRE(p.MenuKey(skills));
        CHECK(p.Menu()["title"] != "");
        CHECK(p.Back());
        CHECK(RowIds(p).front() == "undo");
    }
    CHECK(p.Back());
    CHECK(RowIds(p) == std::vector<std::string>{"move", "stay", "end"});
    CHECK(p.State().units[static_cast<std::size_t>(me)].cell == from);
    // 4. Keyboard only: stay, then end with its key; the turn passes.
    REQUIRE(p.MenuKey(2));
    REQUIRE(p.MenuKey(static_cast<int>(RowIds(p).size())));
    CHECK(Current(p.State()) != me);
}

TEST_CASE("Armed: the menu lists the targets with keys; Back disarms (campaign plan 5)", "[match][play][menu]")
{
    const MatchDef def = Data();
    MatchPlay p = AtPlayer(def, [](MatchState &s) {
        // A foe right beside the one about to act.
        const int me = Current(s);
        for (std::size_t i = 0; i < s.units.size(); ++i)
            if (s.units[i].faction != s.units[static_cast<std::size_t>(me)].faction && Up(s.units[i]))
            {
                for (int d = 0; d < 4; ++d)
                    if (const int n = Neighbour(s.board, s.units[static_cast<std::size_t>(me)].cell, d); n >= 0)
                    {
                        s.units[i].cell = static_cast<std::int16_t>(n);
                        break;
                    }
                break;
            }
    });
    REQUIRE(p.MenuKey(2)); // stay
    const std::vector<std::string> ids = RowIds(p);
    const int attack = static_cast<int>(std::ranges::find(ids, "attack") - ids.begin()) + 1;
    INFO(p.Menu().dump());
    REQUIRE(p.MenuKey(attack));
    // Armed: no panel over the board (feedback 2026-11-24 15:10, note 6); the red squares are the targets.
    CHECK(p.Menu()["rows"].empty());
    const nlohmann::json targets = p.UiData()["targets"];
    REQUIRE_FALSE(targets.empty());
    CHECK(p.Back());
    CHECK(RowIds(p) == ids);
    REQUIRE(p.MenuKey(attack));
    const std::size_t lines = p.State().log.size();
    p.Click(targets[0].get<int>());
    CHECK(p.State().log.size() > lines); // the attack happened
}

TEST_CASE("Skill and item rows: no cost note when free, what it does as the tip, its story first when known (feedback 2026-11-24 15:10, notes 9 and 14)", "[match][play][menu]")
{
    const MatchDef def = Data();
    // Two board skills in the acting unit's slots: one free, one that costs HP (when the data has them).
    int free = -1, costly = -1;
    for (int a = 0; a < static_cast<int>(def.abilities.size()); ++a)
        if (def.abilities[static_cast<std::size_t>(a)].use == Use::Board)
            (def.abilities[static_cast<std::size_t>(a)].hpCost > 0 ? costly : free) = a;
    REQUIRE(free >= 0);
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        Unit &me = s.units[static_cast<std::size_t>(Current(s))];
        me.abilities[1] = static_cast<std::int8_t>(free), me.abilities[2] = static_cast<std::int8_t>(costly >= 0 ? costly : free);
    });
    nlohmann::json words = Words();
    const Unit &u = p.State().units[static_cast<std::size_t>(Current(p.State()))];
    int board = -1;
    for (const int a : u.abilities)
        if (a >= 0 && def.abilities[static_cast<std::size_t>(a)].use == Use::Board && board < 0)
            board = a;
    if (board >= 0)
        words["lore"][def.abilities[static_cast<std::size_t>(board)].id] = "A line of story.";
    p.SetWords(words);
    REQUIRE(p.MenuKey(2)); // stay
    REQUIRE(p.MenuPick("skills"));
    const nlohmann::json menu = p.Menu(); // kept: the loop must not outlive it
    REQUIRE_FALSE(menu["rows"].empty());
    for (const auto &r : menu["rows"])
    {
        const AbilityDef &a = def.abilities[static_cast<std::size_t>(u.abilities[static_cast<std::size_t>(std::stoi(r["id"].get<std::string>().substr(8)))])];
        CHECK(r["note"] == (a.hpCost > 0 ? "-" + std::to_string(a.hpCost) + " HP" : ""));
        REQUIRE(r.contains("tip"));
        CHECK_FALSE(r["tip"].get<std::string>().empty());
        if (r["off"].get<bool>())
            CHECK(r["tip"] == r["why"]); // off: the tip says why
        else if (&a == &def.abilities[static_cast<std::size_t>(board)])
            CHECK(r["tip"].get<std::string>().starts_with("A line of story."));
    }
}

TEST_CASE("No lit squares and no walking before Di chuyen; undo jumps back (feedback 2026-11-24 15:10, notes 2 and 7)", "[match][play][menu]")
{
    const MatchDef def = Data();
    MatchPlay p = AtPlayer(def, [](MatchState &) {});
    const int me = Current(p.State()), from = p.State().units[static_cast<std::size_t>(me)].cell;
    // 1. Before Di chuyen: nothing lit; a click on a square it could reach does nothing; the hint says what to pick.
    CHECK(p.UiData().value("reach", nlohmann::json::array()).empty());
    CHECK(p.UiData()["hint"] == Words()["hints"]["start"]);
    REQUIRE(p.MenuKey(1)); // Di chuyen
    const nlohmann::json reach = p.UiData()["reach"];
    REQUIRE_FALSE(reach.empty());
    const int to = reach[0].get<int>();
    CHECK(p.Back());
    p.Click(to);
    CHECK(p.State().units[static_cast<std::size_t>(me)].cell == from);
    // 2. After it, the click walks; undo puts the figure straight back (a snap, not a walk).
    REQUIRE(p.MenuKey(1));
    p.Click(to);
    CHECK(p.State().units[static_cast<std::size_t>(me)].cell == to);
    (void)p.TakeSnaps();
    REQUIRE(p.MenuPick("undo"));
    CHECK(p.State().units[static_cast<std::size_t>(me)].cell == from);
    CHECK(p.TakeSnaps() == std::vector<int>{me});
}

TEST_CASE("A campaign stage opens with a start banner and ends with its outcome before anything else (feedback 2026-11-24 15:10, notes 5 and 11)", "[match][play][flow]")
{
    const MatchDef def = Data();
    MatchState s = NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    const nlohmann::json words = Words();
    // 1. Round 1 in a stage: the banner says the fight begins, not a turn number.
    MatchPlay start(def, s, BotLevel::Medium);
    start.SetWords(words), start.SetOptions(Pace()), start.SetCast(std::vector<std::string>(s.units.size(), "x"), {});
    CHECK(start.Screen() == "banner");
    CHECK(start.UiData()["banner"]["title"] == words["banner"]["first"]);
    // 2. Over: the outcome banner first (not quiet, so no scene starts), then nothing: no end screen in a stage.
    s.over = true, s.winner = 0;
    MatchPlay end(def, s, BotLevel::Medium);
    end.SetWords(words), end.SetOptions(Pace()), end.SetCast(std::vector<std::string>(s.units.size(), "x"), {});
    CHECK(end.Screen() == "banner");
    CHECK(end.UiData()["banner"]["title"] == words["outcome"]["won"]);
    CHECK_FALSE(end.Quiet());
    end.Update(5.0F);
    CHECK(end.Screen().empty());
    CHECK(end.Quiet());
    // 3. A plain match keeps its end screen.
    MatchPlay plain(def, s, BotLevel::Medium);
    plain.SetWords(words), plain.SetOptions(Pace());
    CHECK(plain.Screen() == "end");
}

TEST_CASE("Target preview equals the rules' damage; the card shows the hovered unit and can be pinned (campaign plan 5)", "[match][play][preview]")
{
    const MatchDef def = Data();
    int foe = -1;
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        const int me = Current(s);
        for (std::size_t i = 0; i < s.units.size() && foe < 0; ++i)
            if (s.units[i].faction != s.units[static_cast<std::size_t>(me)].faction && Up(s.units[i]))
                for (int d = 0; d < 4 && foe < 0; ++d)
                    if (const int n = Neighbour(s.board, s.units[static_cast<std::size_t>(me)].cell, d); n >= 0)
                        s.units[i].cell = static_cast<std::int16_t>(n), foe = static_cast<int>(i);
    });
    const MatchState &s = p.State();
    const int me = Current(s);
    // 1. The attack's preview: the best battle strike each way, as the rules' Damage gives it.
    const nlohmann::json f = p.PreviewFor({ActionKind::Attack, static_cast<std::int8_t>(foe)});
    int best = 0, counter = 0;
    for (int slot = 0; slot < maxAbilities; ++slot)
    {
        const int a = s.units[static_cast<std::size_t>(me)].abilities[static_cast<std::size_t>(slot)];
        if (a >= 0 && def.abilities[static_cast<std::size_t>(a)].use == Use::Battle && def.abilities[static_cast<std::size_t>(a)].kind == AbilityClass::Strike)
            best = std::max(best, Damage(def, s, a, me, foe, def.abilities[static_cast<std::size_t>(a)].dice.count * def.abilities[static_cast<std::size_t>(a)].dice.sides));
        const int b = s.units[static_cast<std::size_t>(foe)].abilities[static_cast<std::size_t>(slot)];
        if (b >= 0 && def.abilities[static_cast<std::size_t>(b)].use == Use::Battle && def.abilities[static_cast<std::size_t>(b)].kind == AbilityClass::Strike)
            counter = std::max(counter, Damage(def, s, b, foe, me, def.abilities[static_cast<std::size_t>(b)].dice.count * def.abilities[static_cast<std::size_t>(b)].dice.sides));
    }
    CHECK(f["max"] == best);
    CHECK(f["counterMax"] == counter);
    CHECK(f["min"].get<int>() <= f["max"].get<int>());
    // 2. Armed, hovering the target's square shows the same numbers on its card, one number when the roll cannot vary.
    REQUIRE(p.MenuKey(2)); // stay
    const nlohmann::json main = p.Menu()["rows"];
    for (std::size_t i = 0; i < main.size(); ++i)
        if (main[i]["id"] == "attack")
            REQUIRE(p.MenuKey(static_cast<int>(i) + 1));
    p.Hover(s.units[static_cast<std::size_t>(foe)].cell);
    const nlohmann::json aimed = p.UiData()["forecast"];
    REQUIRE(aimed["kind"] == "target");
    CHECK(aimed["dealtMax"] == best);
    CHECK(aimed["counterMax"] == counter);
    CHECK(aimed["dealtText"] == (f["min"] == f["max"] ? std::to_string(best) : std::to_string(f["min"].get<int>()) + "\u2013" + std::to_string(best)));
    // 3. The card: the hovered unit's numbers; pinned, it stays when the mouse leaves.
    p.Hover(s.units[static_cast<std::size_t>(foe)].cell);
    nlohmann::json card = p.HudData()["card"];
    CHECK(card["unit"] == foe);
    CHECK(card["maxHp"] == s.units[static_cast<std::size_t>(foe)].maxHp);
    CHECK(card.contains("speed"));
    CHECK(p.Pin());
    p.Hover(-1);
    CHECK(p.HudData()["card"]["unit"] == foe);
    CHECK(p.Pin()); // unpinned
    CHECK(p.HudData()["card"].is_null());
}

TEST_CASE("Telegraphs: hazards, breath rows, opening windows and arrivals marked a round ahead (campaign plan 5)", "[match][play][marks]")
{
    MatchState s;
    s.round = 3;
    s.gimmicks.marks = {{Mark::Kind::Hazard, 5}, {Mark::Kind::Breath, 9}, {Mark::Kind::Window, 12}};
    s.arrivals = {{0, 20, 4}, {1, 21, 7}};
    const nlohmann::json t = MatchPlay::Telegraphs(s);
    REQUIRE(t.size() == 4); // the arrival at round 7 is not shown yet
    CHECK(t[0]["cell"] == 5);
    CHECK(t[0]["kind"] == "hazard");
    CHECK(t[1]["kind"] == "breath");
    CHECK(t[2]["kind"] == "window");
    CHECK(t[3]["cell"] == 20);
    CHECK(t[3]["kind"] == "arrival");
}

TEST_CASE("Toasts: finds, freeing and levels; at most three, each for three seconds (campaign plan 5)", "[match][play][toasts]")
{
    const MatchDef def = Data();
    MatchPlay p = Make(def);
    CHECK(p.ToastText({LogKind::Moved, 0}).empty());
    CHECK_FALSE(p.ToastText({LogKind::Freed, 0, 1, 0}).empty());
    CHECK_FALSE(p.ToastText({LogKind::Level, 0, -1, 3}).empty());
    for (const char *t : {"a", "b", "c", "d"})
        p.Toast(t);
    nlohmann::json toasts = p.HudData()["toasts"];
    REQUIRE(toasts.size() == 3);
    CHECK(toasts[0]["text"] == "b"); // the oldest gave way
    p.Update(1.5F);
    CHECK(p.HudData()["toasts"].size() == 3);
    p.Update(2.0F);
    CHECK(p.HudData()["toasts"].empty());
}

TEST_CASE("The board cursor: starts on the acting unit, steps a square, stays on the board, repeats when held (plan 8, P4)", "[match][play][cursor]")
{
    BoardCursor c;
    // 1. The first press starts it where it is told (the acting unit) and steps from there.
    CHECK(c.Update({1, 0}, 0.016F, 5, 4, 7)); // 7 = (2, 1)
    CHECK(c.cell == 8);
    // 2. Held: nothing until 0.25 s, then a step every 0.08 s.
    CHECK_FALSE(c.Update({1, 0}, 0.2F, 5, 4, 7));
    CHECK(c.Update({1, 0}, 0.06F, 5, 4, 7));
    CHECK(c.cell == 9);
    CHECK_FALSE(c.Update({1, 0}, 0.05F, 5, 4, 7));
    // 3. The edge stops it; letting go and pressing again steps at once.
    CHECK_FALSE(c.Update({1, 0}, 0.1F, 5, 4, 7)); // x 4 is the last column
    CHECK(c.cell == 9);
    CHECK_FALSE(c.Update({0, 0}, 0.016F, 5, 4, 7));
    CHECK(c.Update({0, 1}, 0.016F, 5, 4, 7));
    CHECK(c.cell == 14);
    // 4. Off (the mouse moved): the next press starts again from the unit.
    c.Off();
    CHECK(c.Update({-1, 0}, 0.016F, 5, 4, 12));
    CHECK(c.cell == 11);
}
