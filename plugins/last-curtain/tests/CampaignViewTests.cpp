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

#include <NotebookView.hpp>

TEST_CASE("The notebook: three tabs, unknowns as ?, slots edited only outside battle and never with cafe-only items (campaign plan 6)")
{
    HubData h;
    campaign::Save save;
    save.storage = {{"gung", 1}, {"bandage", 2}};
    Notebook book(h.def, h.cast, h.memories, save);
    // 1. Characters: the party known, Ti a ? until he joins.
    nlohmann::json d = book.Data();
    CHECK(d["tab"] == "characters");
    bool tiHidden = false;
    for (const auto &e : d["entries"])
        tiHidden |= e["id"] == "ti" && e["name"] == "?";
    CHECK(tiHidden);
    // 2. Ingredients: ginger known, sugar a ?.
    book.Tab("ingredients");
    d = book.Data();
    std::map<std::string, std::string> names;
    for (const auto &e : d["entries"])
        names[e["id"]] = e["name"];
    CHECK(names["gung"] != "?");
    CHECK(names["duong"] == "?");
    // 3. It reopens where it was left.
    book.Select("gung");
    Notebook again = book;
    CHECK(again.Data()["tab"] == "ingredients");
    CHECK(again.Data()["selected"] == "gung");
    // 4. Slots: a bandage goes in; ginger never; in battle nothing changes.
    CHECK(book.SetSlot("bao", 0, "bandage"));
    CHECK(save.slots["bao"][0] == "bandage");
    CHECK_FALSE(book.SetSlot("bao", 1, "gung"));
    book.SetReadOnly(true);
    CHECK_FALSE(book.SetSlot("bao", 0, ""));
    CHECK(save.slots["bao"][0] == "bandage");
    CHECK(book.Data()["readOnly"] == true);
}

TEST_CASE("A scripted walk through slice 1: three story wins, the cafe brings Ti in, skirmishes refill, the save is written (campaign plan 7)")
{
    HubData h;
    campaign::Growth growth;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadGrowth(data / "growth.csv", growth, d));
    std::vector<campaign::StageDef> stages;
    for (const char *id : {"t1", "t3", "stage1", "sk_props", "sk_stairs", "sk_boxoffice"})
    {
        campaign::StageDef s;
        REQUIRE(campaign::LoadStage(data / "stages" / (std::string(id) + ".json"), h.cast, h.def, data / "scenes", s, d));
        stages.push_back(s);
    }
    const auto win = [&](campaign::Save &save, const char *id, std::vector<std::string> party) {
        const campaign::StageDef &s = *std::ranges::find(stages, std::string(id), &campaign::StageDef::id);
        campaign::BuiltStage b = campaign::BuildStage(h.def, h.cast, growth, s, campaign::PartyOf(save, party), save.rooms[id].refill, save.taken, 5);
        b.state.over = true, b.state.winner = 0;
        for (std::size_t i = 0; i < b.who.size(); ++i)
            if (b.who[i] == "ti")
                b.state.units[i].flags |= match::flag::Freed; // a stand-in fight: Ti cut free
        campaign::WinStage(h.def, save, s, b, stages);
    };
    campaign::Save save;
    save.player = {"Lan", "female"};
    save.roster = {{"player", 1, 0}, {"bao", 2, 0}, {"mai", 2, 0}};
    win(save, "t1", {"player"});
    win(save, "t3", {"player", "bao", "mai"});
    win(save, "sk_props", {"player", "bao", "mai"});
    win(save, "stage1", {"player", "bao", "mai"});
    CHECK(save.facts["freed:ti"] == "1");
    CHECK(save.rooms["sk_props"].refill == 1); // refilled after Stage 1
    CHECK(save.riteUnlocked);
    // The cafe: Le Thanh's talks; Ti joins and the two recipes are known.
    CafeHub hub(h.def, h.cast, h.recipes, h.memories, h.talks, stages, save, 3);
    for (int i = 0; i < 4 && !save.joined.contains("ti"); ++i)
    {
        (void)hub.Talk("lethanh");
        hub.SceneDone({{"ti.first", "hope"}});
    }
    CHECK(save.joined.contains("ti"));
    CHECK(save.recipes.contains("mut_gung"));
    // Written and read back.
    const auto file = std::filesystem::temp_directory_path() / "umbra-walk" / "campaign.json";
    std::string error;
    REQUIRE(campaign::SaveCampaign(file, save, error));
    campaign::Save back;
    REQUIRE(campaign::LoadCampaign(file, back, error));
    CHECK(back.joined.contains("ti"));
    CHECK(back.rooms["sk_props"].refill == 1);
}

#include <CampaignRoom.hpp>

#include <fstream>
#include <glm/geometric.hpp>

namespace
{
    nlohmann::json CafeRoomFile()
    {
        nlohmann::json room = nlohmann::json::parse(std::ifstream(data / "../cafe/room.json"));
        room["campaign"] = nlohmann::json::parse(std::ifstream(data / "cafe-room.json"));
        return room;
    }
}

TEST_CASE("The campaign cafe is a room: walk, talk with E, brew at the counter, switch who walks (the user, 2026-11-24)")
{
    CampaignRoom room(CafeRoomFile(), {"player", "bao", "mai"}, {"lethanh"});
    CHECK(room.Active() == "player");
    // 1. Everyone stands somewhere; the active one walks, the others wait at their places.
    REQUIRE(room.People().size() == 3); // Le Thanh, Bao, Ba Mai (the player walks)
    // 2. Walk up to Le Thanh behind the counter: E talks to her. The counter stops the walk.
    for (int i = 0; i < 400 && room.Interact() != "talk:lethanh"; ++i)
    {
        const glm::vec2 to = glm::vec2{6.0F, 1.5F} - room.Position();
        room.Move(glm::length(to) > 0.01F ? glm::normalize(to) : glm::vec2{0.0F}, 0.02F);
    }
    CHECK(room.Interact() == "talk:lethanh");
    CHECK(room.Position().y > 1.0F); // stopped at the counter
    // 3. Switch: Bao walks now from where the player stood; the player waits at Bao's... own place.
    const glm::vec2 where = room.Position();
    room.Switch();
    CHECK(room.Active() == "bao");
    CHECK(glm::length(room.Position() - where) < 0.01F);
    bool playerStands = false;
    for (const auto &p : room.People())
        playerStands |= p.who == "player";
    CHECK(playerStands);
    room.Switch(), room.Switch();
    CHECK(room.Active() == "player");
    // 4. The door and the brew spot.
    room.Place({11.2F, 7.3F});
    CHECK(room.Interact() == "door");
    room.Place({2.8F, 1.6F});
    CHECK(room.Interact() == "brew");
    room.Place({0.9F, 6.3F});
    CHECK(room.Interact() == "notebook");
    // 5. A click walks there on its own.
    room.Place({6.0F, 6.0F});
    room.WalkTo({8.0F, 6.0F});
    for (int i = 0; i < 200; ++i)
        room.Update(0.02F);
    CHECK(glm::length(room.Position() - glm::vec2{8.0F, 6.0F}) < 0.2F);
}
