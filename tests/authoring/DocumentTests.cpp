#include "assets/TestFiles.hpp"

#include <Document.hpp>
#include <RpgContent.hpp>
#include <SceneEdits.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace umbra;
using authoring::Change;
using authoring::Json;

namespace
{
    const char *sceneText = R"({
  "format": "umbra.scene",
  "version": 1,
  "entities": [
    {"id": "floor", "name": "Floor", "position": [0, -0.5, 0]},
    {"id": "door", "name": "Door"},
    {"id": "handle", "name": "Handle", "parent": "door", "position": [0.4, 1, 0]}
  ]
}
)";

    authoring::Document OpenScene(const std::filesystem::path &dir)
    {
        test::WriteText(dir / "test.scene.json", sceneText);
        authoring::Document doc(dir / "test.scene.json", authoring::SceneValidator({}));
        std::vector<core::Diagnostic> diagnostics;
        REQUIRE(doc.Open(diagnostics));
        return doc;
    }

    bool HasCode(const std::vector<core::Diagnostic> &diagnostics, const std::string &code)
    {
        for (const core::Diagnostic &d : diagnostics)
            if (d.code == code)
                return true;
        return false;
    }
}

TEST_CASE("An edit applies, undoes and redoes; revisions name states")
{
    const auto dir = test::TempDir("authoring-undo");
    authoring::Document doc = OpenScene(dir);
    const std::uint64_t opened = doc.Revision();
    REQUIRE_FALSE(doc.IsDirty());

    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(doc.Apply(authoring::SetTransform(0, {1, 2, 3}, glm::quat(1, 0, 0, 0), {2, 1, 2}), "Move floor", diagnostics));
    REQUIRE(doc.Data()["entities"][0]["position"] == Json::array({1.0, 2.0, 3.0}));
    REQUIRE(doc.IsDirty());
    REQUIRE(doc.UndoLabel() == "Move floor");

    REQUIRE(doc.Undo());
    REQUIRE(doc.Data()["entities"][0]["position"] == Json::array({0, -0.5, 0}));
    REQUIRE(doc.Revision() == opened);
    REQUIRE_FALSE(doc.IsDirty()); // back to what is on disk

    REQUIRE(doc.Redo());
    REQUIRE(doc.Data()["entities"][0]["position"] == Json::array({1.0, 2.0, 3.0}));
    REQUIRE_FALSE(doc.Redo());
}

TEST_CASE("An edit the scene loader rejects changes nothing and says why")
{
    const auto dir = test::TempDir("authoring-invalid");
    authoring::Document doc = OpenScene(dir);
    const Json before = doc.Data();
    const std::uint64_t revision = doc.Revision();

    std::vector<core::Diagnostic> diagnostics;
    // A duplicate id: the same check the game runs on load.
    REQUIRE_FALSE(doc.Apply({{Change::Op::Set, "/entities/1/id", "floor"}}, "Rename", diagnostics));
    REQUIRE_FALSE(diagnostics.empty());
    REQUIRE(doc.Data() == before);
    REQUIRE(doc.Revision() == revision);
    REQUIRE_FALSE(doc.CanUndo());

    // A bad path is rejected too.
    diagnostics.clear();
    REQUIRE_FALSE(doc.Apply({{Change::Op::Set, "/entities/9/name", "x"}}, "Rename", diagnostics));
    REQUIRE(HasCode(diagnostics, "edit.path"));

    // All or nothing: a valid change followed by an invalid one leaves no trace.
    diagnostics.clear();
    REQUIRE_FALSE(doc.Apply({{Change::Op::Set, "/entities/0/name", "Ground"}, {Change::Op::Set, "/entities/0/mesh", -1}}, "Edit", diagnostics));
    REQUIRE(doc.Data() == before);
}

TEST_CASE("An edit prepared against an older state is a conflict")
{
    const auto dir = test::TempDir("authoring-conflict");
    authoring::Document doc = OpenScene(dir);
    const std::uint64_t seen = doc.Revision();
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(doc.Apply({{Change::Op::Set, "/entities/0/name", "Ground"}}, "Rename", diagnostics));

    // Someone (a tool, an agent) prepared an edit before that rename.
    REQUIRE_FALSE(doc.Apply({{Change::Op::Set, "/entities/1/name", "Gate"}}, "Rename", diagnostics, seen));
    REQUIRE(HasCode(diagnostics, "edit.conflict"));
    REQUIRE(doc.Data()["entities"][1]["name"] == "Door");

    // After undo the document is in exactly that older state again, so the edit fits.
    REQUIRE(doc.Undo());
    diagnostics.clear();
    REQUIRE(doc.Apply({{Change::Op::Set, "/entities/1/name", "Gate"}}, "Rename", diagnostics, seen));
}

TEST_CASE("Merged edits make one undo step (a whole gizmo drag)")
{
    const auto dir = test::TempDir("authoring-merge");
    authoring::Document doc = OpenScene(dir);
    std::vector<core::Diagnostic> diagnostics;
    for (int i = 1; i <= 5; ++i)
        REQUIRE(doc.Apply(authoring::SetTransform(0, {static_cast<float>(i), 0, 0}, glm::quat(1, 0, 0, 0), {1, 1, 1}), "Move", diagnostics, std::nullopt, true));
    REQUIRE(doc.Data()["entities"][0]["position"][0] == 5.0);
    REQUIRE(doc.Undo());
    REQUIRE(doc.Data()["entities"][0]["position"] == Json::array({0, -0.5, 0}));
    REQUIRE_FALSE(doc.CanUndo());
}

