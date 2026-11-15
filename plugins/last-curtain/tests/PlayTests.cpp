#include "NightTestData.hpp"

#include <BoardView.hpp>
#include <Narrate.hpp>
#include <NightBots.hpp>
#include <NightSave.hpp>
#include <Play.hpp>
#include <Options.hpp>
#include <Words.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <thread>
#include <filesystem>

using namespace umbra::night;
using umbra::platform::InputKey;

namespace
{
    // Dismisses the event and dice screens, which hold the bots.
    void Settle(Play &play)
    {
        while (play.HandleUi("arrive", "onward", "") || play.HandleUi("event", "roll", "") || play.HandleUi("found", "take", "") ||
               play.HandleUi("dice", "continue", ""))
        {
        }
    }

    // Bots play until it is the player's own turn; a chat a bot opened is answered first.
    void ToPlayer(Play &play)
    {
        for (int i = 0; i < 400 && (!play.PlayerTurn() || play.State().chat.active || play.Screen() != "menu"); ++i)
        {
            Settle(play);
            if (play.State().chat.active && play.PlayerTurn())
                play.HandleUi("chat", "answer", "0");
            else
                play.Update(10.0F);
        }
        REQUIRE(play.PlayerTurn());
    }

    // A click on `cell`, then the walk there played out (a closed door: Stay).
    void GoTo(Play &play, int cell)
    {
        play.Click(cell);
        for (int i = 0; i < 40 && (play.Sliding().guest >= 0 || play.Screen() == "picking" || play.Screen() == "confirm"); ++i)
            if (play.Screen() == "confirm")
                play.HandleUi("confirm", "stay", "");
            else
                play.Update(Play::slideSeconds);
    }

    int StepCell(const NightDef &def, const Play &play)
    {
        const BoardView view = MakeView(def, play.State(), play.State().player);
        for (int c = 0; c < maxCells; ++c)
            if (view.cells[static_cast<std::size_t>(c)].step >= 0)
                return c;
        return -1;
    }
}

TEST_CASE("A click on a step cell moves; elsewhere nothing (T111)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    ToPlayer(play);
    const int me = play.State().player;
    const int start = play.State().guests[0].cell;
    // 1. A cell that is not a step does nothing.
    const NightState before = play.State();
    play.Click(start);
    play.Click(-1);
    play.Click(maxCells + 3);
    CHECK(play.State().logCount == before.logCount);
    CHECK(play.State().guests[0].cell == start);
    // 2. A step cell moves the pawn there.
    const int step = StepCell(def, play);
    REQUIRE(step >= 0);
    GoTo(play, step);
    CHECK(play.State().guests[static_cast<std::size_t>(me)].cell == step);
}

TEST_CASE("Keys do nothing on a bot's turn (T111)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    ToPlayer(play);
    play.Key(InputKey::Enter);
    REQUIRE_FALSE(play.PlayerTurn());
    const std::uint32_t log = play.State().logCount;
    const int turn = play.State().turn;
    for (const InputKey key : {InputKey::Up, InputKey::Right, InputKey::Down, InputKey::Left, InputKey::Enter})
        play.Key(key);
    for (int c = 0; c < maxCells; ++c)
        play.Click(c);
    CHECK(play.State().logCount == log);
    CHECK(play.State().turn == turn);
}

TEST_CASE("End turn works with moves left (T111)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    ToPlayer(play);
    const int turn = play.State().turn, round = play.State().round;
    REQUIRE(play.State().guests[0].movesLeft > 0);
    play.Key(InputKey::Enter);
    CHECK((play.State().turn != turn || play.State().round != round));
    // The same through the actions screen: its last entry ends the turn.
    ToPlayer(play);
    const int turn2 = play.State().turn;
    const nlohmann::json data = play.UiData();
    const nlohmann::json &end = data["actions"].back();
    CHECK(end["label"] == "End turn");
    CHECK(play.HandleUi("menu", end["action"], end["arg"]));
    CHECK(play.State().turn != turn2);
}

TEST_CASE("An ally is the player's to move (T111)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 4, 0);
    for (int g = 1; g < state.count; ++g)
        state.guests[static_cast<std::size_t>(g)].group = g == 1 ? 0 : static_cast<std::int8_t>(g);
    state.guests[0].group = 0;
    Play play(def, state);
    for (int i = 0; i < 400 && Current(play.State()) != 1; ++i)
        if (Settle(play); play.PlayerTurn())
            play.HandleUi("menu", "act", play.UiData()["actions"].back()["arg"]);
        else
            play.Update(10.0F);
    REQUIRE(Current(play.State()) == 1);
    CHECK(play.PlayerTurn());
    play.Update(10.0F); // no bot plays an ally
    CHECK(Current(play.State()) == 1);
    CHECK(play.UiData()["me"]["name"] == def.guests[1].name);
    const int step = StepCell(def, play);
    REQUIRE(step >= 0);
    GoTo(play, step);
    CHECK(play.State().guests[1].cell == step);
}

TEST_CASE("UiData has the keys the screens bind (T111)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    ToPlayer(play);
    const nlohmann::json data = play.UiData();
    for (const char *key : {"clock", "round", "phase", "me", "moves", "actions", "allies", "winning", "inLight", "bag", "floor", "bagUsed", "carry"})
        CHECK(data.contains(key));
    for (const char *key : {"name", "hp", "maxHp", "might", "sanity", "speed", "level", "buffs"})
        CHECK(data["me"].contains(key));
    REQUIRE(data["actions"].is_array());
    CHECK(data["actions"].back()["label"] == "End turn");
    CHECK(play.Screen() == "menu");
    // The bag opens and closes; it is free and does not end the turn.
    const int turn = play.State().turn;
    CHECK(play.HandleUi("menu", "bag", ""));
    CHECK(play.Screen() == "bag");
    CHECK(play.HandleUi("bag", "close", ""));
    CHECK(play.Screen() == "menu");
    CHECK(play.State().turn == turn);
    CHECK_FALSE(play.HandleUi("bag", "use", "9")); // an empty or unknown slot
    CHECK_FALSE(play.HandleUi("menu", "act", "999"));
    CHECK_FALSE(play.HandleUi("menu", "act", "x"));
}

TEST_CASE("Only recruited guests are allies (T111)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    CHECK(play.UiData()["allies"].empty());
    NightState state = FoyerNight(def, 4, 0);
    state.guests[0].group = state.guests[2].group = 0;
    state.approvals[3 * maxGuests + 0] = 1;
    const nlohmann::json data = Play(def, state).UiData();
    REQUIRE(data["allies"].size() == 1);
    CHECK(data["allies"][0]["name"] == def.guests[2].name);
    REQUIRE(data["winning"].size() == 1);
    CHECK(data["winning"][0]["name"] == def.guests[3].name);
}

namespace
{
    // A night whose first guest to act is a bot that does not open a chat, with captions.
    Play BotFirst(const NightDef &def)
    {
        for (std::uint64_t seed = 1; seed < 100; ++seed)
        {
            Play play(def, FoyerNight(def, seed, 0));
            const int bot = Current(play.State());
            if (!play.PlayerTurn() && ChooseAction(def, play.State(), bot).kind != ActionKind::OpenChat)
            {
                std::vector<umbra::core::Diagnostic> diagnostics;
                Captions captions;
                REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
                play.SetCaptions(captions);
                return play;
            }
        }
        FAIL("no seed with a bot first");
        return Play(def, FoyerNight(def, 1, 0));
    }
}

namespace
{
    // A seen bot's first action: it decides, thinks (Play::thinkSeconds), then acts.
    void Think(Play &play)
    {
        play.Update(0.01F);
        play.Update(Play::thinkSeconds + 0.01F);
    }
}

TEST_CASE("Bot turns advance with time and hold on seen beats (T112)", "[night][play]")
{
    const NightDef def = Data();
    Play play = BotFirst(def);
    Think(play); // the bot acts in the foyer, in the player's light
    REQUIRE(play.Beats() == 1);
    CHECK(play.Screen() == "caption");
    CHECK_FALSE(play.UiData()["caption"]["line"].get<std::string>().empty());
    play.Update(0.5F);
    CHECK(play.Beats() == 1); // still showing
    play.Update(1.1F);
    CHECK((play.Beats() == 2 || play.PlayerTurn()));
}

TEST_CASE("Space makes the bots' beats short (T112)", "[night][play]")
{
    const NightDef def = Data();
    Play play = BotFirst(def);
    play.Key(InputKey::Space);
    Think(play);
    REQUIRE(play.Beats() == 1);
    play.Update(0.35F);
    CHECK((play.Beats() == 2 || play.PlayerTurn()));
}

TEST_CASE("A minimised window does not lose a beat (T112)", "[night][play]")
{
    const NightDef def = Data();
    Play play = BotFirst(def);
    for (int i = 0; i < 100; ++i)
        play.Update(0.0F);
    CHECK(play.Beats() == 0);
    Think(play);
    const std::uint32_t log = play.State().logCount;
    for (int i = 0; i < 100; ++i)
        play.Update(0.0F);
    CHECK(play.Beats() == 1);
    CHECK(play.State().logCount == log);
}

namespace
{
    // The player acts first in this night; seed search.
    NightState PlayerFirst(const NightDef &def)
    {
        for (std::uint64_t seed = 1; seed < 200; ++seed)
            if (NightState state = FoyerNight(def, seed, 0); Current(state) == 0)
                return state;
        FAIL("no seed with the player first");
        return {};
    }
}

