#include <CampaignText.hpp>
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
    save.player = {"Hoang", "male"};
    Notebook book(h.def, h.cast, h.memories, save);
    // 1. Characters: the player by name; Ti left out until he joins (feedback 2026-11-24 15:10, note 18).
    nlohmann::json d = book.Data();
    CHECK(d["tab"] == "characters");
    bool tiListed = false, playerNamed = false;
    for (const auto &e : d["entries"])
        tiListed |= e["id"] == "ti", playerNamed |= e["id"] == "player" && e["name"] == "Hoang";
    CHECK_FALSE(tiListed);
    CHECK(playerNamed);
    save.joined.insert("ti");
    tiListed = false;
    d = book.Data();
    for (const auto &e : d["entries"])
        tiListed |= e["id"] == "ti" && e["name"] != "?";
    CHECK(tiListed);
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

TEST_CASE("The notebook page shows XP toward the next level (feedback 2026-11-24 16:57, note 11)")
{
    HubData h;
    campaign::Save save;
    save.player = {"Hoang", "male"};
    save.roster.push_back({"bao", 2, 40});
    Notebook book(h.def, h.cast, h.memories, save);
    book.Select("bao");
    const nlohmann::json p = book.Data()["detail"];
    CHECK(p["xp"] == 40);
    CHECK(p["xpMax"] == campaign::xpPerLevel);
}

TEST_CASE("A scripted walk through slice 1: three story wins, the cafe brings Ti in, skirmishes refill, the save is written (campaign plan 7)")
{
    HubData h;
    campaign::Growth growth;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadGrowth(data / "growth.csv", growth, d));
    std::vector<campaign::StageDef> stages;
    for (const char *id : {"t1", "t2", "t2s", "t3", "stage1", "sk_props", "sk_stairs", "sk_boxoffice"})
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

TEST_CASE("A click near the door, the counter or the notebook walks there and uses it (feedback 2026-11-24 16:57, note 10)")
{
    CampaignRoom room(CafeRoomFile(), {"player", "bao", "mai"}, {"lethanh"});
    // 1. The spot under a click on the floor, within a step; none in the room's middle.
    const auto door = room.SpotAt({11.0F, 7.2F});
    REQUIRE(door);
    CHECK(door->kind == "door");
    CHECK_FALSE(room.SpotAt({6.0F, 4.0F}));
    // 2. Walking to it ends where E uses it.
    room.WalkTo(door->at);
    for (int i = 0; i < 2000 && room.Interact() != "door"; ++i)
        room.Update(0.02F);
    CHECK(room.Interact() == "door");
    // 3. Every spot has a place to label.
    CHECK(room.Spots().size() == 3);
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
    // 6. A click on someone walks up to where they stand: they end within reach (a talk follows).
    glm::vec2 mai{};
    for (const CampaignRoom::Person &p : room.People())
        if (p.who == "mai")
            mai = p.at;
    room.WalkTo(mai);
    for (int i = 0; i < 400; ++i)
        room.Update(0.02F);
    CHECK(room.Interact() == "talk:mai");
}

#include <Typing.hpp>

TEST_CASE("The on-screen keyboard puts a tone mark where Vietnamese puts it (plan 8, P17)", "[campaign][typing]")
{
    const Typing t = Typing::Load(data / "../../apps/LastCurtain/assets/typing.json");
    REQUIRE(t.Ok());
    // Tones: 1 sac, 2 huyen, 3 hoi, 4 nga, 5 nang, 0 none. Expected words written in the data file's letters.
    const auto row = [&](int r, int tone) { return t.Letter(r, tone); }; // vowel row r with a tone
    CHECK(t.AddTone("Lan", 1) == "L" + row(0, 1) + "n");                 // one vowel
    CHECK(t.AddTone("Mai", 2) == "M" + row(0, 2) + "i");                 // two, open: the first
    CHECK(t.AddTone("Quy", 1) == "Qu" + row(11, 1));                     // qu: the u is a consonant
    CHECK(t.AddTone("Gia", 2) == "Gi" + row(0, 2));                      // gi: the i too
    CHECK(t.AddTone("Toan", 2) == "To" + row(0, 2) + "n");               // closed: the last
    const std::string huong = "H" + row(10, 0) + row(8, 0) + "ng";       // Huong with its horns
    CHECK(t.AddTone(huong, 2) == "H" + row(10, 0) + row(8, 2) + "ng");   // the hatted vowel, the last of them
    CHECK(t.AddTone(t.AddTone("Lan", 1), 3) == "L" + row(0, 3) + "n");   // a new tone replaces the old
    CHECK(t.AddTone(t.AddTone("Lan", 1), 0) == "Lan");                   // none takes it off
    CHECK(t.AddTone("Nam Pht", 1) == "Nam Pht");                          // no vowel in the last word: unchanged
    CHECK(t.AddTone("", 1).empty());
    CHECK(t.Backspace("L" + row(0, 1)) == "L");                           // a whole letter, not a byte
    CHECK(t.Upper(row(10, 2)) == row(22, 2));                             // a vowel keeps its tone in capitals
    CHECK(t.Upper("n") == "N");
}

