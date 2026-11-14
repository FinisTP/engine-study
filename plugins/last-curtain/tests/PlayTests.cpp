#include "NightTestData.hpp"

#include <BoardView.hpp>
#include <Narrate.hpp>
#include <NightBots.hpp>
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
            if (play.State().chat.active)
                play.HandleUi("chat", "answer", "0");
            else if (play.State().omenOffer >= 0 && play.PlayerTurn())
                play.HandleUi("omen", "leave", "");
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
    Play play(def, 4, 0);
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
    Play play(def, 4, 0);
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
    Play play(def, 4, 0);
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
    NightState state = NewNight(def, 4, 0);
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
    Play play(def, 4, 0);
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
    Play play(def, 4, 0);
    CHECK(play.UiData()["allies"].empty());
    NightState state = NewNight(def, 4, 0);
    state.guests[0].group = state.guests[2].group = 0;
    state.guests[3].approvals = 1;
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
            Play play(def, seed, 0);
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
        return Play(def, 1, 0);
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
            if (NightState state = NewNight(def, seed, 0); Current(state) == 0)
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
        Play play(def, 4, 0);
        for (int i = 0; i < 400 && !play.State().chat.active; ++i)
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
    SECTION("the omen's curse")
    {
        NightState state = PlayerFirst(def);
        state.cells[static_cast<std::size_t>(state.foyer)].omen = 0;
        state.omenOffer = state.foyer;
        state.round = 9; // later in the night: the target has grown
        Play play(def, state);
        REQUIRE(play.Screen() == "omen");
        const int target = CurseTarget(def, play.State());
        const nlohmann::json omen = play.UiData()["omen"];
        CHECK(omen["target"] == target);
        CHECK(omen["chance"] == std::clamp((target - 1) * 5, 0, 100));
        CHECK(omen["name"] == def.omens[0].name);
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
    state.chat = {true, true, 1, 5};
    state.cells[static_cast<std::size_t>(state.foyer)].omen = 0;
    state.omenOffer = state.foyer;
    state.over = true;
    state.outcome = Outcome::Company;
    Play play(def, state);
    CHECK(play.Screen() == "");
    CHECK_FALSE(play.UiData().contains("chat"));
    CHECK_FALSE(play.UiData().contains("omen"));
    CHECK_FALSE(play.HandleUi("chat", "answer", "0"));
    play.Update(10.0F);
    CHECK(play.State().logCount == state.logCount);
}
