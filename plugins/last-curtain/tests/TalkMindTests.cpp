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

    // A test line from tests/data/talk-lines.json (UTF-8 lives in data, not in C++ literals).
    std::string Line(const char *name)
    {
        static const json lines = Read(fs::path(UMBRA_TALK_TEST_DATA) / "talk-lines.json");
        return lines.at(name).get<std::string>();
    }

    const Situation &Find(const TestSet &set, const char *id)
    {
        for (const Situation &s : set.situations)
            if (s.id == id)
                return s;
        FAIL("no situation " << id);
        return set.situations[0];
    }

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

TEST_CASE("Lower, Words and Tidy handle Vietnamese letters", "[talk]")
{
    CHECK(Lower(Line("upper-insult")) == Line("upper-insult-lower"));
    CHECK(Lower("ABC xyz") == "abc xyz");
    const std::vector<std::string> words = Words(Line("words-in"));
    REQUIRE(words.size() == 3); // the two tokens are not words
    CHECK(words[0] == Lower(Line("words-in")).substr(0, words[0].size()));
    CHECK(Tidy(Line("quoted")) == Line("good"));
    CHECK(Tidy("  \"plain\" ") == "plain");
}

TEST_CASE("The Vietnamese checks, in order: the first objection wins", "[talk]")
{
    const TestSet set = Set();
    const Situation &mother = Find(set, "ti-visit1-mother");
    CHECK(Check(set, mother, Line("good")).empty());
    CHECK(Check(set, mother, "") == "empty");
    CHECK(Check(set, mother, Line("decomposed")) == "bad-text");
    CHECK(Check(set, mother, Line("digits")) == "bad-text");
    CHECK(Check(set, mother, Line("two-lines")) == "bad-text");
    CHECK(Check(set, mother, std::string("M\xff\xfe tui t\xc3n L\xc3\xa0i")) == "bad-text"); // broken UTF-8
    CHECK(Check(set, mother, Line("too-short")) == "length");
    CHECK(Check(set, mother, Line("no-diacritics")) == "no-marks");
    CHECK(Check(set, mother, Line("english-mixed")) == "english");
    CHECK(Check(set, mother, Line("english-letters")) == "english");
    CHECK(Check(set, mother, Line("upper-insult")) == "never");
    CHECK(Check(set, mother, Line("southern-ti")) == "dialect");
    CHECK(Check(set, mother, Line("bad-token")) == "token");
    CHECK(Check(set, mother, Line("no-mother")) == "missing-fact");
    CHECK(Check(set, mother, Line("sand-too-early")) == "added-memory");
}

TEST_CASE("Every situation's authored line passes its own checks", "[talk]")
{
    const TestSet set = Set();
    for (const Situation &s : set.situations)
    {
        INFO(s.id);
        CHECK(Check(set, s, s.authored).empty());
    }
}