TEST_CASE("Every chance shown matches the rules (T113)", "[night][play]")
{
    NightDef def = Data();
    SECTION("chat answers")
    {
        Play play(def, FoyerNight(def, 4, 0));
        for (int i = 0; i < 400 && !(play.State().chat.active && play.PlayerTurn()); ++i)
            play.PlayerTurn() ? play.Key(InputKey::Enter) : play.Update(10.0F);
        REQUIRE(play.State().chat.active);
        REQUIRE(play.Screen() == "chat");
        const nlohmann::json chat = play.UiData()["chat"];
        REQUIRE(chat["answers"].size() == 2);
        for (int a = 0; a < 2; ++a)
        {
            const int target = AnswerTarget(def, play.State(), a);
            const int stat = Stat(def, play.State(), 0, a == 0 ? StatKind::Might : StatKind::Sanity);
            CHECK(chat["answers"][a]["target"] == target);
            CHECK(chat["answers"][a]["statValue"] == stat);
            CHECK(chat["answers"][a]["chance"] == ChancePercent(stat, target));
        }
    }
    SECTION("an event")
    {
        for (RoomDef &room : def.rooms)
            if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
                room.kind = RoomKind::Event, room.doors = 15;
        Play play(def, PlayerFirst(def));
        const int step = StepCell(def, play);
        REQUIRE(step >= 0);
        GoTo(play, step);
        REQUIRE(play.HandleUi("arrive", "onward", "")); // the arrival first (T152)
        REQUIRE(play.Screen() == "event");
        const nlohmann::json event = play.UiData()["event"];
        CHECK(event["chance"] == ChancePercent(event["statValue"], event["target"]));
        CHECK_FALSE(event["text"].get<std::string>().empty());
        play.Update(10.0F); // bots wait while the event shows
        CHECK(play.Beats() == 0);
    }
}

TEST_CASE("The dice screen shows the logged roll (T113)", "[night][play]")
{
    NightDef def = Data();
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    Play play(def, PlayerFirst(def));
    const std::uint32_t since = play.State().logCount;
    GoTo(play, StepCell(def, play));
    REQUIRE(play.HandleUi("event", "roll", ""));
    REQUIRE(play.HandleUi("arrive", "onward", "")); // the arrival first (T152)
    REQUIRE(play.Screen() == "dice");
    LogEntry check{};
    for (const LogEntry &e : NewLog(play.State(), since))
        if (e.kind == LogKind::Check)
            check = e;
    REQUIRE(check.kind == LogKind::Check);
    const nlohmann::json dice = play.UiData()["dice"];
    CHECK(dice["die"] == check.a);
    CHECK(dice["statValue"] == check.other);
    CHECK(dice["target"] == check.b);
    CHECK(dice["total"] == check.a + check.other);
    CHECK(dice["passed"] == (check.a + check.other >= check.b));
    CHECK_FALSE(dice["result"].get<std::string>().empty());
    REQUIRE(play.HandleUi("dice", "continue", ""));
    CHECK(play.Screen() != "dice"); // an event room stops the guest: the turn may have passed
    CHECK(play.Screen() != "event");
}

TEST_CASE("No screen waits after the night ended (T113)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = PlayerFirst(def);
    state.chat = {true, 1, 0, 5};
    state.over = true;
    state.outcome = Outcome::Company;
    Play play(def, state);
    CHECK(play.Screen() == "end"); // the end screen only, no chat
    CHECK_FALSE(play.UiData().contains("chat"));
    CHECK_FALSE(play.HandleUi("chat", "answer", "0"));
    play.Update(10.0F);
    CHECK(play.State().logCount == state.logCount);
}

namespace
{
    // The player's first turn, with a monster in the foyer, attacked: a battle the player opens.
    Play FightMonster(const NightDef &def, NightState state, int monsterHp, int monsterMight)
    {
        state.monsters.push_back({0, state.foyer, 20, static_cast<std::int16_t>(monsterHp), static_cast<std::int16_t>(monsterMight), 1});
        Play play(def, state);
        const std::vector<Action> legal = LegalActions(def, play.State());
        for (std::size_t i = 0; i < legal.size(); ++i)
            if (legal[i].kind == ActionKind::Attack && legal[i].a == 0)
                REQUIRE(play.HandleUi("menu", "act", std::to_string(i)));
        REQUIRE(play.State().battle.active);
        return play;
    }
}

TEST_CASE("The battle data lists both sides and the expected damage (T114)", "[night][play]")
{
    const NightDef def = Data();
    Play play = FightMonster(def, PlayerFirst(def), 99, 1);
    REQUIRE(play.Screen() == "battle");
    const nlohmann::json battle = play.UiData()["battle"];
    const Battle &b = play.State().battle;
    REQUIRE(battle["company"].size() == 1);
    CHECK(battle["company"][0]["name"] == def.guests[0].name);
    CHECK(battle["company"][0]["acting"] == true);
    REQUIRE(battle["enemies"].size() == 1);
    const int enemy = battle["enemies"][0]["index"];
    CHECK(battle["enemies"][0]["name"] == def.monsters[0].name);
    CHECK(battle["enemies"][0]["hp"] == 99);
    CHECK(battle["enemies"][0]["strike"] == Damage(def, play.State(), b.fighters[static_cast<std::size_t>(b.turn)], b.fighters[static_cast<std::size_t>(enemy)]));
    CHECK(battle["round"] == 1);
    CHECK(battle["rounds"] == def.rules.battle.rounds);
    const std::uint32_t since = play.State().logCount;
    REQUIRE(play.HandleUi("battle", "strike", std::to_string(enemy)));
    bool hit = false;
    for (const LogEntry &e : NewLog(play.State(), since))
        hit = hit || (e.kind == LogKind::Hit && e.who == 0);
    CHECK(hit);
    CHECK_FALSE(play.UiData()["battle"]["last"].get<std::string>().empty());
}

TEST_CASE("Leave is offered from round 2 only (T114)", "[night][play]")
{
    const NightDef def = Data();
    Play play = FightMonster(def, PlayerFirst(def), 99, 1);
    CHECK(play.UiData()["battle"]["canLeave"] == false);
    CHECK_FALSE(play.HandleUi("battle", "leave", ""));
    REQUIRE(play.HandleUi("battle", "guard", "")); // the monster acts, round 2 begins
    REQUIRE(play.State().battle.active);
    REQUIRE(play.State().battle.round == 2);
    CHECK(play.UiData()["battle"]["canLeave"] == true);
    CHECK(play.HandleUi("battle", "leave", ""));
    CHECK_FALSE(play.State().battle.active);
}

TEST_CASE("The player's death mid-battle ends the screen and the watching begins (T114)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = PlayerFirst(def);
    // After midnight 0 HP is death; the Taken waits far away, shut in a closet.
    const int far = CellAt(def, 0, 0);
    Closet(def, state, far);
    state.taken = {true, 6, 0, false};
    state.marked = 6;
    state.guests[6].cell = static_cast<std::int8_t>(far);
    state.guests[0].hp = 1;
    Play play = FightMonster(def, state, 99, 30);
    const int enemy = play.UiData()["battle"]["enemies"][0]["index"];
    REQUIRE(play.HandleUi("battle", "strike", std::to_string(enemy)));
    REQUIRE(play.State().guests[0].dead);
    CHECK_FALSE(play.PlayerTurn());
    CHECK(play.Screen() != "battle");
    CHECK_FALSE(play.UiData().contains("battle"));
    const int beats = play.Beats();
    play.Update(10.0F);
    play.Update(10.0F);
    CHECK((play.Beats() > beats || play.State().over));
}

namespace
{
    // Plays (bots, and the player ending turns) until `done`; event, dice and Turn screens are dismissed.
    template <class Done> void PlayUntil(Play &play, Done done)
    {
        for (int i = 0; i < 3000 && !done() && !play.State().over; ++i)
        {
            Settle(play);
            if (play.Screen() == "turn")
                play.HandleUi("turn", "begin", "");
            else if (play.State().chat.active && play.PlayerTurn())
                play.HandleUi("chat", "answer", "0");
            else if (play.PlayerTurn() && play.State().battle.active)
                play.HandleUi("battle", "guard", "");
            else if (play.PlayerTurn())
            {
                const nlohmann::json actions = play.UiData()["actions"];
                if (!actions.empty() && actions.back()["label"] == "End turn")
                    play.Key(InputKey::Enter);
                else if (!actions.empty())
                    play.HandleUi("menu", actions.front()["action"], actions.front()["arg"]); // a level up, say
            }
            else
                play.Update(10.0F);
        }
    }
}

TEST_CASE("A save and Continue give the same night (T115)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 11, 2));
    PlayUntil(play, [&] { return play.State().round >= 4; });
    const nlohmann::json saved = play.Save();
    NightState restored;
    REQUIRE(Play::Restore(def, saved, restored));
    CHECK(Fingerprint(restored) == Fingerprint(play.State()));
    CHECK(Play::StartData(def, true)["start"]["canContinue"] == true);
    Play again(def, restored);
    PlayUntil(play, [&] { return play.State().round >= 7; });
    PlayUntil(again, [&] { return again.State().round >= 7; });
    CHECK(Fingerprint(again.State()) == Fingerprint(play.State()));
}

TEST_CASE("A save from other data hides Continue (T115)", "[night][play]")
{
    const NightDef def = Data();
    const nlohmann::json saved = Play(def, FoyerNight(def, 11, 2)).Save();
    NightDef other = def;
    other.rules.battle.rounds += 1;
    other.fingerprint ^= 1; // other data
    NightState state;
    CHECK_FALSE(Play::Restore(other, saved, state));
    CHECK_FALSE(Play::Restore(def, nlohmann::json::parse("{\"broken\": true}"), state));
    CHECK_FALSE(Play::Restore(def, nlohmann::json(), state));
    const nlohmann::json start = Play::StartData(def, false)["start"];
    CHECK(start["canContinue"] == false);
    REQUIRE(start["guests"].size() == def.guests.size());
    CHECK(start["guests"][0]["id"] == def.guests[0].id);
    CHECK(start["guests"][0]["hp"] == def.guests[0].hp);
}

TEST_CASE("The Turn screen names the form and its numbers (T115)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 11, 0);
    state.marked = 3;
    state.round = static_cast<std::int8_t>(def.rules.clock.turnAfterRound);
    Play play(def, state);
    for (int i = 0; i < 3000 && !play.State().taken.present; ++i)
    {
        Settle(play);
        if (play.State().chat.active && play.PlayerTurn())
            play.HandleUi("chat", "answer", "0");
        else if (play.PlayerTurn())
            play.Key(InputKey::Enter);
        else
            play.Update(10.0F);
    }
    REQUIRE(play.State().taken.present);
    REQUIRE(play.Screen() == "turn");
    const nlohmann::json turn = play.UiData()["turn"];
    CHECK(turn["who"] == def.guests[3].name);
    CHECK(turn["form"] == def.guests[3].taken);
    CHECK(turn["hp"] == def.guests[3].takenHp);
    int minions = 0;
    for (const MonsterDef &m : def.monsters)
        minions += m.taken == 3 ? m.count : 0;
    CHECK(turn["minions"] == minions);
    CHECK_FALSE(turn["power"].get<std::string>().empty());
    const int beats = play.Beats();
    play.Update(10.0F); // the bots wait for the player to read it
    CHECK(play.Beats() == beats);
    CHECK(play.HandleUi("turn", "begin", ""));
    CHECK(play.Screen() != "turn");
}

