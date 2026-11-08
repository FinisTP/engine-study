#include "RpgTestContent.hpp"
#include "assets/TestFiles.hpp"

#include <Inventory.hpp>
#include <SaveGame.hpp>

#include <catch2/catch_approx.hpp>

using namespace umbra;
using Catch::Approx;

namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }

    game::SaveData Sample()
    {
        game::SaveData d;
        d.scene = "scenes/dungeon.scene.json";
        d.feet = {1.5F, 0.0F, -3.25F};
        d.yaw = 0.75F;
        d.mode = game::CombatMode::Realtime;
        d.hero = rpgtest::Make("hero", "hero");
        d.hero.health = 42;
        d.hero.level = 3;
        d.hero.xp = 160;
        d.hero.skills.insert("heavy_blow");
        REQUIRE(rpg::AddItem(C(), d.hero, "iron_sword", 1));
        d.hero.pending.push_back({"sun_relic", 1});
        d.quests["sun_relic"] = {rpg::QuestState::Active, 1, 0, false};
        return d;
    }

    std::vector<core::Diagnostic> diagnostics;
    bool Has(const std::string &code)
    {
        for (const auto &d : diagnostics)
            if (d.code == code)
                return true;
        return false;
    }
}

TEST_CASE("A save restores the hero, quests, area, position and combat mode")
{
    const auto dir = test::TempDir("save-roundtrip");
    const game::SaveSlots slots{dir};
    REQUIRE(game::WriteSave(slots.Manual(), Sample(), diagnostics));
    game::SaveData loaded;
    REQUIRE(game::ReadSave(slots.Manual(), C(), loaded, diagnostics));
    REQUIRE(loaded.scene == "scenes/dungeon.scene.json");
    REQUIRE(loaded.feet.z == Approx(-3.25F));
    REQUIRE(loaded.mode == game::CombatMode::Realtime);
    REQUIRE(loaded.hero.health == 42);
    REQUIRE(loaded.hero.level == 3);
    REQUIRE(loaded.hero.skills.contains("heavy_blow"));
    REQUIRE(rpg::CountItem(loaded.hero, "iron_sword") == 1);
    REQUIRE(loaded.hero.equipment.at(rpg::Slot::Weapon) == "rusty_sword");
    REQUIRE(loaded.hero.pending.front().item == "sun_relic");
    REQUIRE(loaded.quests.at("sun_relic").objective == 1);
    REQUIRE(loaded.quests.at("sun_relic").state == rpg::QuestState::Active);
}

TEST_CASE("Damaged, foreign and newer saves are refused and change nothing")
{
    const auto dir = test::TempDir("save-bad");
    const auto file = dir / "manual.sav";
    REQUIRE(game::WriteSave(file, Sample(), diagnostics));
    const std::string good = test::ReadText(file);
    game::SaveData untouched;
    untouched.scene = "keep";

    // Edited payload: the checksum no longer matches.
    std::string edited = good;
    edited.replace(edited.find("42"), 2, "99");
    test::WriteText(file, edited);
    diagnostics.clear();
    REQUIRE_FALSE(game::ReadSave(file, C(), untouched, diagnostics));
    REQUIRE(Has("save.corrupt"));

    // Cut off half way (interrupted copy).
    test::WriteText(file, good.substr(0, good.size() / 2));
    REQUIRE_FALSE(game::ReadSave(file, C(), untouched, diagnostics));

    // From a newer game version.
    std::string newer = good;
    newer.replace(newer.find("\"version\": 1"), 12, "\"version\": 7");
    test::WriteText(file, newer);
    diagnostics.clear();
    REQUIRE_FALSE(game::ReadSave(file, C(), untouched, diagnostics));
    REQUIRE(Has("save.newer"));

    test::WriteText(file, R"({"hello":"world"})");
    REQUIRE_FALSE(game::ReadSave(file, C(), untouched, diagnostics));
    REQUIRE_FALSE(game::ReadSave(dir / "nothing.sav", C(), untouched, diagnostics));
    REQUIRE(untouched.scene == "keep");
}

TEST_CASE("A save that names content that no longer exists is refused")
{
    const auto dir = test::TempDir("save-content");
    game::SaveData d = Sample();
    d.hero.inventory.push_back({"old_removed_item", 1});
    REQUIRE(game::WriteSave(dir / "manual.sav", d, diagnostics));
    diagnostics.clear();
    game::SaveData loaded;
    REQUIRE_FALSE(game::ReadSave(dir / "manual.sav", C(), loaded, diagnostics));
    REQUIRE(Has("save.content.missing"));
}

TEST_CASE("Saving again keeps the previous save as a backup that rescues a damaged file")
{
    const auto dir = test::TempDir("save-backup");
    const auto file = dir / "manual.sav";
    game::SaveData first = Sample();
    first.hero.health = 10;
    REQUIRE(game::WriteSave(file, first, diagnostics));
    game::SaveData second = Sample();
    second.hero.health = 20;
    REQUIRE(game::WriteSave(file, second, diagnostics));

    test::WriteText(file, "garbage"); // the newest save got damaged
    diagnostics.clear();
    game::SaveData loaded;
    REQUIRE(game::ReadSaveOrBackup(file, C(), loaded, diagnostics));
    REQUIRE(loaded.hero.health == 10); // the previous save
    REQUIRE(Has("save.backup.used"));
    REQUIRE_FALSE(core::HasErrors(diagnostics));
}

TEST_CASE("A failed write leaves the existing save as it was")
{
    const auto dir = test::TempDir("save-fail");
    const auto file = dir / "manual.sav";
    game::SaveData first = Sample();
    first.hero.health = 33;
    REQUIRE(game::WriteSave(file, first, diagnostics));
    const std::string before = test::ReadText(file);

    // A folder where the temporary file must go makes the write fail, like a full or locked disk.
    std::filesystem::path blocker = file;
    blocker += ".tmp";
    std::filesystem::create_directories(blocker);
    diagnostics.clear();
    REQUIRE_FALSE(game::WriteSave(file, Sample(), diagnostics));
    REQUIRE(Has("save.write"));
    REQUIRE(test::ReadText(file) == before);
    std::filesystem::remove_all(blocker);
}
