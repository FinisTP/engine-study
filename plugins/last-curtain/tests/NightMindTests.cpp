#include "NightTestData.hpp"

#include <NightMind.hpp>
#include <Narrate.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <sstream>

using namespace umbra::night;

namespace
{
    int G(const NightDef &def, const char *id) { return NightDef::Find(def.guests, id); }
}

TEST_CASE("A line's request names the speaker's tone and who it is about (T133)", "[night][mind]")
{
    const NightDef def = Data();
    const LineAsk ask{LineKind::Question, G(def, "thanh"), false, {G(def, "khang")}, "Do you trust Vo Minh Khang?", 77};
    const umbra::town::MindRequest request = Request(def, ask);
    CHECK(request.prompt.find("Le Thanh (theatrical, warm, proud)") != std::string::npos);
    CHECK(request.prompt.find("About: Vo Minh Khang (he)") != std::string::npos);
    CHECK(request.prompt.find("Line: Do you trust Vo Minh Khang?") != std::string::npos);
    CHECK(request.grammar.find("[A-Z]") != std::string::npos);
    CHECK(request.seed == 77U);

    // Once Taken, the form's name and the Taken voice.
    const LineAsk taken{LineKind::Taken, G(def, "thanh"), true, {G(def, "lan")}, "The Opera Specter strikes Tran Ngoc Lan.", 1};
    const std::string prompt = Request(def, taken).prompt;
    CHECK(prompt.find(def.guests[static_cast<std::size_t>(G(def, "thanh"))].taken + " (operatic, burning, mocking)") != std::string::npos);
}

TEST_CASE("The guard keeps names, pronouns and meaning, and hides outcomes and hints (T133)", "[night][mind]")
{
    const NightDef def = Data();
    const LineAsk caption{LineKind::Caption, G(def, "thanh"), false, {G(def, "thanh"), G(def, "lan")}, "Le Thanh talks with Tran Ngoc Lan.", 1};
    // 1. A clean rewording passes, tidied of quotes and spaces.
    CHECK(Guard(def, caption, "  \"Le Thanh leans in and whispers to Tran Ngoc Lan.\" ") == "Le Thanh leans in and whispers to Tran Ngoc Lan.");
    // 2. Names: none dropped, none added (a given name alone counts).
    CHECK(Guard(def, caption, "Le Thanh whispers to someone in the dark.").empty());
    CHECK(Guard(def, caption, "Le Thanh whispers to Tran Ngoc Lan while Mai watches.").empty());
    // 3. Pronouns from guests.csv: both are "she".
    CHECK(Guard(def, caption, "Le Thanh whispers to Tran Ngoc Lan, and he nods.").empty());
    CHECK_FALSE(Guard(def, caption, "Le Thanh whispers to Tran Ngoc Lan, and she nods.").empty());
    // 4. No digits, no flipped meaning, no outcome the template does not state.
    CHECK(Guard(def, caption, "Le Thanh talks with Tran Ngoc Lan for 2 minutes.").empty());
    CHECK(Guard(def, caption, "Le Thanh does not talk with Tran Ngoc Lan.").empty());
    CHECK(Guard(def, caption, "Le Thanh talks with Tran Ngoc Lan, who dies.").empty());

    // 5. A chat question must not hint at an answer's stat or which is easier.
    const LineAsk question{LineKind::Question, G(def, "thanh"), false, {}, "Would you sing for the empty hall?", 1};
    CHECK_FALSE(Guard(def, question, "Would you dare to sing for the empty hall, darling?").empty());
    CHECK(Guard(def, question, "Would you sing for the empty hall, if your Sanity holds?").empty());
    CHECK(Guard(def, question, "Would you sing for the empty hall? It is the easy answer.").empty());
}

namespace
{
    struct Echo final : umbra::town::Mind
    {
        std::string Name() const override { return "echo"; }
        std::string Complete(const umbra::town::MindRequest &request) override
        {
            const std::size_t at = request.prompt.rfind("Line: ") + 6;
            return "Quietly, " + request.prompt.substr(at, request.prompt.find('\n', at) - at);
        }
    };
}

TEST_CASE("TryLines words a few bot nights and counts what the guard accepts (T136)", "[night][mind]")
{
    const NightDef def = Data();
    std::vector<umbra::core::Diagnostic> diagnostics;
    Captions captions;
    REQUIRE(LoadCaptions(std::filesystem::path(UMBRA_NIGHT_DATA) / "captions.json", captions, diagnostics));
    Echo echo;
    std::ostringstream out;
    const LineReport report = TryLines(def, captions, echo, 3, 1, 8, out);
    CHECK(report.lines >= 3 * 4); // spread over each night: captions, chat questions, the Turn
    CHECK(report.accepted > 0);
    CHECK(report.accepted <= report.lines);
    const std::string text = out.str();
    CHECK(text.find("night 3") != std::string::npos);
    CHECK(text.find("template: ") != std::string::npos);
    CHECK(text.find("model:    Quietly, ") != std::string::npos);
    CHECK(text.find("accepted") != std::string::npos);
    CHECK(text.find("[the Turn]") != std::string::npos);
}

TEST_CASE("A Taken line keeps to the form's name (final review)", "[night][mind]")
{
    const NightDef def = Data();
    const int thanh = G(def, "thanh"), lan = G(def, "lan");
    const std::string form = def.guests[static_cast<std::size_t>(thanh)].taken;
    const LineAsk taunt{LineKind::Taken, thanh, true, {lan}, form + " attacks Tran Ngoc Lan.", 1};
    CHECK_FALSE(Guard(def, taunt, "Sing with me, Tran Ngoc Lan, says " + form + ".").empty());
    CHECK(Guard(def, taunt, "Le Thanh is gone, Tran Ngoc Lan, only " + form + " remains.").empty());
    CHECK(Guard(def, taunt, "Thanh is gone, Tran Ngoc Lan, only " + form + " remains.").empty());
}
