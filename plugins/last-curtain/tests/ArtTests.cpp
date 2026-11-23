#include "NightTestData.hpp"

#include <Art.hpp>
#include <Font.hpp>
#include <Play.hpp>
#include <Wav.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace umbra::night;

namespace
{
    int RoomCell(const NightDef &def, const NightState &state, const char *room)
    {
        const int r = NightDef::Find(def.rooms, room);
        for (int c = 0; c < maxCells; ++c)
            if (state.cells[static_cast<std::size_t>(c)].room == r)
                return c;
        return -1;
    }
}

TEST_CASE("Featured rooms: foyer, under-stage, the night's start rooms and goal rooms (T140)", "[night][art]")
{
    const NightDef def = Data();
    const NightState start = NewNight(def, 21, 0);
    // 1. The start cells are where the guests began.
    const std::vector<int> starts = StartCells(def, start);
    REQUIRE(starts.size() == def.guests.size());
    for (std::size_t g = 0; g < starts.size(); ++g)
        CHECK(starts[g] == start.guests[g].cell);
    // 2. Later in the night (guests moved, saved and restored) they are the same.
    Play play(def, start);
    for (int i = 0; i < 60; ++i)
        play.PlayerTurn() ? play.Key(umbra::platform::InputKey::Enter) : play.Update(10.0F);
    NightState restored;
    REQUIRE(Play::Restore(def, play.Save(), restored));
    CHECK(StartCells(def, restored) == starts);
    // 3. Featured: those cells, the foyer, the under-stage, goal rooms; not a plain room.
    for (const int cell : starts)
        CHECK(Featured(def, restored, cell));
    CHECK(Featured(def, restored, restored.foyer));
    for (int c = 0; c < maxCells; ++c)
    {
        const int room = restored.cells[static_cast<std::size_t>(c)].room;
        if (room < 0 || std::find(starts.begin(), starts.end(), c) != starts.end())
            continue;
        const std::string &id = def.rooms[static_cast<std::size_t>(room)].id;
        const bool goal = std::any_of(def.goals.begin(), def.goals.end(), [&](const GoalDef &g) { return g.kind == GoalKind::Room && g.target == room; });
        const bool expected = id == "foyer" || id == "understage" || goal;
        CHECK(Featured(def, restored, c) == expected);
    }
    CHECK(RoomCell(def, restored, "foyer") == restored.foyer);
}

TEST_CASE("ArtPath names a picture by its data id; MissingArt lists what is not there (T140)", "[night][art]")
{
    CHECK(ArtPath("rooms", "foyer") == std::filesystem::path("art/rooms/foyer.jpg"));
    CHECK(ArtPath("cards", "moths") == std::filesystem::path("art/cards/moths.png"));
    const NightDef def = Data();
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / "umbra-art-test";
    std::filesystem::remove_all(folder);
    const std::vector<std::string> missing = MissingArt(def, folder);
    CHECK(std::find(missing.begin(), missing.end(), "art/rooms/foyer.jpg") != missing.end());
    CHECK(std::find(missing.begin(), missing.end(), "art/events/applause.jpg") != missing.end());
    CHECK(std::find(missing.begin(), missing.end(), "art/portraits/thanh.jpg") != missing.end());
    CHECK(std::find(missing.begin(), missing.end(), "art/plaques/foyer.png") != missing.end()); // its floor plaque (T213)
    CHECK(std::find(missing.begin(), missing.end(), "art/sky/before.jpg") != missing.end());    // the two skies (T214)
    CHECK(std::find(missing.begin(), missing.end(), "art/sky/after.jpg") != missing.end());
    std::filesystem::create_directories(folder / "art/rooms");
    std::ofstream(folder / "art/rooms/foyer.jpg") << "x";
    const std::vector<std::string> fewer = MissingArt(def, folder);
    CHECK(std::find(fewer.begin(), fewer.end(), "art/rooms/foyer.jpg") == fewer.end());
    CHECK(fewer.size() == missing.size() - 1);
    std::filesystem::remove_all(folder);
}

TEST_CASE("Every picture the night's data needs is in LastCurtain's assets (T141)", "[night][art]")
{
    const NightDef def = Data();
    const std::vector<std::string> missing = MissingArt(def, UMBRA_LASTCURTAIN_ASSETS);
    for (const std::string &path : missing)
        UNSCOPED_INFO("missing " << path);
    CHECK(missing.empty());
}

TEST_CASE("The night's mood: a creep from 19:00, a snap to blood at the Turn (T145)", "[night][art][mood]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 5, 0);
    // 1. The first round looks normal.
    state.round = 1;
    const Mood start = NightMood(def, state);
    CHECK(start.creep == 0.0F);
    CHECK(start.blood == 0.0F);
    CHECK(start.post.saturation == 1.0F);
    // 2. Just before the Turn: drained, colder, darker corners; still no blood.
    state.round = static_cast<std::int8_t>(def.rules.clock.turnAfterRound);
    const Mood late = NightMood(def, state);
    CHECK(late.creep == 1.0F);
    CHECK(late.blood == 0.0F);
    CHECK(late.post.saturation < 0.7F);
    CHECK(late.post.temperature < 0.0F);
    CHECK(late.post.vignette > start.post.vignette);
    CHECK(late.flicker > start.flicker);
    // 3. The Turn: blood at once, more as the hunt goes on, the blood grade on.
    state.taken.present = true;
    state.round = static_cast<std::int8_t>(def.rules.clock.turnAfterRound + 1);
    const Mood turned = NightMood(def, state);
    CHECK(turned.blood >= 0.3F);
    CHECK(turned.bloodGrade);
    state.round = static_cast<std::int8_t>(def.rules.clock.rounds);
    CHECK(NightMood(def, state).blood > turned.blood);
    CHECK(NightMood(def, state).blood <= 1.0F);
}

