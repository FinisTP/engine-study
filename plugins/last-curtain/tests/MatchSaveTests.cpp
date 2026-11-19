#include "MatchTestData.hpp"

#include <MatchBots.hpp>
#include <MatchSave.hpp>

using namespace umbra::match;

namespace
{
    MatchState Start(const MatchDef &def, std::uint64_t seed)
    {
        return NewMatch(def, seed, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    }

    void Play(const MatchDef &def, MatchState &s, int actions)
    {
        for (int i = 0; i < actions && Current(s) >= 0; ++i)
            REQUIRE(Apply(def, s, ChooseAction(def, s, BotLevel::Medium)));
    }
}

TEST_CASE("A saved match loads to the same state (T308)", "[match][save]")
{
    const MatchDef def = Data();
    MatchState s = Start(def, 3);
    Play(def, s, 200);
    // Go on to a battle pick, the hardest moment to save.
    for (int i = 0; i < 5000 && !s.battle.on && Current(s) >= 0; ++i)
        REQUIRE(Apply(def, s, ChooseAction(def, s, BotLevel::Medium)));
    REQUIRE(s.battle.on);
    MatchState loaded;
    std::string error;
    REQUIRE(LoadMatch(SaveMatch(s), loaded, error));
    CHECK(Fingerprint(loaded) == Fingerprint(s));
    CHECK(Current(loaded) == Current(s));
    for (int i = 0; i < 200 && Current(s) >= 0; ++i)
    {
        const Action a = ChooseAction(def, s, BotLevel::Medium);
        REQUIRE(ChooseAction(def, loaded, BotLevel::Medium) == a);
        REQUIRE(Apply(def, s, a));
        REQUIRE(Apply(def, loaded, a));
        REQUIRE(Fingerprint(loaded) == Fingerprint(s));
    }
}

TEST_CASE("A save of another version or a broken file is refused (T309)", "[match][save]")
{
    const MatchDef def = Data();
    const MatchState s = Start(def, 4);
    MatchState out = Start(def, 9);
    const std::uint64_t before = Fingerprint(out);
    std::string error;
    nlohmann::json j = SaveMatch(s);
    j["version"] = 2;
    CHECK_FALSE(LoadMatch(j, out, error));
    CHECK(error.find("version") != std::string::npos);
    nlohmann::json cut = SaveMatch(s);
    cut.erase("units");
    CHECK_FALSE(LoadMatch(cut, out, error));
    CHECK_FALSE(LoadMatch(nlohmann::json::array(), out, error));
    CHECK(Fingerprint(out) == before);
}
