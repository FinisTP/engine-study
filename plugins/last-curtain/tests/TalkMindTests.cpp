#include <TalkMind.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

using namespace umbra::talk;
using nlohmann::json;
namespace fs = std::filesystem;

namespace
{
    TestSet Set()
    {
        TestSet set;
        std::vector<umbra::core::Diagnostic> diagnostics;
        REQUIRE(LoadTestSet(UMBRA_CAMPAIGN_DATA, set, diagnostics));
        return set;
    }

    fs::path Copy(const char *name)
    {
        const fs::path dir = fs::temp_directory_path() / name;
        fs::remove_all(dir);
        fs::copy(UMBRA_CAMPAIGN_DATA, dir, fs::copy_options::recursive);
        return dir;
    }

    json Read(const fs::path &file)
    {
        std::ifstream in(file);
        return json::parse(in);
    }

    void Write(const fs::path &file, const json &value) { std::ofstream(file) << value.dump(2); }

    // Loads a broken copy: the first diagnostic as "file|field" ("" if it loaded).
    std::string Broken(const fs::path &dir)
    {
        TestSet set;
        std::vector<umbra::core::Diagnostic> diagnostics;
        if (LoadTestSet(dir, set, diagnostics))
            return "";
        REQUIRE_FALSE(diagnostics.empty());
        return fs::path(diagnostics[0].file).filename().string() + "|" + diagnostics[0].field;
    }
}

TEST_CASE("The model test set loads: four voices, twenty situations (campaign plan 1)", "[talk]")
{
    const TestSet set = Set();
    CHECK(set.voices.size() == 4);
    CHECK(set.situations.size() == 20);
    REQUIRE(set.Find("ti") != nullptr);
    CHECK(set.Find("ti")->fragments.size() == 4);
    CHECK(set.Find("nobody") == nullptr);
    CHECK(set.rules.passShare == Catch::Approx(0.9));
    CHECK(set.rules.maxMs == 1500);
    CHECK(set.rules.maxVideoMb == 3072);
    CHECK(set.rules.dialects.count("central") == 1);
    CHECK(set.prompt.find("{authored}") != std::string::npos);
}

TEST_CASE("A broken test set names the file, the field and the fix", "[talk]")
{
    // 1. An unknown speaker.
    fs::path dir = Copy("umbra-talk-speaker");
    json situations = Read(dir / "model-test/situations.json");
    situations["situations"][0]["speaker"] = "nobody";
    Write(dir / "model-test/situations.json", situations);
    CHECK(Broken(dir) == "situations.json|situations[0].speaker");
    // 2. A situation without its authored line.
    dir = Copy("umbra-talk-authored");
    situations = Read(dir / "model-test/situations.json");
    situations["situations"][2].erase("authored");
    Write(dir / "model-test/situations.json", situations);
    CHECK(Broken(dir) == "situations.json|situations[2].authored");
    // 3. A dialect the rules don't know.
    dir = Copy("umbra-talk-dialect");
    json voice = Read(dir / "voices/bao.json");
    voice["dialect"] = "martian";
    Write(dir / "voices/bao.json", voice);
    CHECK(Broken(dir) == "bao.json|dialect");
    // 4. A prompt without the line to reword.
    dir = Copy("umbra-talk-prompt");
    std::ofstream(dir / "model-test/prompt.txt") << "Say something.";
    CHECK(Broken(dir) == "prompt.txt|");
    // 5. A wrong version.
    dir = Copy("umbra-talk-version");
    json rules = Read(dir / "model-test/rules.json");
    rules["version"] = 9;
    Write(dir / "model-test/rules.json", rules);
    CHECK(Broken(dir) == "rules.json|version");
}
