#include <DialogView.hpp>

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace umbra;

namespace
{
    const std::filesystem::path data = UMBRA_CAMPAIGN_DATA;
    const std::filesystem::path tests = UMBRA_CAMPAIGN_TEST_DATA;
    const std::filesystem::path app = UMBRA_LASTCURTAIN_APP_ASSETS;

    // The face exists if the app's art has it.
    bool Exists(const std::string &path) { return std::filesystem::exists(app / path); }
}

TEST_CASE("The dialog's data: portraits face the middle, the listener dims, missing faces fall back")
{
    campaign::Cast cast;
    campaign::Scene scene;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadCast(data / "cast.csv", cast, d));
    REQUIRE(campaign::LoadScene(tests / "scenes" / "test.json", cast, scene, d));
    campaign::Facts facts;
    const campaign::Speaker player{"Lan", "female"};
    campaign::ScenePlayer p(scene, cast, player, facts);

    // 1. Narration: Le Thanh dimmed on the left, no name.
    nlohmann::json j = DialogData(p, cast, player, Exists)["dialog"];
    CHECK(j["narration"] == true);
    CHECK(j["name"] == "");
    CHECK(j["slots"]["l0"]["image"] == "art/sprites/lethanh/face_calm.png");
    CHECK(j["slots"]["l0"]["tint"] == "#8a7e71ff"); // dimmed 8a times the host's ffe8d0
    CHECK(j["slots"]["r0"]["image"] == "");
    CHECK(j["slots"]["r0"]["show"] == false);

    // 2. Line b, complete: Le Thanh lit with her talking face; Ti strung on the right, mirrored, dimmed, his
    //    bound sprite has no "afraid" face so calm is shown; the name tag on the left; the more-arrow.
    p.Advance();
    p.Advance();
    p.Advance();
    j = DialogData(p, cast, player, Exists)["dialog"];
    CHECK(j["name"] == "L\xe1\xbb\x87 Thanh");
    CHECK(j["nameLeft"] == true);
    CHECK(j["nameRight"] == false);
    CHECK(j["slots"]["l0"]["image"] == "art/sprites/lethanh/face_talk.png");
    CHECK(j["slots"]["l0"]["tint"] == "#ffe8d0ff");
    CHECK(j["slots"]["l0"]["flip"] == false);
    CHECK(j["slots"]["r0"]["image"] == "art/sprites/ti_bound/face_calm.png");
    CHECK(j["slots"]["r0"]["flip"] == true);
    CHECK(j["slots"]["r0"]["tint"] == "#8a5353ff");
    CHECK(j["more"] == true);
    CHECK(j["text"] == "Ch\xc3\xa0o ch\xe1\xbb\x8b Lan.");

    // 3. Line c, complete: two choices, no more-arrow; the name tag on the right.
    p.Advance();
    p.Advance();
    j = DialogData(p, cast, player, Exists)["dialog"];
    CHECK(j["nameRight"] == true);
    CHECK(j["more"] == false);
    REQUIRE(j["choices"].size() == 2);
    CHECK(j["choices"][1]["index"] == 1);
    REQUIRE(j["log"].size() == 3);
    CHECK(j["log"][1]["name"] == "L\xe1\xbb\x87 Thanh");

    // 4. A sprite with no faces at all: nothing shown, never a missing file.
    j = DialogData(p, cast, player, [](const std::string &) { return false; })["dialog"];
    CHECK(j["slots"]["l0"]["image"] == "");
    CHECK(j["slots"]["l0"]["show"] == false);
}