TEST_CASE("The player as the Taken plays on (T115)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 11, 0);
    state.marked = 0;
    state.round = static_cast<std::int8_t>(def.rules.clock.turnAfterRound);
    Play play(def, state);
    PlayUntil(play, [&] { return play.State().taken.present; });
    REQUIRE(play.State().taken.present);
    REQUIRE(play.State().taken.guest == 0);
    CHECK(play.UiData()["turn"]["isYou"] == true);
    play.HandleUi("turn", "begin", "");
    for (int i = 0; i < 400 && Current(play.State()) != 0 && !play.State().over; ++i)
        if (Settle(play); !play.PlayerTurn())
            play.Update(10.0F);
    REQUIRE(Current(play.State()) == 0);
    CHECK(play.PlayerTurn());
    const BoardView view = MakeView(def, play.State(), 0);
    CHECK(view.cells[static_cast<std::size_t>(play.State().guests[0].cell)].lit);
    CHECK(play.UiData()["me"]["name"] == def.guests[0].name);
}

TEST_CASE("Clicks and keys do nothing while a dialog shows (final review)", "[night][play]")
{
    // The Turn screen shows while the player, first of the company, could already move.
    const NightDef def = Data();
    bool tested = false;
    for (std::uint64_t seed = 1; seed < 40 && !tested; ++seed)
    {
        NightState state = FoyerNight(def, seed, 0);
        state.marked = 3;
        state.round = static_cast<std::int8_t>(def.rules.clock.turnAfterRound);
        Play play(def, state);
        for (int i = 0; i < 3000 && !play.State().taken.present; ++i)
        {
            Settle(play);
            if (play.State().chat.active && play.PlayerTurn())
                play.HandleUi("chat", "answer", "0");
            else if (play.PlayerTurn())
                play.Key(InputKey::Enter);
            else
                play.Update(10.0F);
        }
        if (play.Screen() != "turn" || !play.PlayerTurn() || StepCell(def, play) < 0)
            continue;
        tested = true;
        const int cell = play.State().guests[0].cell;
        GoTo(play, StepCell(def, play));
        for (const InputKey key : {InputKey::Up, InputKey::Right, InputKey::Down, InputKey::Left, InputKey::Enter, InputKey::I})
            play.Key(key);
        CHECK(play.State().guests[0].cell == cell);
        CHECK(play.Screen() == "turn");
    }
    CHECK(tested);
}

TEST_CASE("A push-back event names the room it happened in (final review)", "[night][play]")
{
    NightDef def = Data();
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    for (EventDef &event : def.events) // every event pushes back, pass or fail
        event.passEffect = event.failEffect = Effect::Back;
    Play play(def, PlayerFirst(def));
    const int step = StepCell(def, play);
    GoTo(play, step);
    REQUIRE(play.HandleUi("arrive", "onward", "")); // the arrival first (T152)
    REQUIRE(play.Screen() == "event");
    CHECK(play.State().guests[0].cell == play.State().foyer); // pushed back
    CHECK(play.UiData()["event"]["room"] == def.rooms[static_cast<std::size_t>(play.State().cells[static_cast<std::size_t>(step)].room)].name);
}

TEST_CASE("The HUD data carries the player's goal (T122)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 4, 0);
    state.guests[0].knows = static_cast<std::uint8_t>(1u << 2); // the player has learnt guest 2's goal
    Play play(def, state);
    const nlohmann::json data = play.UiData();
    REQUIRE(data.contains("goal"));
    CHECK(data["goal"]["text"] == def.goals[static_cast<std::size_t>(state.guests[0].goal)].text);
    CHECK(data["goal"]["done"] == false);
    REQUIRE(data["knownGoals"].size() == 1);
    CHECK(data["knownGoals"][0]["name"] == def.guests[2].name);
    CHECK(data["knownGoals"][0]["text"] == def.goals[static_cast<std::size_t>(state.guests[2].goal)].text);
}

namespace
{
    // The cell `n` steps west of the foyer, on its row.
    int West(const NightDef &def, int n) { return CellAt(def, def.rules.board.width / 2 - n, def.rules.board.height - 1); }

    // The player's turn in the foyer with `rooms` plain rooms in a row to the west (doors E and W),
    // and `moves` moves left; every bot elsewhere so no chat or fight gets in the way.
    NightState PlayerCorridor(const NightDef &def, int rooms, int moves)
    {
        NightState state = FoyerNight(def, 4, 0);
        while (Current(state) != 0)
            REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
        const int room = NightDef::Find(def.rooms, "hallway");
        for (int n = 1; n <= rooms; ++n)
            state.cells[static_cast<std::size_t>(West(def, n))] = {static_cast<std::int8_t>(room), 2 | 8, true, true};
        state.cells[static_cast<std::size_t>(state.foyer)].doors |= 8;
        Closet(def, state, 0);
        for (int g = 1; g < state.count; ++g)
            state.guests[static_cast<std::size_t>(g)].cell = 0;
        state.guests[0].movesLeft = state.guests[0].budget = static_cast<std::int8_t>(moves), state.guests[0].origin = state.guests[0].cell;
        return state;
    }

    // Updates in slide-sized steps until the walk is over (or `limit` steps); the cells after each.
    std::vector<int> Walk(Play &play, int limit = 20)
    {
        std::vector<int> cells;
        for (int i = 0; i < limit && (play.Sliding().guest >= 0 || play.Screen() == "picking"); ++i)
        {
            play.Update(Play::slideSeconds);
            cells.push_back(play.State().guests[0].cell);
        }
        return cells;
    }
}

TEST_CASE("A click walks a lit square three steps away, one sliding step at a time (T125)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 3, 4));
    REQUIRE(play.Screen() == "menu");
    play.Click(West(def, 3));
    std::vector<int> seen;
    bool slid = false;
    for (int i = 0; i < 20 && (play.Sliding().guest >= 0 || play.Screen() == "picking"); ++i)
    {
        play.Update(Play::slideSeconds);
        slid = slid || play.Sliding().guest == 0;
        if (seen.empty() || seen.back() != play.State().guests[0].cell)
            seen.push_back(play.State().guests[0].cell);
    }
    CHECK(seen == std::vector<int>{West(def, 1), West(def, 2), West(def, 3)});
    CHECK(slid);
    CHECK(play.State().guests[0].movesLeft == 1);
    CHECK(play.TurnMode() == Play::Mode::Menu);
    CHECK(play.Screen() == "menu");
}

TEST_CASE("A square beyond the moves left is ignored; its path shows red past the last affordable step (T125)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 3, 2));
    REQUIRE(play.HandleUi("menu", "move", ""));
    CHECK(play.TurnMode() == Play::Mode::Picking);
    play.Hover(West(def, 3));
    CHECK(play.Previewed().path.size() == 3);
    CHECK(play.Previewed().affordable == 2);
    const NightState before = play.State();
    play.Click(West(def, 3));
    play.Update(Play::slideSeconds);
    CHECK(play.State().guests[0].cell == before.guests[0].cell);
    CHECK(play.State().guests[0].movesLeft == 2);
}

TEST_CASE("Illegal squares are ignored in the menu and in picking (T125)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 2, 4));
    for (int pass = 0; pass < 2; ++pass)
    {
        if (pass == 1)
            REQUIRE(play.HandleUi("menu", "move", ""));
        for (const int cell : {0, static_cast<int>(play.State().foyer), -1, maxCells})
        {
            play.Click(cell);
            play.Update(Play::slideSeconds);
            CHECK(play.State().guests[0].cell == play.State().foyer);
            CHECK(play.Sliding().guest < 0);
        }
    }
}

TEST_CASE("The walk stops at a new room's door with Stay/Back (T125)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 2, 4));
    const int door = West(def, 3);
    REQUIRE_FALSE(play.State().cells[static_cast<std::size_t>(door)].opened);
    play.Click(door);
    Walk(play);
    CHECK(play.Screen() == "confirm");
    CHECK(play.State().guests[0].cell == West(def, 2));
    REQUIRE(play.HandleUi("confirm", "back", ""));
    CHECK(play.TurnMode() == Play::Mode::Picking);
    CHECK_FALSE(play.State().cells[static_cast<std::size_t>(door)].opened);
    CHECK(play.State().guests[0].movesLeft == 2);
    play.Click(door);
    Walk(play);
    REQUIRE(play.Screen() == "confirm");
    const std::uint32_t logBefore = play.State().logCount;
    REQUIRE(play.HandleUi("confirm", "stay", ""));
    CHECK(play.State().cells[static_cast<std::size_t>(door)].opened);
    bool opened = false;
    for (const LogEntry &e : NewLog(play.State(), logBefore))
        opened = opened || e.kind == LogKind::Opened;
    CHECK(opened);
    for (int i = 0; i < 10 && play.Sliding().guest >= 0; ++i)
        play.Update(Play::slideSeconds);
    CHECK(play.TurnMode() == Play::Mode::Menu);
}

TEST_CASE("A dialog part-way along a walk drops the rest of the walk (T125)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = PlayerCorridor(def, 3, 4);
    state.cells[static_cast<std::size_t>(West(def, 1))].room = static_cast<std::int8_t>(NightDef::Find(def.rooms, "lighting")); // an event room
    state.cells[static_cast<std::size_t>(West(def, 1))].resolved = false;
    Play play(def, state);
    play.Click(West(def, 3));
    Walk(play);
    REQUIRE(play.HandleUi("arrive", "onward", "")); // the arrival first (T152)
    CHECK(play.Screen() == "event");
    play.HandleUi("event", "roll", "");
    while (play.Screen() == "dice")
        play.HandleUi("dice", "continue", "");
    play.Update(Play::slideSeconds);
    CHECK(play.TurnMode() == Play::Mode::Menu);
    CHECK(play.State().guests[0].cell != West(def, 2));
    CHECK(play.State().guests[0].cell != West(def, 3));
}