TEST_CASE("The notebook's character page as in the mockup: portrait, stats, fights or not, skills, a skill's upgrades (feedback 2026-11-24)")
{
    HubData h;
    campaign::Growth growth;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadGrowth(data / "growth.csv", growth, d));
    campaign::Save save;
    save.player = {"Hoang", "male"};
    save.roster = {{"player", 2, 0}, {"bao", 2, 0}, {"mai", 2, 0}};
    save.party = {"player", "bao"};
    Notebook book(h.def, h.cast, h.memories, save);
    book.SetGrowth(&growth);
    book.Select("bao");
    nlohmann::json p = book.Data()["detail"];
    // 1. Who: a portrait, level and stats from the growth at that level.
    CHECK(p["portrait"].get<std::string>().find("art/sprites/") == 0);
    CHECK(p["level"] == 2);
    CHECK(p["stats"]["hp"].get<int>() > 0);
    CHECK(p["fights"] == true);
    // 2. Skills: the basic attack first; one chosen shows its line and no versions for a skill with no upgrade.
    REQUIRE(p["skills"].size() >= 2);
    CHECK(p["skills"][0]["id"] == "basic");
    book.Skill("basic");
    p = book.Data()["detail"];
    CHECK(p["skill"]["name"] == h.def.abilities[static_cast<std::size_t>(std::ranges::find(h.def.abilities, std::string("basic"), &match::AbilityDef::id) - h.def.abilities.begin())].name);
    CHECK(!p["skill"]["line"].get<std::string>().empty());
    CHECK(p["skill"]["versions"].empty());
    // 3. Fights: toggled for anyone but the player.
    CHECK(book.ToggleFight("mai"));
    CHECK(std::ranges::find(save.party, std::string("mai")) != save.party.end());
    CHECK_FALSE(book.ToggleFight("player"));
}

TEST_CASE("Ky uc: each character's life by year; a moment shows ??? until its linked skill opens (the user, 2026-11-25 00:11)")
{
    HubData h;
    campaign::Growth growth;
    campaign::Skills skills;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadGrowth(data / "growth.csv", growth, d));
    REQUIRE(campaign::LoadSkills(data / "skills.json", h.def, skills, d));
    {
        CampaignText text;
        std::string error;
        (void)text.table.Load(std::filesystem::path(UMBRA_LASTCURTAIN_APP_ASSETS) / "localization.csv", error);
        text.Fill(skills);
    }
    campaign::Save save;
    save.roster = {{"player", 2, 0}, {"bao", 2, 0}};
    Notebook book(h.def, h.cast, h.memories, save);
    book.SetGrowth(&growth), book.SetSkills(&skills);
    book.Select("bao");
    // 1. In year order; the death paper's year is there, its words hidden until level 3.
    const auto paper = [&] {
        const nlohmann::json all = book.Data()["detail"]["timeline"]; // kept: a temporary's items would dangle
        for (const auto &m : all)
            if (m["skill"] == "giay_chung_tu")
                return m;
        return nlohmann::json();
    };
    const nlohmann::json life = book.Data()["detail"]["timeline"];
    REQUIRE(life.size() >= 4);
    for (std::size_t i = 1; i < life.size(); ++i)
        CHECK(life[i]["year"].get<int>() >= life[i - 1]["year"].get<int>());
    REQUIRE(paper().is_object());
    CHECK(paper()["known"] == false);
    CHECK(paper()["text"] == "???");
    CHECK(paper()["year"].get<int>() > 0);
    save.roster[1].level = 3;
    CHECK(paper()["known"] == true);
    CHECK(paper()["text"] != "???");
    // 2. A moment whose skill a memory upgrades waits for that memory too (Ti's message).
    save.joined.insert("ti"), save.roster.push_back({"ti", 4, 0});
    book.Select("ti");
    const auto ti = [&](const std::string &skill) {
        const nlohmann::json all = book.Data()["detail"]["timeline"];
        for (const auto &m : all)
            if (m["skill"] == skill)
                return m["known"].get<bool>();
        return true;
    };
    CHECK_FALSE(ti("na_cao_su"));
    save.memories.insert("ti_3");
    CHECK(ti("na_cao_su"));
}

