#include <Play.hpp>

#include <catch2/catch_test_macros.hpp>

#include <fstream>

using namespace umbra;
using json = nlohmann::json;

TEST_CASE("Play starts come from project.json; none gives the game on the scene")
{
    std::vector<core::Diagnostic> d;
    const json project = {{"play", {{{"label", "Career"}, {"program", "LastCurtain"}, {"args", json::array()}},
                                    {{"label", "This scene"}, {"program", "Umbra"}, {"args", {"{scene}"}}}}}};
    const auto starts = authoring::LoadPlayStarts(project, d);
    REQUIRE(starts.size() == 2);
    CHECK(starts[0].label == "Career");
    CHECK(starts[1].args == std::vector<std::string>{"{scene}"});
    CHECK(d.empty());

    const auto fallback = authoring::LoadPlayStarts(json::object(), d);
    REQUIRE(fallback.size() == 1);
    CHECK(fallback[0].program == "Umbra");
    CHECK(fallback[0].args == std::vector<std::string>{"{scene}"});

    // A start without a program is reported and left out.
    const auto bad = authoring::LoadPlayStarts({{"play", {{{"label", "x"}}}}}, d);
    CHECK(bad.size() == 1); // the fallback
    REQUIRE_FALSE(d.empty());
    CHECK(d.back().field == "play[0].program");
}

TEST_CASE("A play command is the program's path, then the arguments with {scene} filled")
{
    const json programs = {{"Umbra", "C:/out/Umbra.exe"}, {"LastCurtain", "C:/out/LastCurtain.exe"}};
    const authoring::PlayStart scene{"This scene", "Umbra", {"{scene}"}};
    CHECK(authoring::PlayCommand(scene, programs, "scenes/a.scene.json") == std::vector<std::string>{"C:/out/Umbra.exe", "scenes/a.scene.json"});
    const authoring::PlayStart prologue{"Prologue", "LastCurtain", {"--scene", "prologue"}};
    CHECK(authoring::PlayCommand(prologue, programs, "x") == std::vector<std::string>{"C:/out/LastCurtain.exe", "--scene", "prologue"});
    CHECK(authoring::PlayCommand({"?", "Nothing", {}}, programs, "x").empty());
}

TEST_CASE("Every start of the reference project names a program this build makes")
{
    std::ifstream projectFile(UMBRA_TEST_PROJECT_FILE), programsFile(UMBRA_TEST_PROGRAMS_FILE);
    REQUIRE(projectFile);
    REQUIRE(programsFile);
    const json project = json::parse(projectFile), programs = json::parse(programsFile);
    std::vector<core::Diagnostic> d;
    const auto starts = authoring::LoadPlayStarts(project, d);
    CHECK(d.empty());
    CHECK(starts.size() >= 2);
    for (const auto &start : starts)
    {
        INFO(start.label);
        CHECK_FALSE(authoring::PlayCommand(start, programs, "scenes/settlement.scene.json").empty());
    }
}