TEST_CASE("dt 0 changes nothing mid-walk (T125)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 3, 4));
    play.Click(West(def, 3));
    play.Update(Play::slideSeconds); // the first step starts sliding
    const int cell = play.State().guests[0].cell;
    const float t = play.Sliding().t;
    play.Update(0.0F);
    CHECK(play.State().guests[0].cell == cell);
    CHECK(play.Sliding().t == t);
    CHECK(play.TurnMode() == Play::Mode::Picking);
    Walk(play);
    CHECK(play.State().guests[0].cell == West(def, 3));
}

TEST_CASE("Continue mid-walk opens the menu with the moves that were left (T125)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 3, 4));
    play.Click(West(def, 3));
    play.Update(Play::slideSeconds);
    NightState restored;
    REQUIRE(Play::Restore(def, play.Save(), restored));
    Play again(def, restored);
    CHECK(again.Screen() == "menu");
    CHECK(again.State().guests[0].movesLeft == 3);
    CHECK(again.State().guests[0].cell == West(def, 1));
}

TEST_CASE("Input: menu, Esc opens the save menu (T126)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 2, 4));
    play.Key(InputKey::Escape);
    CHECK(play.Screen() == "pause");
    play.Key(InputKey::Escape); // Esc in the save menu is Resume
    CHECK(play.Screen() == "menu");
    play.Key(InputKey::Escape);
    REQUIRE(play.HandleUi("pause", "resume", ""));
    CHECK(play.Screen() == "menu");
}

TEST_CASE("Input: menu, an arrow steps that way in pick mode (T126)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 2, 4));
    play.Key(InputKey::Left);
    CHECK(play.TurnMode() == Play::Mode::Picking);
    Walk(play);
    CHECK(play.State().guests[0].cell == West(def, 1));
}

TEST_CASE("Input: menu, Enter ends the turn (T126)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 2, 4));
    play.Key(InputKey::Enter);
    CHECK_FALSE(play.PlayerTurn());
}

TEST_CASE("Input: menu and picking, a right click inspects (T126)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 2, 4));
    play.Click(West(def, 1), true);
    CHECK(play.Screen() == "inspect");
    play.Key(InputKey::Escape);
    CHECK(play.Screen() == "menu");
    REQUIRE(play.HandleUi("menu", "move", ""));
    play.Click(West(def, 1), true);
    CHECK(play.Screen() == "inspect");
    REQUIRE(play.HandleUi("inspect", "close", ""));
    CHECK(play.Screen() == "picking");
}

TEST_CASE("Input: picking, Esc goes back to the menu and keeps the steps taken (T126)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 3, 4));
    play.Click(West(def, 3));
    play.Update(Play::slideSeconds); // one step under way
    play.Key(InputKey::Escape);
    Walk(play);
    CHECK(play.TurnMode() == Play::Mode::Menu);
    CHECK(play.State().guests[0].cell == West(def, 1));
    CHECK(play.State().guests[0].movesLeft == 3);
}

TEST_CASE("Input: picking, Enter walks to the hovered square (T126)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 2, 4));
    REQUIRE(play.HandleUi("menu", "move", ""));
    play.Hover(West(def, 2));
    play.Key(InputKey::Enter);
    Walk(play);
    CHECK(play.State().guests[0].cell == West(def, 2));
}

TEST_CASE("Input: confirm, Enter is Stay and Esc is Back; arrows and clicks do nothing (T126)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 1, 4));
    play.Click(West(def, 2));
    Walk(play);
    REQUIRE(play.Screen() == "confirm");
    play.Key(InputKey::Up);
    play.Click(play.State().foyer);
    play.Click(play.State().foyer, true);
    CHECK(play.Screen() == "confirm");
    play.Key(InputKey::Escape);
    CHECK(play.TurnMode() == Play::Mode::Picking);
    CHECK_FALSE(play.State().cells[static_cast<std::size_t>(West(def, 2))].opened);
    play.Click(West(def, 2));
    Walk(play);
    REQUIRE(play.Screen() == "confirm");
    play.Key(InputKey::Enter);
    CHECK(play.State().cells[static_cast<std::size_t>(West(def, 2))].opened);
}

TEST_CASE("Input: a dialog ignores clicks on the board; Esc closes it where it can (T126)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 2, 4));
    REQUIRE(play.HandleUi("menu", "bag", ""));
    REQUIRE(play.Screen() == "bag");
    play.Click(West(def, 1));
    play.Click(West(def, 1), true);
    play.Update(Play::slideSeconds);
    CHECK(play.Screen() == "bag");
    CHECK(play.State().guests[0].cell == play.State().foyer);
    play.Key(InputKey::Escape);
    CHECK(play.Screen() == "menu");
}

TEST_CASE("Input: a bot's turn, Esc opens the save menu and a left click inspects (T126)", "[night][play]")
{
    const NightDef def = Data();
    Play play = BotFirst(def);
    REQUIRE_FALSE(play.PlayerTurn());
    play.Key(InputKey::Escape);
    CHECK(play.Screen() == "pause");
    const int beats = play.Beats();
    play.Update(10.0F);
    CHECK(play.Beats() == beats); // the night waits behind the save menu
    REQUIRE(play.HandleUi("pause", "resume", ""));
    play.Click(play.State().guests[0].cell);
    CHECK(play.Screen() == "inspect");
}

TEST_CASE("Inspect shows only what the player may know (T126)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = PlayerCorridor(def, 2, 4);
    // A bot in the lit room west of the foyer, carrying a bandage; cell 0 a dark closet.
    state.guests[1].cell = static_cast<std::int8_t>(West(def, 1));
    state.guests[1].items[0] = static_cast<std::int8_t>(NightDef::Find(def.items, "bandage"));
    Play play(def, state);
    // 1. Nothing for an unopened cell or outside the board.
    for (const int cell : {West(def, 4), -1, maxCells})
    {
        play.Click(cell, true);
        CHECK(play.Screen() == "menu");
    }
    // 2. A dark room: its name and kind, nobody in it.
    play.Click(0, true);
    REQUIRE(play.Screen() == "inspect");
    nlohmann::json data = play.UiData()["inspect"];
    CHECK(data["room"]["dark"] == true);
    CHECK(data["room"]["name"] == def.rooms[static_cast<std::size_t>(state.cells[0].room)].name);
    CHECK(data["guests"].empty());
    play.HandleUi("inspect", "close", "");
    // 3. A lit room: the bot's numbers and bag; its goal only once learnt.
    play.Click(West(def, 1), true);
    data = play.UiData()["inspect"];
    REQUIRE(data["guests"].size() == 1);
    const nlohmann::json bot = data["guests"][0];
    CHECK(bot["name"] == def.guests[1].name);
    CHECK(bot["hp"] == state.guests[1].hp);
    CHECK(bot["bag"].size() == 1);
    CHECK(bot["goal"] == "");
    play.HandleUi("inspect", "close", "");
    NightState knows = state;
    knows.guests[0].knows = 1u << 1;
    Play learnt(def, knows);
    learnt.Click(West(def, 1), true);
    CHECK(learnt.UiData()["inspect"]["guests"][0]["goal"] == def.goals[static_cast<std::size_t>(state.guests[1].goal)].text);
    // 4. The player's own goal always shows.
    learnt.HandleUi("inspect", "close", "");
    learnt.Click(state.foyer, true);
    CHECK(learnt.UiData()["inspect"]["guests"][0]["goal"] == def.goals[static_cast<std::size_t>(state.guests[0].goal)].text);
}

namespace
{
    // A decider that plays the bots' choice but says it took `seconds`.
    Play::Decider Slow(const NightDef &def, float seconds)
    {
        return [&def, seconds](const NightState &state, int guest, float &took) {
            took = seconds;
            return ChooseAction(def, state, guest);
        };
    }

    // A night where a bot the player sees acts first, with captions.
    Play SeenBotFirst(const NightDef &def)
    {
        NightState state = PlayerCorridor(def, 1, 4);
        Apply(def, state, {ActionKind::EndTurn});
        const int bot = Current(state);
        REQUIRE(bot != 0);
        state.guests[static_cast<std::size_t>(bot)].cell = state.foyer; // in the player's light
        Play play(def, state);
        std::vector<umbra::core::Diagnostic> diagnostics;
        Captions captions;
        REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
        play.SetCaptions(captions);
        return play;
    }
}

TEST_CASE("A bot in view thinks max(0.4 s, its decision time) before its first action (T127)", "[night][play]")
{
    const NightDef def = Data();
    for (const float took : {0.1F, 0.9F})
    {
        Play play = SeenBotFirst(def);
        play.SetDecider(Slow(def, took));
        const int bot = Current(play.State());
        play.Update(0.01F); // decides
        const float pause = std::max(0.4F, took);
        CHECK(std::abs(play.Thinking() - pause) < 1e-4F);
        CHECK(play.ThinkingGuest() == bot);
        CHECK(play.Beats() == 0);
        play.Update(pause - 0.05F);
        CHECK(play.Beats() == 0);
        play.Update(0.06F);
        CHECK(play.Beats() == 1);
        CHECK(play.Thinking() == 0.0F);
        // The second action of the same turn: no pause.
        if (Current(play.State()) == bot)
        {
            for (int i = 0; i < 100 && play.Beats() == 1; ++i)
                play.Update(0.1F);
            CHECK(play.Thinking() == 0.0F);
        }
    }
}

TEST_CASE("A bot the player cannot see does not think on screen (T127)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = PlayerCorridor(def, 1, 4);
    Apply(def, state, {ActionKind::EndTurn}); // the bots stand in the dark closet
    Play play(def, state);
    play.SetDecider(Slow(def, 0.9F));
    play.Update(0.01F);
    CHECK(play.Thinking() == 0.0F);
    CHECK(play.Beats() == 1);
}

TEST_CASE("A bot's step slides (T127)", "[night][play]")
{
    const NightDef def = Data();
    Play play = SeenBotFirst(def);
    const int bot = Current(play.State());
    bool slid = false;
    for (int i = 0; i < 200 && !slid && Current(play.State()) == bot; ++i)
    {
        const int before = play.State().guests[static_cast<std::size_t>(bot)].cell;
        play.Update(0.05F);
        if (play.State().guests[static_cast<std::size_t>(bot)].cell != before && play.Sliding().guest == bot)
            slid = play.Sliding().from == before;
    }
    CHECK(slid);
}

