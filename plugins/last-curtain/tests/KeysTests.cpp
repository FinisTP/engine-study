#include <Keys.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using umbra::platform::InputKey;

TEST_CASE("The key file maps meanings to keys; a remap works; unknown rows are refused (campaign plan 5)", "[keys]")
{
    std::vector<umbra::core::Diagnostic> d;
    // 1. The shipped file: every meaning the game asks for is there.
    umbra::night::Keys keys;
    const std::filesystem::path file = std::filesystem::path(UMBRA_LASTCURTAIN_ASSETS) / "keys-keyboard.json";
    REQUIRE(keys.Load(nlohmann::json::parse(std::ifstream(file)), file.string(), d));
    CHECK(d.empty());
    for (const char *meaning : {"back", "confirm", "next", "endTurn", "skip", "hurry", "log", "pin", "notebook", "menu1", "menu6"})
    {
        INFO(meaning);
        CHECK(keys.Of(meaning) != InputKey::Unknown);
    }
    CHECK(keys.Of("endTurn") == InputKey::F);
    CHECK(keys.Name("endTurn") == "F");
    CHECK(keys.Of("menu6") == InputKey::Num6);
    // 2. A remap.
    umbra::night::Keys mine;
    REQUIRE(mine.Load({{"format", "umbra.lc-keys"}, {"version", 1}, {"keys", {{"endTurn", "Q"}}}}, "mine.json", d));
    CHECK(mine.Of("endTurn") == InputKey::Q);
    CHECK(mine.Of("log") == InputKey::L); // the rest keep their defaults
    // 3. An unknown meaning or key name is refused, naming its row.
    d.clear();
    umbra::night::Keys bad;
    CHECK_FALSE(bad.Load({{"format", "umbra.lc-keys"}, {"version", 1}, {"keys", {{"fly", "Q"}, {"log", "Banana"}}}}, "bad.json", d));
    REQUIRE(d.size() == 2);
    CHECK(d[0].field == "keys.fly");
    CHECK(d[1].field == "keys.log");
}
