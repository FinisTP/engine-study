find_package(Catch2 3 CONFIG REQUIRED)

# The night of The Last Curtain, headless.
add_executable(UmbraNightTests)
target_sources(UmbraNightTests PRIVATE NightDataTests.cpp BoardTests.cpp TurnTests.cpp BattleTests.cpp CompanyTests.cpp SaveTests.cpp BotTests.cpp BoardViewTests.cpp NarrateTests.cpp TableTests.cpp PlayTests.cpp PawnModelTests.cpp NightMindTests.cpp WordsTests.cpp ArtTests.cpp OptionsTests.cpp KeysTests.cpp TableCameraTests.cpp RoyaleTests.cpp MotionTests.cpp)
target_compile_definitions(UmbraNightTests PRIVATE UMBRA_NIGHT_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/night"
                                                   UMBRA_LASTCURTAIN_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/../apps/LastCurtain/assets")
target_compile_features(UmbraNightTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraNightTests
    PRIVATE
        Umbra::Night
        Umbra::NightBots
        Umbra::Audio
        Umbra::NightBoard
        Umbra::NightPlay
        Umbra::NightMind
        Umbra::Assets
        Umbra::Scene
        Umbra::Ui
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraNightTests)

add_test(NAME Umbra.Night COMMAND UmbraNightTests)

# The faction match, headless (docs/design/2026-11-19-factions.md).
add_executable(UmbraMatchTests)
target_sources(UmbraMatchTests PRIVATE MatchDataTests.cpp MatchBoardTests.cpp MatchSetupTests.cpp MatchTurnTests.cpp MatchBattleTests.cpp MatchAbilityTests.cpp MatchStageTests.cpp MatchMonsterTests.cpp MatchBotTests.cpp MatchSaveTests.cpp MatchSpawnTests.cpp MatchPlayTests.cpp MatchTreasureTests.cpp CafePlayTests.cpp MatchFightTests.cpp)
target_compile_definitions(UmbraMatchTests PRIVATE UMBRA_MATCH_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/match"
                                                   UMBRA_LASTCURTAIN_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/../apps/LastCurtain/assets"
                                                   UMBRA_CAFE_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/cafe")
target_compile_features(UmbraMatchTests PRIVATE cxx_std_20)
target_link_libraries(UmbraMatchTests PRIVATE Umbra::Match Umbra::MatchBots Umbra::MatchPlay Umbra::Night Umbra::Warnings Umbra::Sanitizers Catch2::Catch2WithMain)
umbra_copy_asan_runtime(UmbraMatchTests)
add_test(NAME Umbra.Match COMMAND UmbraMatchTests "~[match-balance]")
add_test(NAME Umbra.Match.Balance COMMAND UmbraMatchTests "[match-balance]")
add_test(NAME Umbra.Night.Balance COMMAND UmbraNightTests "[balance]")

# The cafe between matches, headless (modules/cafe/include/Cafe.md).
add_executable(UmbraCafeTests)
target_sources(UmbraCafeTests PRIVATE CafeTests.cpp)
target_compile_definitions(UmbraCafeTests PRIVATE UMBRA_CAFE_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/cafe")
target_compile_features(UmbraCafeTests PRIVATE cxx_std_20)
target_link_libraries(UmbraCafeTests PRIVATE Umbra::Cafe Umbra::Warnings Umbra::Sanitizers Catch2::Catch2WithMain)
umbra_copy_asan_runtime(UmbraCafeTests)
add_test(NAME Umbra.Cafe COMMAND UmbraCafeTests)

# The model's Vietnamese test (campaign plan 1, modules/talkmind/include/TalkMind.md), headless.
add_executable(UmbraTalkMindTests)
target_sources(UmbraTalkMindTests PRIVATE TalkMindTests.cpp)
target_compile_definitions(UmbraTalkMindTests PRIVATE UMBRA_CAMPAIGN_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/campaign"
                                                      UMBRA_TALK_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_compile_features(UmbraTalkMindTests PRIVATE cxx_std_20)
target_link_libraries(UmbraTalkMindTests PRIVATE Umbra::TalkMind Umbra::Warnings Umbra::Sanitizers Catch2::Catch2WithMain)
umbra_copy_asan_runtime(UmbraTalkMindTests)
add_test(NAME Umbra.TalkMind COMMAND UmbraTalkMindTests)

# The campaign: cast, scenes (modules/campaign/include/Campaign.md), headless.
add_executable(UmbraCampaignTests)
target_sources(UmbraCampaignTests PRIVATE CampaignTests.cpp CampaignViewTests.cpp)
target_compile_definitions(UmbraCampaignTests PRIVATE UMBRA_CAMPAIGN_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/campaign"
                                                      UMBRA_CAMPAIGN_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data"
                                                      UMBRA_LASTCURTAIN_APP_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/../apps/LastCurtain/assets"
                                                      UMBRA_MATCH_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/match")
target_compile_features(UmbraCampaignTests PRIVATE cxx_std_20)
target_link_libraries(UmbraCampaignTests PRIVATE Umbra::Campaign Umbra::CampaignPlay Umbra::Warnings Umbra::Sanitizers Catch2::Catch2WithMain)
umbra_copy_asan_runtime(UmbraCampaignTests)
add_test(NAME Umbra.Campaign COMMAND UmbraCampaignTests "~[campaign-balance]")
add_test(NAME Umbra.Campaign.Balance COMMAND UmbraCampaignTests "[campaign-balance]")
# The dialog screens drawn with their sample data: no errors, no warnings (campaign plan 3).
if(TARGET UmbraTool)
    add_test(NAME Umbra.Tool.DialogPreview
             COMMAND UmbraTool ui-preview dialog dialog-log --assets ${CMAKE_CURRENT_SOURCE_DIR}/../apps/LastCurtain/assets
                     --data ${CMAKE_CURRENT_SOURCE_DIR}/../apps/LastCurtain/assets/ui/dialog-sample.json --out ${CMAKE_CURRENT_BINARY_DIR}/dialog.png)
    set_tests_properties(Umbra.Tool.DialogPreview PROPERTIES PASS_REGULAR_EXPRESSION "0 error\\(s\\), 0 warning")
    # The campaign cafe and the notebook (campaign plan 6).
    add_test(NAME Umbra.Tool.CafePreview
             COMMAND UmbraTool ui-preview cafe-hub brew notebook worldmap prestage result gameover title newgame settings --assets ${CMAKE_CURRENT_SOURCE_DIR}/../apps/LastCurtain/assets
                     --data ${CMAKE_CURRENT_SOURCE_DIR}/../apps/LastCurtain/assets/ui/cafe-campaign-sample.json --out ${CMAKE_CURRENT_BINARY_DIR}/cafe-campaign.png)
    set_tests_properties(Umbra.Tool.CafePreview PROPERTIES PASS_REGULAR_EXPRESSION "0 error\\(s\\), 0 warning")
endif()
