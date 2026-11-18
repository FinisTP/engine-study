find_package(Catch2 3 CONFIG REQUIRED)

# The night of The Last Curtain, headless.
add_executable(UmbraNightTests)
target_sources(UmbraNightTests PRIVATE NightDataTests.cpp BoardTests.cpp TurnTests.cpp BattleTests.cpp CompanyTests.cpp SaveTests.cpp BotTests.cpp BoardViewTests.cpp NarrateTests.cpp TableTests.cpp PlayTests.cpp PawnModelTests.cpp NightMindTests.cpp WordsTests.cpp ArtTests.cpp OptionsTests.cpp TableCameraTests.cpp RoyaleTests.cpp MotionTests.cpp)
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
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraNightTests)

add_test(NAME Umbra.Night COMMAND UmbraNightTests)

# The faction match, headless (docs/design/2026-11-19-factions.md).
add_executable(UmbraMatchTests)
target_sources(UmbraMatchTests PRIVATE MatchDataTests.cpp MatchBoardTests.cpp)
target_compile_definitions(UmbraMatchTests PRIVATE UMBRA_MATCH_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/match")
target_compile_features(UmbraMatchTests PRIVATE cxx_std_20)
target_link_libraries(UmbraMatchTests PRIVATE Umbra::Match Umbra::Night Umbra::Warnings Umbra::Sanitizers Catch2::Catch2WithMain)
umbra_copy_asan_runtime(UmbraMatchTests)
add_test(NAME Umbra.Match COMMAND UmbraMatchTests "~[match-balance]")
add_test(NAME Umbra.Match.Balance COMMAND UmbraMatchTests "[match-balance]")
add_test(NAME Umbra.Night.Balance COMMAND UmbraNightTests "[balance]")