TEST_CASE("The camera follows each step of a walk (T128)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 3, 4));
    CHECK(play.CameraCell() == play.State().foyer);
    play.Click(West(def, 3));
    std::vector<int> looked;
    for (int i = 0; i < 20 && (play.Sliding().guest >= 0 || play.Screen() == "picking"); ++i)
    {
        play.Update(Play::slideSeconds);
        if (looked.empty() || looked.back() != play.CameraCell())
            looked.push_back(play.CameraCell());
    }
    CHECK(looked == std::vector<int>{West(def, 1), West(def, 2), West(def, 3)});
}

TEST_CASE("A drag stops following until the portrait is clicked (T128, T163)", "[night][play]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 2, 4));
    CHECK(play.Following());
    play.Drag();
    CHECK_FALSE(play.Following());
    REQUIRE(play.HandleUi("hud", "focus", ""));
    CHECK(play.Following());
}

TEST_CASE("A dead player still follows the beats; Focus and Esc are safe (T128)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = PlayerCorridor(def, 1, 4);
    Apply(def, state, {ActionKind::EndTurn});
    state.guests[0].dead = true;
    for (int g = 1; g < state.count; ++g)
        state.guests[static_cast<std::size_t>(g)].cell = state.foyer;
    Play play(def, state);
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
    play.SetCaptions(captions);
    for (int i = 0; i < 50 && play.Beats() == 0; ++i)
        play.Update(0.2F);
    REQUIRE(play.Beats() > 0);
    if (play.LastBeat().seen)
        CHECK(play.CameraCell() == play.LastBeat().camera);
    play.Drag();
    CHECK(play.HandleUi("hud", "focus", ""));
    play.Key(InputKey::Escape);
    CHECK(play.Screen() == "pause");
}

TEST_CASE("Rings are numbered by group, the player's first (T129)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = PlayerCorridor(def, 1, 4);
    state.guests[3].group = state.guests[5].group = 4; // a bot group, lowest member 3
    state.guests[6].group = 7;                         // a group of one: no ring
    Play alone(def, state);
    CHECK(alone.Rings() == std::vector<std::pair<int, int>>{{3, 2}, {5, 2}});
    state.guests[0].group = state.guests[2].group = 0; // the player's group
    Play play(def, state);
    CHECK(play.Rings() == std::vector<std::pair<int, int>>{{0, 1}, {2, 1}, {3, 2}, {5, 2}});
}

TEST_CASE("The HUD shows a goal's progress (T129)", "[night][play]")
{
    NightDef def = Data();
    NightState state = PlayerCorridor(def, 1, 4);
    def.goals.push_back({0, "test", GoalKind::Events, -1, 2, "Pass two tests."});
    state.guests[0].goal = static_cast<std::int8_t>(def.goals.size() - 1);
    state.guests[0].goalCount = 1;
    Play play(def, state);
    CHECK(play.UiData()["goal"]["progress"] == "1 of 2");
    def.goals.back().kind = GoalKind::Room;
    def.goals.back().target = 0;
    Play room(def, state);
    CHECK(room.UiData()["goal"]["progress"] == "");
}

TEST_CASE("An arrow with no move left does nothing, not even the door's question (final review)", "[night][play]")
{
    const NightDef def = Data();
    NightState state = PlayerCorridor(def, 0, 0); // the foyer's west door leads to an unopened cell
    Play play(def, state);
    play.Key(InputKey::Left);
    play.Update(Play::slideSeconds);
    CHECK(play.Screen() == "menu");
}

namespace
{
    // A stand-in model: `answer(prompt)`, or never (until the test ends) when `never` is set.
    struct StandIn final : umbra::town::Mind
    {
        std::function<std::string(const umbra::town::MindRequest &)> answer;
        std::atomic<bool> never{false};
        std::vector<std::uint32_t> *seeds = nullptr;
        std::string Name() const override { return "stand-in"; }
        std::string Complete(const umbra::town::MindRequest &request) override
        {
            if (seeds)
                seeds->push_back(request.seed);
            while (never)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return answer(request);
        }
    };

    // The template line out of a request's prompt (after the last "Line: ").
    std::string LineOf(const umbra::town::MindRequest &request)
    {
        const std::size_t at = request.prompt.rfind("Line: ") + 6;
        return request.prompt.substr(at, request.prompt.find('\n', at) - at);
    }

    // Small steps of real time until `done` (the stand-in answers on its own thread).
    template <typename Done> void Until(Play &play, Done done)
    {
        for (int i = 0; i < 3000 && !done(); ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            play.Update(0.001F);
        }
    }
}

TEST_CASE("A seen bot's caption waits up to 3 s for its line, then shows the template (T135)", "[night][play][words]")
{
    const NightDef def = Data();
    auto mind = std::make_unique<StandIn>();
    mind->never = true;
    mind->answer = [](const umbra::town::MindRequest &) { return std::string{}; };
    StandIn &standIn = *mind;
    Words words(def, std::move(mind));
    Play play = SeenBotFirst(def);
    play.SetWords(&words);
    play.Update(0.01F);  // decides and asks
    play.Update(0.5F);   // thought; the line is not in
    CHECK(play.Beats() == 0);
    CHECK(play.ThinkingGuest() >= 0);
    play.Update(2.4F);   // 2.91 s since asked
    CHECK(play.Beats() == 0);
    play.Update(0.15F);  // past 3 s: the template
    CHECK(play.Beats() == 1);
    CHECK(play.Screen() == "caption");
    CHECK(play.UiData()["caption"]["line"] == play.LastBeat().caption);
    standIn.never = false;
}

TEST_CASE("A seen bot's caption is the model's line; the result stays the template (T135)", "[night][play][words]")
{
    const NightDef def = Data();
    auto mind = std::make_unique<StandIn>();
    mind->answer = [](const umbra::town::MindRequest &request) { return "Quietly, " + LineOf(request); };
    Words words(def, std::move(mind));
    Play reference = SeenBotFirst(def); // the same beat without the model
    Think(reference);
    REQUIRE(reference.Beats() == 1);
    Play play = SeenBotFirst(def);
    play.SetWords(&words);
    play.Update(0.01F);
    play.Update(Play::thinkSeconds + 0.01F);
    Until(play, [&] { return play.Beats() == 1; });
    REQUIRE(play.Beats() == 1);
    const nlohmann::json caption = play.UiData()["caption"];
    CHECK(caption["line"] == "Quietly, " + reference.LastBeat().caption);
    CHECK(caption["result"] == reference.LastBeat().result);
}

TEST_CASE("The night plays the same with the model as without it (T135)", "[night][play][words]")
{
    const NightDef def = Data();
    auto mind = std::make_unique<StandIn>();
    mind->answer = [](const umbra::town::MindRequest &request) { return "Quietly, " + LineOf(request); };
    Words words(def, std::move(mind));
    const auto run = [&](Words *w) {
        Play play(def, FoyerNight(def, 9, 0));
        play.SetWords(w);
        for (int i = 0; i < 6000 && play.Beats() < 40 && !play.State().over; ++i)
        {
            Settle(play);
            if (play.State().chat.active && play.PlayerTurn() && play.Screen() == "chat")
                play.HandleUi("chat", "answer", "0");
            else if (play.PlayerTurn() && play.Screen() == "menu")
                play.Key(InputKey::Enter);
            else
                play.Update(10.0F);
        }
        return Fingerprint(play.State());
    };
    CHECK(run(&words) == run(nullptr));
}

TEST_CASE("A chat question waits for its line and shows it; the answers stay (T135)", "[night][play][words]")
{
    const NightDef def = Data();
    auto mind = std::make_unique<StandIn>();
    mind->answer = [](const umbra::town::MindRequest &request) {
        return request.prompt.find("never hint") != std::string::npos ? "Tell me truly, " + LineOf(request) : std::string{};
    };
    Words words(def, std::move(mind));
    Play play(def, FoyerNight(def, 4, 0));
    play.SetWords(&words);
    for (int i = 0; i < 400 && !(play.State().chat.active && play.PlayerTurn()); ++i)
        play.PlayerTurn() ? play.Key(InputKey::Enter) : play.Update(10.0F);
    REQUIRE(play.State().chat.active);
    const QuestionDef &q = def.questions[static_cast<std::size_t>(play.State().chat.question)];
    Until(play, [&] { return play.Screen() == "chat"; });
    REQUIRE(play.Screen() == "chat");
    const nlohmann::json chat = play.UiData()["chat"];
    std::string lower = q.text;
    lower[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[0])));
    CHECK((chat["question"] == "Tell me truly, " + q.text || chat["question"] == "Tell me truly, " + lower));
    CHECK(chat["answers"][0]["label"] == q.physicalText);
}

TEST_CASE("The Turn screen shows the model's two sentences (T135)", "[night][play][words]")
{
    const NightDef def = Data();
    auto mind = std::make_unique<StandIn>();
    mind->answer = [&def](const umbra::town::MindRequest &request) {
        return request.prompt.find("Midnight:") != std::string::npos
                   ? def.guests[3].name + ", I am so cold. " + def.guests[3].taken + " comes to collect."
                   : std::string{};
    };
    Words words(def, std::move(mind));
    NightState state = FoyerNight(def, 11, 0);
    state.marked = 3;
    state.round = static_cast<std::int8_t>(def.rules.clock.turnAfterRound);
    Play play(def, state);
    play.SetWords(&words);
    for (int i = 0; i < 3000 && !play.State().taken.present; ++i)
    {
        Settle(play);
        if (play.State().chat.active && play.PlayerTurn() && play.Screen() == "chat")
            play.HandleUi("chat", "answer", "0");
        else if (play.PlayerTurn() && play.Screen() == "menu")
            play.Key(InputKey::Enter);
        else
            play.Update(0.5F);
    }
    REQUIRE(play.State().taken.present);
    Until(play, [&] { return play.Screen() == "turn"; });
    REQUIRE(play.Screen() == "turn");
    CHECK(play.UiData()["turn"]["words"] == def.guests[3].name + ", I am so cold. " + def.guests[3].taken + " comes to collect.");
}

