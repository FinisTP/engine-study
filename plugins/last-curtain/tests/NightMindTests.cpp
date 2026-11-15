#include "NightTestData.hpp"

#include <NightMind.hpp>

#include <catch2/catch_test_macros.hpp>

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
