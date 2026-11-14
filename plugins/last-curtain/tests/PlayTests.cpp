#include "NightTestData.hpp"

#include <BoardView.hpp>
#include <Narrate.hpp>
#include <NightBots.hpp>
#include <NightSave.hpp>
#include <Play.hpp>

#include <algorithm>
#include <filesystem>

using namespace umbra::night;
using umbra::platform::InputKey;

namespace
{
    // Dismisses the event and dice screens, which hold the bots.
    void Settle(Play &play)
    {
        while (play.HandleUi("event", "roll", "") || play.HandleUi("dice", "continue", ""))
        {
        }
    }

    // Bots play until it is the player's own turn; a chat a bot opened is answered first.
    void ToPlayer(Play &play)
    {
        for (int i = 0; i < 400 && (!play.PlayerTurn() || play.State().chat.active || play.Screen() != "actions"); ++i)
        {
            Settle(play);
            if (play.State().chat.active && play.PlayerTurn())
                play.HandleUi("chat", "answer", "0");
            else
                play.Update(10.0F);
        }
        REQUIRE(play.PlayerTurn());
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
    play.Click(step);
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
    CHECK(play.HandleUi("actions", end["action"], end["arg"]));
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
            play.HandleUi("actions", "act", play.UiData()["actions"].back()["arg"]);
        else
            play.Update(10.0F);
    REQUIRE(Current(play.State()) == 1);
    CHECK(play.PlayerTurn());
    play.Update(10.0F); // no bot plays an ally
    CHECK(Current(play.State()) == 1);
    CHECK(play.UiData()["me"]["name"] == def.guests[1].name);
    const int step = StepCell(def, play);
    REQUIRE(step >= 0);
    play.Click(step);
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
    CHECK(play.Screen() == "actions");
    // The bag opens and closes; it is free and does not end the turn.
    const int turn = play.State().turn;
    CHECK(play.HandleUi("actions", "bag", ""));
    CHECK(play.Screen() == "bag");
    CHECK(play.HandleUi("bag", "close", ""));
    CHECK(play.Screen() == "actions");
    CHECK(play.State().turn == turn);
    CHECK_FALSE(play.HandleUi("bag", "use", "9")); // an empty or unknown slot
    CHECK_FALSE(play.HandleUi("actions", "act", "999"));
    CHECK_FALSE(play.HandleUi("actions", "act", "x"));
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

TEST_CASE("Bot turns advance with time and hold on seen beats (T112)", "[night][play]")
{
    const NightDef def = Data();
    Play play = BotFirst(def);
    play.Update(0.01F); // the bot acts in the foyer, in the player's light
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
    play.Update(0.01F);
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
    play.Update(0.01F);
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
        play.Click(step);
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
    play.Click(StepCell(def, play));
    REQUIRE(play.HandleUi("event", "roll", ""));
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
                REQUIRE(play.HandleUi("actions", "act", std::to_string(i)));
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
                    play.HandleUi("actions", actions.front()["action"], actions.front()["arg"]); // a level up, say
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
        play.Click(StepCell(def, play));
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
    play.Click(step);
    REQUIRE(play.Screen() == "event");
    CHECK(play.State().guests[0].cell == play.State().foyer); // pushed back
    CHECK(play.UiData()["event"]["room"] == def.rooms[static_cast<std::size_t>(play.State().cells[static_cast<std::size_t>(step)].room)].name);
}
