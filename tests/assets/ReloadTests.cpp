#include <catch2/catch_test_macros.hpp>

#include <FileWatcher.hpp>
#include <GltfImporter.hpp>

#include "TestFiles.hpp"

using umbra::assets::FileWatcher;
using namespace std::chrono_literals;

namespace
{
    // Polls are driven with explicit times, so tests don't sleep.
    const FileWatcher::Clock::time_point t0{std::chrono::seconds{1000}};
}

TEST_CASE("A change is reported once, after it has been stable for the settle time")
{
    const auto file = test::TempDir("watch-basic") / "a.txt";
    test::WriteText(file, "one");
    FileWatcher watcher(250ms);
    watcher.SetFiles({file});
    REQUIRE(watcher.Poll(t0).empty());

    test::WriteText(file, "two!");
    REQUIRE(watcher.Poll(t0 + 10ms).empty());  // change seen, settle timer starts
    REQUIRE(watcher.Poll(t0 + 200ms).empty()); // still settling
    const auto changed = watcher.Poll(t0 + 300ms);
    REQUIRE(changed.size() == 1);
    REQUIRE(changed[0] == file);
    REQUIRE(watcher.Poll(t0 + 600ms).empty()); // reported only once
}

TEST_CASE("A file that keeps changing is not reported until it stops")
{
    const auto file = test::TempDir("watch-busy") / "a.txt";
    test::WriteText(file, "x");
    FileWatcher watcher(250ms);
    watcher.SetFiles({file});

    // An editor saving in steps: every poll sees a different size.
    std::string text = "x";
    for (int step = 1; step <= 5; ++step)
    {
        text += "more";
        test::WriteText(file, text);
        REQUIRE(watcher.Poll(t0 + step * 200ms).empty());
    }
    REQUIRE(watcher.Poll(t0 + 1300ms).size() == 1);
}

TEST_CASE("Deleting and recreating a file are both reported")
{
    const auto dir = test::TempDir("watch-delete");
    const auto file = dir / "a.txt";
    test::WriteText(file, "data");
    FileWatcher watcher(100ms);
    watcher.SetFiles({file});

    std::filesystem::remove(file);
    REQUIRE(watcher.Poll(t0).empty());
    REQUIRE(watcher.Poll(t0 + 150ms).size() == 1);

    test::WriteText(file, "back again");
    REQUIRE(watcher.Poll(t0 + 200ms).empty());
    REQUIRE(watcher.Poll(t0 + 350ms).size() == 1);
}

TEST_CASE("Changing the watch list keeps old state and does not report new files")
{
    const auto dir = test::TempDir("watch-set");
    test::WriteText(dir / "a.txt", "a");
    test::WriteText(dir / "b.txt", "b");
    FileWatcher watcher(100ms);
    watcher.SetFiles({dir / "a.txt"});

    test::WriteText(dir / "a.txt", "changed");
    REQUIRE(watcher.Poll(t0).empty());
    watcher.SetFiles({dir / "a.txt", dir / "b.txt"}); // a's pending change survives
    REQUIRE(watcher.Count() == 2);
    const auto changed = watcher.Poll(t0 + 200ms);
    REQUIRE(changed == std::vector<std::filesystem::path>{dir / "a.txt"});
}

TEST_CASE("SameStructure tells content edits from node changes")
{
    const auto result = umbra::assets::ImportGltf(std::filesystem::path{UMBRA_TEST_ASSET_DIRECTORY} / "models" / "tower.gltf");
    REQUIRE(result.Ok());
    const umbra::assets::Model &original = result.model;

    umbra::assets::Model recolored = original;
    recolored.meshes[0].data.baseColor = {1, 0, 0, 1};
    recolored.nodes[1].position.y += 1.0F; // moving a node is a content change
    REQUIRE(umbra::assets::SameStructure(original, recolored));

    umbra::assets::Model renamed = original;
    renamed.nodes[2].name = "Spire";
    REQUIRE_FALSE(umbra::assets::SameStructure(original, renamed));

    umbra::assets::Model grown = original;
    grown.nodes.push_back({"Flag", 3});
    REQUIRE_FALSE(umbra::assets::SameStructure(original, grown));
}