TEST_CASE("Line seeds come from the night, so Continue words the same lines (T135)", "[night][play][words]")
{
    const NightDef def = Data();
    const auto seedsAfter = [&](const NightState &state) {
        std::vector<std::uint32_t> seeds;
        auto mind = std::make_unique<StandIn>();
        mind->seeds = &seeds;
        mind->answer = [](const umbra::town::MindRequest &) { return std::string{}; };
        {
            Words words(def, std::move(mind));
            Play play = Play(def, state);
            play.SetWords(&words);
            for (int i = 0; i < 200 && play.Beats() < 6; ++i)
            {
                Settle(play);
                if (play.State().chat.active && play.PlayerTurn() && play.Screen() == "chat")
                    play.HandleUi("chat", "answer", "0");
                else if (play.PlayerTurn() && play.Screen() == "menu")
                    play.Key(InputKey::Enter);
                else
                    play.Update(10.0F);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        return seeds;
    };
    // The same state, played twice (as a save and its Continue would be): the same seeds.
    NightState state = FoyerNight(def, 9, 0);
    NightState restored;
    REQUIRE(Play::Restore(def, Play(def, state).Save(), restored));
    const std::vector<std::uint32_t> a = seedsAfter(state), b = seedsAfter(restored);
    CHECK_FALSE(a.empty());
    CHECK(a == b);
}

TEST_CASE("The save menu says when the written lines are used (T135)", "[night][play][words]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    CHECK(play.UiData()["writtenLines"] == true);
    Words none(def, nullptr);
    play.SetWords(&none);
    CHECK(play.UiData()["writtenLines"] == true);
    auto mind = std::make_unique<StandIn>();
    mind->answer = [](const umbra::town::MindRequest &) { return std::string{}; };
    Words words(def, std::move(mind));
    play.SetWords(&words);
    CHECK(play.UiData()["writtenLines"] == false);
}

TEST_CASE("Screens carry their pictures: the event's, the asker's and my portrait (T143)", "[night][play][art]")
{
    NightDef def = Data();
    SECTION("an event")
    {
        for (RoomDef &room : def.rooms)
            if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
                room.kind = RoomKind::Event, room.doors = 15;
        Play play(def, PlayerFirst(def));
        GoTo(play, StepCell(def, play));
        REQUIRE(play.HandleUi("arrive", "onward", "")); // the arrival first (T152)
        REQUIRE(play.Screen() == "event");
        const nlohmann::json event = play.UiData()["event"];
        const auto drawn = std::find_if(def.events.begin(), def.events.end(), [&](const EventDef &e) { return e.name == event["name"].get<std::string>(); });
        REQUIRE(drawn != def.events.end());
        CHECK(event["picture"] == "art/events/" + drawn->id + ".jpg");
    }
    SECTION("a chat and the HUD")
    {
        Play play(def, FoyerNight(def, 4, 0));
        CHECK(play.UiData()["me"]["portrait"] == "art/portraits/" + def.guests[0].id + ".jpg");
        for (int i = 0; i < 400 && !(play.State().chat.active && play.PlayerTurn()); ++i)
            play.PlayerTurn() ? play.Key(InputKey::Enter) : play.Update(10.0F);
        REQUIRE(play.State().chat.active);
        const int asker = play.State().chat.asker;
        CHECK(play.UiData()["chat"]["portrait"] == "art/portraits/" + def.guests[static_cast<std::size_t>(asker)].id + ".jpg");
    }
}

TEST_CASE("Room view is a switch: the player plays on in it; the night does not stop (T144, T158)", "[night][play][stage]")
{
    const NightDef def = Data();
    Play play(def, PlayerFirst(def));
    REQUIRE(play.Screen() == "menu");
    CHECK_FALSE(play.RoomView());
    // 1. The menu's switch turns the room view on; the menu stays and moves still work there.
    const nlohmann::json actions = play.UiData()["actions"];
    CHECK(std::any_of(actions.begin(), actions.end(), [](const nlohmann::json &a) { return a["action"] == "look" && a["label"] == "Room view"; }));
    REQUIRE(play.HandleUi("menu", "look", ""));
    CHECK(play.RoomView());
    CHECK(play.Screen() == "menu");
    const nlohmann::json again = play.UiData()["actions"];
    CHECK(std::any_of(again.begin(), again.end(), [](const nlohmann::json &a) { return a["action"] == "look" && a["label"] == "Board view"; }));
    const int step = StepCell(def, play);
    REQUIRE(step >= 0);
    GoTo(play, step); // a click on a room's floor walks, as on the table
    CHECK(play.State().guests[0].cell == step);
    CHECK(play.RoomView()); // the view stays as chosen
    // 2. Off the player's turn the switch does nothing (the user: it is the player's to use).
    REQUIRE(play.HandleUi("hud", "look", ""));
    CHECK_FALSE(play.RoomView());
    Play bot = BotFirst(def);
    REQUIRE_FALSE(bot.PlayerTurn());
    CHECK_FALSE(bot.HandleUi("hud", "look", ""));
    CHECK_FALSE(bot.RoomView());
    CHECK(bot.UiData()["canView"] == false);
    // 3. The top bar's portrait is the focus: a list of one, its action "focus".
    bot.Drag();
    REQUIRE(bot.HandleUi("hud", "focus", ""));
    CHECK(bot.Following());
    CHECK(bot.UiData()["topList"].size() == 1);
}

TEST_CASE("In room view a click on a figure engages it; a click on a floor item picks it up (T158)", "[night][play][stage]")
{
    const NightDef def = Data();
    NightState state = PlayerCorridor(def, 1, 3);
    const int bandage = NightDef::Find(def.items, "bandage");
    state.floor.push_back({static_cast<std::int8_t>(state.foyer), static_cast<std::int8_t>(bandage)});
    Play play(def, state);
    REQUIRE(play.HandleUi("menu", "look", ""));
    // 1. The item on the floor: picked up.
    REQUIRE(play.ClickItem(static_cast<int>(play.State().floor.size()) - 1));
    CHECK(std::find(play.State().guests[0].items.begin(), play.State().guests[0].items.end(), bandage) != play.State().guests[0].items.end());
    // 2. Another guest in the room: the chat with them opens, if the rules allow one.
    NightState together = PlayerCorridor(def, 1, 3);
    together.guests[1].cell = together.foyer;
    Play chat(def, together);
    const std::vector<Action> legal = LegalActions(def, chat.State());
    const bool canChat = std::any_of(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Chat && a.a == 1; });
    CHECK(chat.ClickGuest(1) == canChat);
    if (canChat)
        CHECK(chat.State().chat.active);
}

TEST_CASE("A battle in the player's light plays on the stage with both sides; one in the dark does not (T144)", "[night][play][stage]")
{
    const NightDef def = Data();
    NightState state = PlayerFirst(def);
    const int here = state.guests[0].cell;
    Monster moths;
    moths.kind = static_cast<std::int8_t>(NightDef::Find(def.monsters, "moths"));
    moths.cell = static_cast<std::int8_t>(here);
    moths.hp = 3, moths.might = 2, moths.sanity = 1;
    state.monsters.push_back(moths);
    // 1. The player attacks the monster in their own room: the stage shows both sides.
    Play play(def, state);
    const std::vector<Action> legal = LegalActions(def, play.State());
    const auto attack = std::find_if(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Attack; });
    REQUIRE(attack != legal.end());
    REQUIRE(play.HandleUi("menu", "act", std::to_string(attack - legal.begin())));
    REQUIRE(play.State().battle.active);
    CHECK(play.StageCell() == play.State().battle.cell);
    const std::vector<Play::Actor> actors = play.StageActors();
    CHECK(std::any_of(actors.begin(), actors.end(), [](const Play::Actor &a) { return a.guest == 0 && a.side == 0; }));
    CHECK(std::any_of(actors.begin(), actors.end(), [&](const Play::Actor &a) { return a.monsterKind == moths.kind && a.side == 1; }));
    // 2. The same battle moved to a cell the player cannot see: not staged.
    NightState far = play.State();
    const BoardView view = MakeView(def, far, 0);
    for (int c = 0; c < maxCells; ++c)
        if (!view.cells[static_cast<std::size_t>(c)].lit)
        {
            far.battle.cell = static_cast<std::int8_t>(c);
            break;
        }
    REQUIRE_FALSE(view.cells[static_cast<std::size_t>(far.battle.cell)].lit);
    CHECK(Play(def, far).StageCell() == -1);
}

namespace
{
    // Bots play (with Space off) until a seen beat that waits for a tap, or `limit` updates.
    bool UntilTap(Play &play, int limit)
    {
        for (int i = 0; i < limit; ++i)
        {
            Settle(play);
            if (play.WaitingForTap())
                return true;
            if (play.State().chat.active && play.PlayerTurn() && play.Screen() == "chat")
                play.HandleUi("chat", "answer", "0");
            else if (play.PlayerTurn() && play.Screen() == "menu")
                play.Key(InputKey::Enter);
            else
                play.Update(1.0F);
        }
        return false;
    }

    umbra::night::GameOptions Designed()
    {
        umbra::night::GameOptions options;
        options.tapToContinue = options.moments = true;
        return options;
    }
}

TEST_CASE("A bot beat where something happens waits for a tap, shown at full size (T148)", "[night][play][pace]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
    play.SetCaptions(captions);
    play.SetOptions(Designed());
    REQUIRE(UntilTap(play, 3000));
    // 1. It waits however long, on the caption with a Continue, its room on the stage.
    const int beats = play.Beats();
    CHECK(play.Screen() == "caption");
    CHECK(play.UiData()["caption"]["tap"] == true);
    CHECK(play.StageCell() == play.LastBeat().camera);
    play.Update(100.0F);
    CHECK(play.Beats() == beats);
    // 2. A tap lets the night go on.
    REQUIRE(play.HandleUi("caption", "next", ""));
    CHECK_FALSE(play.WaitingForTap());
}

