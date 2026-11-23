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

#include <CafeHub.hpp>

namespace
{
    struct HubData
    {
        match::MatchDef def;
        campaign::Cast cast;
        campaign::Recipes recipes;
        campaign::Memories memories;
        campaign::Talks talks;
        std::vector<campaign::StageDef> stages;
        HubData()
        {
            std::vector<core::Diagnostic> d;
            REQUIRE(match::LoadMatch(data / "../match", def, d));
            REQUIRE(campaign::LoadCast(data / "cast.csv", cast, d));
            REQUIRE(campaign::LoadRecipes(data / "recipes.json", def, recipes, d));
            REQUIRE(campaign::LoadMemories(data / "memories.json", data / "scenes", recipes, def, memories, d));
            REQUIRE(campaign::LoadTalks(data / "talks", data / "scenes", cast, talks, d));
            campaign::StageDef s;
            REQUIRE(campaign::LoadStage(data / "stages" / "stage1.json", cast, def, data / "scenes", s, d));
            stages.push_back(s);
        }
    };
}

TEST_CASE("The cafe hub: who has something to say, brewing a memory and a boost, toasts (campaign plan 6)")
{
    HubData h;
    campaign::Save save;
    save.facts["won:stage1"] = "1";
    save.recipes = {"mut_gung", "nuoc_gung"};
    CafeHub hub(h.def, h.cast, h.recipes, h.memories, h.talks, h.stages, save, 3);
    // 1. People: Le Thanh has new talks; the brew line is off with nothing in storage.
    nlohmann::json d = hub.Data();
    bool lethanh = false;
    for (const auto &p : d["people"])
        lethanh |= p["who"] == "lethanh" && p["talk"] == true;
    CHECK(lethanh);
    CHECK(d["canBrew"] == false);
    // 2. Talking to Le Thanh after Stage 1 plays her first talk, then Ti's memory 1: Ti joins.
    std::string scene = hub.Talk("lethanh");
    CHECK(scene == "talk-lethanh-first");
    scene = hub.Talk("lethanh");
    CHECK(scene == "ti-memory-1");
    hub.SceneDone({{"ti.first", "hope"}});
    CHECK(save.joined.contains("ti"));
    CHECK_FALSE(hub.Data()["toasts"].empty());
    // 3. Storage filled: the jam is ready; its preview shows the two versions; brewing plays memory 2.
    save.storage = {{"gung", 2}, {"duong", 1}};
    d = hub.Data();
    CHECK(d["canBrew"] == true);
    nlohmann::json jam;
    for (const auto &r : d["recipes"])
        if (r["id"] == "mut_gung")
            jam = r;
    REQUIRE(jam.is_object());
    CHECK(jam["ok"] == true);
    CHECK(jam["versions"].size() == 2);
    CHECK(hub.Brew("mut_gung", "ti") == "ti-memory-2");
    hub.SceneDone({{"ti.memory2", "b"}});
    CHECK(save.versions["dua_tin"] == "dua_tin_phan_qua");
    // 3b. The keepsake in storage brings back memory 3 when given.
    save.storage["giay_gap_tu"] = 1;
    REQUIRE(hub.Data()["keepsakes"].size() == 1);
    CHECK(hub.Give("giay_gap_tu") == "ti-memory-3");
    hub.SceneDone({{"ti.memory3", "a"}});
    CHECK(save.versions["na_cao_su"] == "na_bao_dong");
    CHECK(hub.Data()["keepsakes"].empty()); // its memory is back
    // 4. A boost: Nuoc gung on Bao, no scene.
    CHECK(hub.Brew("nuoc_gung", "bao").empty());
    CHECK(save.boosts["bao"]["nuoc_gung"] == 1);
    CHECK(save.storage["gung"] == 0);
    // 5. Nothing left: the jam's row says what is missing and where.
    for (const auto &r : hub.Data()["recipes"])
        if (r["id"] == "mut_gung")
            CHECK(r["lacks"][0]["where"] != "");
}
