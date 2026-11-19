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

    // Updates until it is the player's turn (or the match is over).
    void ToPlayer(MatchPlay &p)
    {
        for (int i = 0; i < 20000 && !p.PlayerTurn() && !p.State().over; ++i)
            p.Update(0.25F);
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