TEST_CASE("Skip runs the bots to the player's turn without stopping (T148)", "[night][play][pace]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 4, 0);
    Play play(def, state);
    play.SetOptions(Designed());
    // Off the player's turn: skip, then plain time reaches the player's menu with no taps.
    for (int i = 0; i < 50 && play.PlayerTurn(); ++i)
        play.Key(InputKey::Enter);
    REQUIRE_FALSE(play.PlayerTurn());
    CHECK(play.UiData()["canSkip"] == true);
    REQUIRE(play.HandleUi("hud", "skip", ""));
    for (int i = 0; i < 2000 && !play.PlayerTurn(); ++i)
    {
        Settle(play);
        CHECK_FALSE(play.WaitingForTap());
        play.Update(1.0F);
    }
    CHECK(play.PlayerTurn());
}

TEST_CASE("The room panel says where the player stands and what waits there (T149)", "[night][play][room]")
{
    NightDef def = Data();
    NightState state = FoyerNight(def, 4, 0);
    Play play(def, state);
    const nlohmann::json here = play.UiData()["here"];
    CHECK(here["name"] == def.rooms[static_cast<std::size_t>(state.cells[static_cast<std::size_t>(state.foyer)].room)].name);
    // An armed trap in the player's room: named, with how to clear it.
    const int sandbag = NightDef::Find(def.events, "sandbag");
    state.cells[static_cast<std::size_t>(state.foyer)].event = static_cast<std::int8_t>(sandbag);
    const std::string text = Play(def, state).UiData()["here"]["what"].get<std::string>();
    CHECK(text.find(def.events[static_cast<std::size_t>(sandbag)].name) != std::string::npos);
    CHECK(text.find("Speed 5") != std::string::npos);
}

TEST_CASE("A bot's walk stays on the table; only what it does opens the room and waits (T150)", "[night][play][pace]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
    play.SetCaptions(captions);
    play.SetOptions(Designed());
    int waits = 0;
    for (int i = 0; i < 4000 && !play.State().over; ++i)
    {
        Settle(play);
        const std::uint32_t before = play.State().logCount;
        if (play.State().chat.active && play.PlayerTurn() && play.Screen() == "chat")
            play.HandleUi("chat", "answer", "0");
        else if (play.PlayerTurn() && play.Screen() == "menu")
            play.Key(InputKey::Enter);
        else
            play.Update(1.0F);
        if (!play.WaitingForTap())
            continue;
        // Something was done (a logged deed or a chat), or is about to be (a preview), never a plain step.
        bool deed = play.State().chat.active || play.UiData()["caption"]["preview"] == true;
        for (const LogEntry &e : NewLog(play.State(), before))
            deed = deed || (e.kind != LogKind::Opened && e.kind != LogKind::Effect && e.kind != LogKind::Woke && e.kind != LogKind::Level);
        CHECK(deed);
        ++waits;
        REQUIRE(play.HandleUi("caption", "next", ""));
    }
    CHECK(waits > 0);
}

TEST_CASE("After a room resolves the player may still look around; the bots wait for End turn (T151)", "[night][play][rest]")
{
    NightDef def = Data();
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    Play play(def, PlayerFirst(def));
    const int step = StepCell(def, play);
    REQUIRE(step >= 0);
    GoTo(play, step);
    Settle(play);
    // 1. The rules ended the turn, but the screen holds: Look around and End turn.
    CHECK_FALSE(play.PlayerTurn());
    REQUIRE(play.Screen() == "rest");
    play.Update(10.0F);
    CHECK(play.Beats() == 0);
    // 2. The switch shows the room just entered; the rest menu stays.
    REQUIRE(play.HandleUi("rest", "look", ""));
    CHECK(play.RoomView());
    CHECK(play.Screen() == "rest");
    // 3. End turn lets the bots go (Enter does too).
    REQUIRE(play.HandleUi("rest", "end", ""));
    for (int i = 0; i < 20 && play.Beats() == 0; ++i)
        play.Update(1.0F);
    CHECK(play.Beats() > 0);
}

namespace
{
    // The corridor with its first room made `roomId`, unresolved, `event` armed in it (-1: none).
    Play IntoRoom(NightDef &def, const char *roomId, int event)
    {
        umbra::night::GameOptions options;
        options.diceSeconds = 1.0F, options.moments = true;
        NightState state = PlayerCorridor(def, 1, 4);
        Cell &cell = state.cells[static_cast<std::size_t>(West(def, 1))];
        cell.room = static_cast<std::int8_t>(NightDef::Find(def.rooms, roomId));
        cell.resolved = false;
        cell.event = static_cast<std::int8_t>(event);
        cell.doors = 2 | 8;
        Play play(def, state);
        play.SetOptions(options);
        GoTo(play, West(def, 1));
        return play;
    }

    // Lets the die land: it tumbles for a second.
    void Land(Play &play)
    {
        for (int i = 0; i < 20 && play.UiData()["dice"]["rolling"] == true; ++i)
            play.Update(0.1F);
    }
}

TEST_CASE("Walking into a room plays it step by step: arrival, card, tumbling die, a plain result (T152)", "[night][play][flow]")
{
    NightDef def = Data();
    const int sandbag = NightDef::Find(def.events, "sandbag");
    SECTION("a lasting trap: arrival names it, the card says it stays, the die tumbles, the result is plain")
    {
        def.events[static_cast<std::size_t>(sandbag)].clear = ClearKind::None;
        Play play = IntoRoom(def, "loosestairs", sandbag);
        // 1. Arrival: the kind and the room, at full size.
        REQUIRE(play.Screen() == "arrive");
        CHECK(play.UiData()["arrive"]["kind"] == "TRAP ROOM");
        CHECK(play.UiData()["arrive"]["room"] == "Loose stairs");
        CHECK(play.StageCell() == West(def, 1));
        play.Update(10.0F);
        CHECK(play.Screen() == "arrive"); // it waits for the player
        REQUIRE(play.HandleUi("arrive", "onward", ""));
        // 2. The card: the event, that it stays, the roll and its odds.
        REQUIRE(play.Screen() == "event");
        const nlohmann::json event = play.UiData()["event"];
        CHECK(event["name"] == def.events[static_cast<std::size_t>(sandbag)].name);
        CHECK(event["lasting"].get<std::string>().find("until someone") != std::string::npos);
        REQUIRE(play.HandleUi("event", "roll", ""));
        // 3. The die tumbles: no verdict and no Continue until it lands.
        REQUIRE(play.Screen() == "dice");
        CHECK(play.UiData()["dice"]["rolling"] == true);
        CHECK_FALSE(play.HandleUi("dice", "continue", ""));
        Land(play);
        CHECK(play.UiData()["dice"]["rolling"] == false);
        // 4. The result in plain words, with what is left.
        const std::string result = play.UiData()["dice"]["result"].get<std::string>();
        CHECK(result.find(play.UiData()["dice"]["passed"] == true ? "Speed" : "now") != std::string::npos);
        const bool failed = play.UiData()["dice"]["failed"] == true;
        REQUIRE(play.HandleUi("dice", "continue", ""));
        CHECK(play.Screen() == "rest");
        // 5. The HUD flashes what it cost.
        CHECK(play.UiData()["me"]["flash"] == (failed ? "-5 HP" : ""));
    }
    SECTION("its condition met: the card says so, and no die")
    {
        def.events[static_cast<std::size_t>(sandbag)].clearAmount = 1;
        Play play = IntoRoom(def, "loosestairs", sandbag);
        REQUIRE(play.HandleUi("arrive", "onward", ""));
        REQUIRE(play.Screen() == "event");
        CHECK(play.UiData()["event"]["cleared"].get<std::string>().find("Speed") != std::string::npos);
        REQUIRE(play.HandleUi("event", "roll", ""));
        CHECK(play.Screen() == "rest");
    }
    SECTION("an item room: arrival, then what was found")
    {
        Play play = IntoRoom(def, "armoury", -1);
        REQUIRE(play.Screen() == "arrive");
        CHECK(play.UiData()["arrive"]["kind"] == "ITEM ROOM");
        REQUIRE(play.HandleUi("arrive", "onward", ""));
        REQUIRE(play.Screen() == "found");
        CHECK_FALSE(play.UiData()["found"]["name"].get<std::string>().empty());
        CHECK_FALSE(play.UiData()["found"]["does"].get<std::string>().empty());
        REQUIRE(play.HandleUi("found", "take", ""));
        CHECK(play.Screen() == "rest");
    }
}

TEST_CASE("A bot's roll in sight names the event it faced (T152)", "[night][play][flow]")
{
    NightDef def = Data();
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    Play play(def, FoyerNight(def, 4, 0));
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
    play.SetCaptions(captions);
    play.SetOptions(Designed());
    bool named = false;
    for (int i = 0; i < 40 && !named; ++i)
    {
        const std::uint32_t before = play.State().logCount;
        if (!UntilTap(play, 3000))
            break;
        for (const LogEntry &e : NewLog(play.State(), before))
            if (e.kind == LogKind::Check && e.who != play.State().player)
                named = !play.UiData()["caption"]["event"].get<std::string>().empty();
        play.HandleUi("caption", "next", "");
    }
    CHECK(named);
}

TEST_CASE("A bot's last beat that waits still shows its caption when the player's turn comes (T153)", "[night][play][pace]")
{
    const NightDef def = Data();
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
    int seen = 0;
    for (std::uint64_t seed = 1; seed < 40 && seen == 0; ++seed)
    {
        Play play(def, FoyerNight(def, seed, 0));
        play.SetCaptions(captions);
        play.SetOptions(Designed());
        for (int i = 0; i < 60; ++i)
        {
            if (!UntilTap(play, 3000))
                break;
            if (play.PlayerTurn())
            {
                // The beat ended the bot's turn: its caption must still be there to tap, then the board.
                ++seen;
                CHECK(play.Screen() == "caption");
                REQUIRE(play.HandleUi("caption", "next", ""));
                play.Update(10.0F);
                CHECK((play.StageCell() < 0 || play.State().chat.active || play.State().battle.active));
                break;
            }
            play.HandleUi("caption", "next", "");
        }
    }
    CHECK(seen > 0);
}