TEST_CASE("Removing and renaming keep parents consistent")
{
    const auto dir = test::TempDir("authoring-tree");
    authoring::Document doc = OpenScene(dir);
    std::vector<core::Diagnostic> diagnostics;

    // A plain remove of the parent would leave the handle pointing at nothing: rejected.
    REQUIRE_FALSE(doc.Apply({{Change::Op::Remove, "/entities/1", {}}}, "Delete", diagnostics));

    // Rename updates the child; remove takes the subtree.
    REQUIRE(doc.Apply(authoring::RenameEntity(doc.Data(), "door", "gate"), "Rename", diagnostics));
    REQUIRE(doc.Data()["entities"][2]["parent"] == "gate");
    REQUIRE(doc.Apply(authoring::RemoveEntity(doc.Data(), "gate"), "Delete", diagnostics));
    REQUIRE(doc.Data()["entities"].size() == 1);

    // New ids never clash.
    REQUIRE(authoring::UniqueEntityId(doc.Data(), "floor") == "floor_2");
    REQUIRE(doc.Apply(authoring::AddEntity({{"id", "floor_2"}, {"name", "Second floor"}}), "Add", diagnostics));
    REQUIRE(authoring::FindEntity(doc.Data(), "floor_2") == 1);
}

TEST_CASE("Saving is atomic, keeps key order, and refuses to overwrite outside changes")
{
    const auto dir = test::TempDir("authoring-save");
    authoring::Document doc = OpenScene(dir);
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(doc.Apply({{Change::Op::Set, "/entities/0/name", "Ground"}}, "Rename", diagnostics));
    REQUIRE(doc.Save(diagnostics));
    REQUIRE_FALSE(doc.IsDirty());

    // The saved file is a valid scene for the game, with keys in their original order.
    const std::string saved = test::ReadText(dir / "test.scene.json");
    REQUIRE(saved.find("\"id\": \"floor\",\n      \"name\": \"Ground\"") != std::string::npos);
    world::World world;
    REQUIRE(scene::LoadScene(dir / "test.scene.json", {}, world, diagnostics));
    REQUIRE(*world.GetName(world.Find("floor")) == "Ground");

    // Someone else edits the file: saving now is a conflict, unless overwriting on purpose.
    test::WriteText(dir / "test.scene.json", sceneText);
    REQUIRE(doc.ChangedOnDisk());
    REQUIRE(doc.Apply({{Change::Op::Set, "/entities/1/name", "Gate"}}, "Rename", diagnostics));
    REQUIRE_FALSE(doc.Save(diagnostics));
    REQUIRE(HasCode(diagnostics, "save.conflict"));
    REQUIRE(test::ReadText(dir / "test.scene.json") == sceneText);
    REQUIRE(doc.Save(diagnostics, true));
}

TEST_CASE("A new item and a quest that uses it are added with data only")
{
    // Roadmap P7.2 evidence: supported content through data, checked by the game's own loader.
    const auto dir = test::TempDir("authoring-rpg");
    std::filesystem::copy_file(std::filesystem::path{UMBRA_TEST_ASSET_DIRECTORY} / "rpg/reference.rpg.json", dir / "game.rpg.json");
    authoring::Document doc(dir / "game.rpg.json", authoring::RpgValidator());
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(doc.Open(diagnostics));

    // A quest that needs an item that doesn't exist yet is refused...
    const Json quest = {{"id", "lost_amulet"}, {"name", "The Lost Amulet"}, {"description", "Find it."},
                        {"objectives", Json::array({{{"obtain", "amulet"}, {"text", "Find the amulet"}}})},
                        {"reward", {{"xp", 30}}}};
    REQUIRE_FALSE(doc.Apply({{Change::Op::Insert, "/quests/-", quest}}, "Add quest", diagnostics));
    REQUIRE(HasCode(diagnostics, "ref.item"));

    // ...and accepted once the item is added in the same edit.
    diagnostics.clear();
    const Json item = {{"id", "amulet"}, {"name", "Amulet"}};
    REQUIRE(doc.Apply({{Change::Op::Insert, "/items/-", item}, {Change::Op::Insert, "/quests/-", quest}}, "Add quest", diagnostics));
    REQUIRE(doc.Save(diagnostics));

    rpg::Content content;
    REQUIRE(rpg::LoadContent(dir / "game.rpg.json", content, diagnostics));
    REQUIRE(content.FindItem("amulet") != nullptr);
    REQUIRE(content.FindQuest("lost_amulet")->objectives.size() == 1);
}

TEST_CASE("Non-ASCII names and folders survive editing and saving")
{
    // Roadmap T37: ids stay ASCII, display text and paths may be anything (UTF-8).
    const auto dir = test::TempDir("authoring-utf8") / std::filesystem::path(u8"thư mục");
    std::filesystem::create_directories(dir);
    authoring::Document doc = OpenScene(dir);
    std::vector<core::Diagnostic> diagnostics;
    const std::string name = "B\xC3\xA1nh m\xC3\xAC \xE2\x80\x94 \xE5\xAE\x9D"; // "Bánh mì — 宝"
    REQUIRE(doc.Apply({{Change::Op::Set, "/entities/0/name", name}}, "Name", diagnostics));
    REQUIRE(doc.Save(diagnostics));

    world::World world;
    REQUIRE(scene::LoadScene(dir / "test.scene.json", {}, world, diagnostics));
    REQUIRE(*world.GetName(world.Find("floor")) == name);
}
