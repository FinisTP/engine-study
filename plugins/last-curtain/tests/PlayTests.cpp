#include "NightTestData.hpp"

#include <BoardView.hpp>
#include <Play.hpp>

using namespace umbra::night;
using umbra::platform::InputKey;

namespace
{
    // Bots play until it is the player's own turn; a chat a bot opened is answered first.
    void ToPlayer(Play &play)
    {
        for (int i = 0; i < 400 && (!play.PlayerTurn() || play.State().chat.active); ++i)
            if (play.State().chat.active)
                play.HandleUi("chat", "act", "0");
            else
                play.Update(10.0F);
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
        if (play.PlayerTurn())
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