TEST_CASE("The camera follows whoever acts; a drag frees it only until the next turn (T154)", "[night][play][camera]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    ToPlayer(play);
    REQUIRE(play.PlayerTurn());
    play.Drag();
    CHECK_FALSE(play.Following());
    play.Key(InputKey::Enter); // end the turn: the next guest acts
    REQUIRE_FALSE(play.PlayerTurn());
    CHECK(play.Following());
    // A bot in the player's sight: the camera is on it.
    const int bot = Current(play.State());
    if (MakeView(def, play.State(), play.State().player).cells[static_cast<std::size_t>(play.State().guests[static_cast<std::size_t>(bot)].cell)].lit)
        CHECK(play.CameraCell() == play.State().guests[static_cast<std::size_t>(bot)].cell);
}

TEST_CASE("The top bar shows whoever acts; the Taken go by their risen name (T155)", "[night][play][hud]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 4, 0);
    // 1. The Taken's name once risen.
    const int g = 1;
    CHECK(ShownName(def, state, g) == def.guests[g].name);
    state.taken.present = true, state.taken.guest = g;
    CHECK(ShownName(def, state, g) == def.guests[g].taken);
    // 2. A bot's turn: its stats, marked as theirs; the player's HP stays in a chip.
    Play play(def, FoyerNight(def, 4, 0));
    ToPlayer(play);
    play.Key(InputKey::Enter);
    REQUIRE_FALSE(play.PlayerTurn());
    const int bot = Current(play.State());
    nlohmann::json top = play.UiData()["top"];
    CHECK(top["name"] == def.guests[static_cast<std::size_t>(bot)].name);
    CHECK(top["theirs"] == true);
    CHECK(top["hp"] == play.State().guests[static_cast<std::size_t>(bot)].hp);
    CHECK(play.UiData()["me"]["hp"] == play.State().guests[0].hp);
    // 3. The player's turn: their own.
    ToPlayer(play);
    top = play.UiData()["top"];
    CHECK(top["name"] == def.guests[0].name);
    CHECK(top["theirs"] == false);
}

TEST_CASE("A bonus room says what it gives on the way in, and announces it when paid (T156)", "[night][play][flow]")
{
    NightDef def = Data();
    Play play = IntoRoom(def, "rehearsal", -1);
    // 1. Walking in: what it will give.
    REQUIRE(play.Screen() == "arrive");
    CHECK(play.UiData()["arrive"]["kind"] == "BONUS ROOM");
    CHECK(play.UiData()["arrive"]["line"].get<std::string>().find("Might") != std::string::npos);
    REQUIRE(play.HandleUi("arrive", "onward", ""));
    // 2. Ending the turn there pays it, said plainly.
    const int might = play.State().guests[0].might;
    const std::vector<Action> legal = LegalActions(def, play.State());
    const auto end = std::find_if(legal.begin(), legal.end(), [](const Action &x) { return x.kind == ActionKind::EndTurn; });
    REQUIRE(end != legal.end());
    REQUIRE(play.HandleUi("menu", "act", std::to_string(end - legal.begin())));
    CHECK(play.State().guests[0].might == might + 1);
    REQUIRE(play.Screen() == "arrive");
    CHECK(play.UiData()["arrive"]["line"].get<std::string>().find("+1 Might") != std::string::npos);
    play.Update(10.0F);
    CHECK(play.Beats() == 0); // the bots wait for it
    REQUIRE(play.HandleUi("arrive", "onward", ""));
}

TEST_CASE("A walk back toward where moves count from is always affordable (T157)", "[night][play][moves]")
{
    const NightDef def = Data();
    Play play(def, PlayerCorridor(def, 3, 1));
    GoTo(play, West(def, 1));
    REQUIRE(play.State().guests[0].cell == West(def, 1));
    CHECK(play.State().guests[0].movesLeft == 0);
    // Out of moves, but the foyer is where they count from: the way back is free.
    REQUIRE(play.HandleUi("menu", "move", ""));
    play.Hover(play.State().foyer);
    CHECK(play.Previewed().affordable == 1);
    GoTo(play, play.State().foyer);
    CHECK(play.State().guests[0].cell == play.State().foyer);
    CHECK(play.State().guests[0].movesLeft == 1);
}

TEST_CASE("A bot's deed in sight is previewed before it happens, then its result shown (T159)", "[night][play][pace]")
{
    NightDef def = Data();
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    Play play(def, FoyerNight(def, 4, 0));
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
    play.SetCaptions(captions);
    play.SetOptions(Designed());
    int previews = 0;
    for (int i = 0; i < 60 && previews == 0; ++i)
    {
        if (!UntilTap(play, 3000))
            break;
        if (play.UiData()["caption"]["preview"] == true)
        {
            // 1. The preview: what the bot faces, nothing decided yet (no result, the log unchanged by it).
            ++previews;
            const std::uint32_t log = play.State().logCount;
            CHECK(play.UiData()["caption"]["result"] == "");
            CHECK_FALSE(play.UiData()["caption"]["event"].get<std::string>().empty());
            REQUIRE(play.HandleUi("caption", "next", ""));
            // 2. Then the deed and its result, waiting again.
            REQUIRE(UntilTap(play, 50));
            CHECK(play.State().logCount > log);
            CHECK(play.UiData()["caption"]["preview"] == false);
            CHECK_FALSE(play.UiData()["caption"]["result"].get<std::string>().empty());
        }
        play.HandleUi("caption", "next", "");
    }
    CHECK(previews > 0);
}

TEST_CASE("Answering a chat gets a reply from the asker, pleased or not (T160)", "[night][play][chat]")
{
    NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
    play.SetCaptions(captions);
    for (int i = 0; i < 400 && !(play.State().chat.active && play.PlayerTurn()); ++i)
        play.PlayerTurn() ? play.Key(InputKey::Enter) : play.Update(10.0F);
    REQUIRE(play.Screen() == "chat");
    const int asker = play.State().chat.asker;
    REQUIRE(play.HandleUi("chat", "answer", "0"));
    REQUIRE(play.Screen() == "dice");
    const nlohmann::json dice = play.UiData()["dice"];
    CHECK(dice["replyBy"] == ShownName(def, play.State(), asker));
    CHECK_FALSE(dice["reply"].get<std::string>().empty());
}

TEST_CASE("A blow in a bot's fight says who hit whom, for how much, and what is left (T161)", "[night][narrate]")
{
    const NightDef def = Data();
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
    NightState before = FoyerNight(def, 4, -1), after = before;
    after.log[after.logCount++ % after.log.size()] = {1, LogKind::Hit, 1, 2, 3, 9};
    const Beat beat = Narrate(def, before, after, NewLog(after, before.logCount), Action{ActionKind::Strike, 0}, 1, -1, captions);
    CHECK(beat.result.find(def.guests[1].name) != std::string::npos);
    CHECK(beat.result.find(def.guests[2].name) != std::string::npos);
    CHECK(beat.result.find("3") != std::string::npos);
    CHECK(beat.result.find("9 HP left") != std::string::npos);
}

TEST_CASE("A blow puts its damage over the struck guest for a moment (T161)", "[night][play][battle]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed < 60; ++seed)
    {
        Play play(def, FoyerNight(def, seed, 0));
        play.SetOptions(Designed());
        for (int i = 0; i < 6000 && !play.State().over; ++i)
        {
            const std::uint32_t before = play.State().logCount;
            Settle(play);
            if (play.WaitingForTap())
                play.HandleUi("caption", "next", "");
            else if (play.State().chat.active && play.PlayerTurn() && play.Screen() == "chat")
                play.HandleUi("chat", "answer", "0");
            else if (play.State().battle.active && play.PlayerTurn())
                play.HandleUi("battle", "guard", "");
            else if (play.PlayerTurn() && play.Screen() == "menu")
                play.Key(InputKey::Enter);
            else
                play.Update(0.1F);
            for (const LogEntry &e : NewLog(play.State(), before))
                if (e.kind == LogKind::Hit && e.other >= 0)
                {
                    const nlohmann::json popups = play.UiData()["popups"];
                    const bool shown = std::any_of(popups.begin(), popups.end(), [&](const nlohmann::json &p) {
                        return p["guest"] == e.other && p["text"] == "-" + std::to_string(e.a);
                    });
                    CHECK(shown);
                    return;
                }
        }
    }
    FAIL("no blow on a guest in 60 nights");
}

TEST_CASE("During the rest after a room the top bar is still the player's (T164)", "[night][play][hud]")
{
    NightDef def = Data();
    for (RoomDef &room : def.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Event, room.doors = 15;
    Play play(def, PlayerFirst(def));
    GoTo(play, StepCell(def, play));
    Settle(play);
    REQUIRE(play.Screen() == "rest");
    CHECK(play.UiData()["top"]["theirs"] == false);
    CHECK(play.UiData()["top"]["name"] == def.guests[0].name);
}

TEST_CASE("When the turn comes to the player the camera goes to them at once (T164)", "[night][play][camera]")
{
    const NightDef def = Data();
    Play play(def, FoyerNight(def, 4, 0));
    for (int i = 0; i < 4000; ++i)
    {
        Settle(play);
        if (play.WaitingForTap())
            play.HandleUi("caption", "next", "");
        else if (play.State().chat.active && play.PlayerTurn() && play.Screen() == "chat")
            play.HandleUi("chat", "answer", "0");
        else if (play.PlayerTurn())
        {
            CHECK(play.CameraCell() == play.State().guests[0].cell);
            if (play.Screen() != "menu")
                break;
            play.Key(InputKey::Enter);
        }
        else
            play.Update(0.2F);
    }
}

TEST_CASE("A guest knocked down is told plainly, and the HUD says so (T164)", "[night][play][flow]")
{
    NightDef def = Data();
    const int sandbag = NightDef::Find(def.events, "sandbag");
    def.events[static_cast<std::size_t>(sandbag)].clear = ClearKind::None;
    def.events[static_cast<std::size_t>(sandbag)].target = 99;
    def.events[static_cast<std::size_t>(sandbag)].failAmount = -99;
    Play play = IntoRoom(def, "loosestairs", sandbag);
    REQUIRE(play.HandleUi("arrive", "onward", ""));
    REQUIRE(play.HandleUi("event", "roll", ""));
    for (int i = 0; i < 20 && play.UiData()["dice"]["rolling"] == true; ++i)
        play.Update(0.2F);
    play.Update(1.0F);
    const std::string result = play.UiData()["dice"]["result"].get<std::string>();
    CHECK(result.find("down") != std::string::npos);
    CHECK(play.UiData()["me"]["down"] == true);
}
