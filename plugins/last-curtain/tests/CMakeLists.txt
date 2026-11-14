find_package(Catch2 3 CONFIG REQUIRED)

# The night of The Last Curtain, headless.
add_executable(UmbraNightTests)
target_sources(UmbraNightTests PRIVATE NightDataTests.cpp BoardTests.cpp TurnTests.cpp BattleTests.cpp CompanyTests.cpp HuntTests.cpp SaveTests.cpp BotTests.cpp BoardViewTests.cpp)
target_compile_definitions(UmbraNightTests PRIVATE UMBRA_NIGHT_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/night")
target_compile_features(UmbraNightTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraNightTests
    PRIVATE
        Umbra::Night
        Umbra::NightBots
        Umbra::NightBoard
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraNightTests)

add_test(NAME Umbra.Night COMMAND UmbraNightTests)
add_test(NAME Umbra.Night.Balance COMMAND UmbraNightTests "[balance]")