TEST_CASE("Walls, figures and items glow more as the night darkens, so they stay readable (T162)", "[night][art][mood]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 3);
    const float evening = Readability(NightMood(def, state));
    state.round = def.rules.clock.turnAfterRound;
    const float late = Readability(NightMood(def, state));
    state.taken.present = true;
    const float hunt = Readability(NightMood(def, state));
    CHECK(evening > 0.0F);
    CHECK(late > evening);
    CHECK(hunt >= late);
}

TEST_CASE("Every sound cue in sounds.csv names a WAV the engine plays (T227)", "[night][art][sound]")
{
    const std::filesystem::path assets = UMBRA_LASTCURTAIN_ASSETS;
    std::ifstream in(assets / "sounds.csv");
    REQUIRE(in);
    std::string line;
    std::getline(in, line);
    int cues = 0;
    while (std::getline(in, line))
    {
        const std::string file = line.substr(line.find(',') + 1, line.find(',', line.find(',') + 1) - line.find(',') - 1);
        std::string error;
        INFO(file);
        CHECK(umbra::audio::LoadWav(assets / file, error).has_value());
        CHECK(error.empty());
        ++cues;
    }
    CHECK(cues == 12); // with the killer (battle royale), pass and fail (2026-11-17)
}

TEST_CASE("Every font in the Last Curtain skin draws every Vietnamese letter (campaign plan 1)", "[night][art]")
{
    // 1. The letters: U+1EA0-1EF9, plus the Vietnamese ones outside that range.
    std::u32string vietnamese = U"\u0102\u0103\u00C2\u00E2\u0110\u0111\u00CA\u00EA\u00D4\u00F4\u01A0\u01A1\u01AF\u01B0"
                                U"\u00C0\u00C1\u00C3\u00C8\u00C9\u00CC\u00CD\u00D2\u00D3\u00D5\u00D9\u00DA\u00DD"
                                U"\u00E0\u00E1\u00E3\u00E8\u00E9\u00EC\u00ED\u00F2\u00F3\u00F5\u00F9\u00FA\u00FD\u0128\u0129\u0168\u0169";
    for (char32_t c = 0x1EA0; c <= 0x1EF9; ++c)
        vietnamese += c;
    // 2. Each font file the skin names.
    const std::filesystem::path assets = UMBRA_LASTCURTAIN_ASSETS;
    std::ifstream skinFile(assets / "ui/skin.json");
    const nlohmann::json skin = nlohmann::json::parse(skinFile);
    for (const auto &[role, face] : skin["fonts"].items())
    {
        std::ifstream in(assets / face["file"].get<std::string>(), std::ios::binary);
        const std::vector<unsigned char> ttf{std::istreambuf_iterator<char>(in), {}};
        INFO(role);
        CHECK(umbra::ui::MissingGlyphs(ttf, vietnamese).empty());
    }
}

TEST_CASE("Every letter of the campaign's Vietnamese words and screens is in the fonts (campaign plan 5)", "[night][art]")
{
    // 1. The text: the Vietnamese fight words, the campaign words, the campaign HUD and dialog screens.
    const std::filesystem::path assets = UMBRA_LASTCURTAIN_ASSETS;
    std::string text;
    for (const char *file : {"match-words.vi.json", "campaign-words.json", "ui/campaign-hud.ui.json", "ui/dialog.ui.json", "ui/cafe-hub.ui.json", "ui/brew.ui.json", "ui/notebook.ui.json", "ui/worldmap.ui.json", "ui/prestage.ui.json", "ui/result.ui.json", "ui/gameover.ui.json", "ui/title.ui.json", "ui/newgame.ui.json", "ui/settings.ui.json"})
    {
        std::ifstream in(assets / file, std::ios::binary);
        text += std::string{std::istreambuf_iterator<char>(in), {}};
    }
    // 2. UTF-8 to code points (letters only, sorted and unique).
    std::u32string letters;
    for (std::size_t i = 0; i < text.size();)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        const int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        char32_t cp = n == 1 ? c : c & (0x7F >> n);
        for (int k = 1; k < n && i + static_cast<std::size_t>(k) < text.size(); ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(text[i + static_cast<std::size_t>(k)]) & 0x3F);
        i += static_cast<std::size_t>(n);
        if (cp >= 0x20 && letters.find(cp) == std::u32string::npos)
            letters += cp;
    }
    // 3. The body and small fonts draw them all.
    const nlohmann::json skin = nlohmann::json::parse(std::ifstream(assets / "ui/skin.json"));
    for (const char *role : {"body", "small", "strong"})
    {
        std::ifstream in(assets / skin["fonts"][role]["file"].get<std::string>(), std::ios::binary);
        const std::vector<unsigned char> ttf{std::istreambuf_iterator<char>(in), {}};
        INFO(role);
        CHECK(umbra::ui::MissingGlyphs(ttf, letters).empty());
    }
}