TEST_CASE("The notebook's slots, pick then place: skills carried and in stock, locked ones with their level, items from storage, a card for what is picked (feedback 2026-11-24 15:10, notes 13, 16, 17)")
{
    HubData h;
    campaign::Growth growth;
    campaign::Skills skills;
    std::vector<core::Diagnostic> d;
    REQUIRE(campaign::LoadGrowth(data / "growth.csv", growth, d));
    REQUIRE(campaign::LoadSkills(data / "skills.json", h.def, skills, d));
    campaign::Save save;
    save.player = {"Hoang", "male"};
    save.roster = {{"player", 2, 0}, {"bao", 2, 0}};
    save.storage = {{"bandage", 2}, {"gung", 1}};
    Notebook book(h.def, h.cast, h.memories, save);
    {
        CampaignText text; // the stories, as the game fills them
        std::string error;
        (void)text.table.Load(std::filesystem::path(UMBRA_LASTCURTAIN_APP_ASSETS) / "localization.csv", error);
        text.Fill(skills);
    }
    book.SetGrowth(&growth), book.SetSkills(&skills);
    book.Select("bao");
    const auto page = [&] { return book.Data()["detail"]; };
    const auto has = [](const nlohmann::json &list, const std::string &id) { return std::ranges::any_of(list, [&](const nlohmann::json &x) { return x["id"] == id; }); };
    // 1. Four skill slots: his start (basic, scalpel) and two empty; the rest in stock; the death paper locked to level 3.
    nlohmann::json p = page();
    REQUIRE(p["loadout"].size() == 4);
    CHECK(p["loadout"][0]["id"] == "basic");
    CHECK(p["loadout"][1]["id"] == "dao_mo");
    CHECK(p["loadout"][2]["empty"] == true);
    CHECK(has(p["learned"], "chan_doan"));
    REQUIRE(has(p["locked"], "giay_chung_tu"));
    for (const auto &l : p["locked"])
        if (l["id"] == "giay_chung_tu")
            CHECK(l["level"] == 3);
    // 2. Picking a stocked skill shows its card (story, what it does, Mang theo); taking it fills a slot.
    book.Pick("learned:chan_doan");
    p = page();
    CHECK_FALSE(p["card"]["lore"].get<std::string>().empty());
    CHECK_FALSE(p["card"]["text"].get<std::string>().empty());
    CHECK(p["card"]["take"] == true);
    CHECK(p["card"]["drop"] == false);
    REQUIRE(book.Take());
    p = page();
    CHECK(p["loadout"][2]["id"] == "chan_doan");
    CHECK_FALSE(has(p["learned"], "chan_doan"));
    // 3. A carried one: Bo ra puts it back in stock.
    book.Pick("skill:1");
    CHECK(page()["card"]["drop"] == true);
    REQUIRE(book.Drop());
    CHECK(has(page()["learned"], "dao_mo"));
    // 4. Items: storage lists what can be carried (ginger cannot); an empty slot picked first is where it goes.
    p = page();
    CHECK(has(p["store"], "bandage"));
    CHECK_FALSE(has(p["store"], "gung"));
    book.Pick("bag:2");
    book.Pick("store:bandage");
    CHECK(page()["card"]["take"] == true);
    REQUIRE(book.Take());
    CHECK(save.slots["bao"][2] == "bandage");
    book.Pick("bag:2");
    REQUIRE(book.Drop());
    CHECK(save.slots["bao"][2] == "");
    // 5. No more than storage holds: two bandages, two slots.
    book.Pick("store:bandage"), (void)book.Take();
    book.Pick("store:bandage"), (void)book.Take();
    book.Pick("store:bandage");
    CHECK(page()["card"]["take"] == false);
    CHECK_FALSE(book.Take());
    // 6. In battle nothing moves.
    book.SetReadOnly(true);
    book.Pick("skill:0");
    CHECK_FALSE(book.Drop());
}

TEST_CASE("In a cafe talk, whoever stands between the camera and the speaker fades (feedback 2026-11-24 15:10, note 15)")
{
    // The camera looks from the room's front (larger floor y is nearer); a figure is about a metre wide.
    const glm::vec2 speaker{5.0F, 3.0F};
    CHECK(CampaignRoom::Hides({5.3F, 4.5F}, speaker));  // in front, overlapping
    CHECK_FALSE(CampaignRoom::Hides({5.3F, 2.0F}, speaker)); // behind
    CHECK_FALSE(CampaignRoom::Hides({7.5F, 4.5F}, speaker)); // in front, but well to the side
    CHECK_FALSE(CampaignRoom::Hides(speaker, speaker));       // the speaker itself
}
