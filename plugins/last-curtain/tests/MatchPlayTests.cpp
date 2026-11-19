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
    // 1. The player picks first: both fighters, the round, the picks with damage.
    REQUIRE(p.Screen() == "battle");
    nlohmann::json b = p.UiData()["battle"];
    CHECK(b["fighters"][0]["unit"] == me);
    CHECK(b["fighters"][1]["unit"] == enemy);
    CHECK(b["round"] == 1);
    REQUIRE_FALSE(b["picks"].empty());
    CHECK(b["picks"][0]["dealtMin"].get<int>() >= 1);
    REQUIRE(p.HandleUi("battle", "pick", "0"));
    // 2. The bot answers on a beat; the blow's line shows.
    CHECK_FALSE(p.PlayerTurn());
    for (int i = 0; i < 20 && !p.PlayerTurn(); ++i)
        p.Update(0.5F);
    REQUIRE(p.PlayerTurn());
    b = p.UiData()["battle"];
    CHECK(b["round"] == 2);
    CHECK_FALSE(b["line"].get<std::string>().empty());
    while (p.Screen() == "battle" && p.PlayerTurn())
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
        s.battle = {true, static_cast<std::int8_t>(hunter), static_cast<std::int8_t>(mine), 1, 1, 0, {}};
    });
    REQUIRE(h.Screen() == "battle");
    CHECK(def.monsters[static_cast<std::size_t>(h.State().units[static_cast<std::size_t>(h.UiData()["battle"]["fighters"][0]["unit"].get<int>())].def)].behaviour == Behaviour::Hunt);
    CHECK_FALSE(h.UiData()["battle"]["picks"].empty());
}

TEST_CASE("Level choices and events are their own screens (T329)", "[match][play]")
{
    const MatchDef def = Data();
    // 1. A level choice: the two abilities; picking one learns it.
    int row = -1;
    MatchPlay p = AtPlayer(def, [&](MatchState &s) {
        const Unit &u = s.units[static_cast<std::size_t>(Current(s))];
        for (int j = 0; j < static_cast<int>(def.jobs.size()); ++j)
            if (def.jobs[static_cast<std::size_t>(j)].role == def.characters[static_cast<std::size_t>(u.def)].role && def.jobs[static_cast<std::size_t>(j)].choice >= 0)
                row = j;
        s.units[static_cast<std::size_t>(Current(s))].pending = static_cast<std::int8_t>(row);
    });
    REQUIRE(p.Screen() == "level");
    const nlohmann::json level = p.UiData()["level"];
    REQUIRE(level["choices"].size() == 2);
    CHECK(level["choices"][1]["name"] == def.abilities[static_cast<std::size_t>(def.jobs[static_cast<std::size_t>(row)].choice)].name);
    REQUIRE(p.HandleUi("level", "pick", "1"));
    CHECK(p.Screen() == "");
    const auto &abilities = p.State().units[static_cast<std::size_t>(Current(p.State()))].abilities;
    CHECK(std::ranges::find(abilities, def.jobs[static_cast<std::size_t>(row)].choice) != abilities.end());

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
    for (int i = 0; i < 400 && !p.State().over; ++i)
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
    sample["level"] = {{"name", "Dinh Quoc Bao"}, {"level", 2}, {"choices", {{{"index", 0}, {"name", "Cleave"}, {"power", 7}, {"cost", 3}}, {{"index", 1}, {"name", "Gamble"}, {"power", 0}, {"cost", 2}}}}};
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
