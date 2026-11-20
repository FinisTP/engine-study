# LastCurtain.exe: the night on a table (docs/design/2026-11-14-board-on-screen.md). Its screens and
# skin live in assets/ here, not in the plugin's assets, which the Umbra game also loads.
# The night as the person plays it, without a window, so the tests link it.
add_library(UmbraNightPlay STATIC Play.cpp Play.hpp Motion.cpp Motion.hpp Words.cpp Words.hpp Art.cpp Art.hpp Options.cpp Options.hpp TableCamera.cpp TableCamera.hpp)
add_library(Umbra::NightPlay ALIAS UmbraNightPlay)
target_include_directories(UmbraNightPlay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(UmbraNightPlay PUBLIC cxx_std_20)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(UmbraNightPlay PUBLIC Umbra::Night Umbra::NightBots Umbra::NightBoard Umbra::NightMind Umbra::Platform nlohmann_json::nlohmann_json
                      PRIVATE Threads::Threads Umbra::Warnings Umbra::Sanitizers)

# The faction match as the person plays it, without a window (MatchPlay.md).
add_library(UmbraMatchPlay STATIC MatchPlay.cpp MatchPlay.hpp CareerMatch.cpp CareerMatch.hpp CafePlay.cpp CafePlay.hpp)
add_library(Umbra::MatchPlay ALIAS UmbraMatchPlay)
target_include_directories(UmbraMatchPlay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(UmbraMatchPlay PUBLIC cxx_std_20)
target_link_libraries(UmbraMatchPlay PUBLIC Umbra::Match Umbra::MatchBots Umbra::NightPlay Umbra::Cafe nlohmann_json::nlohmann_json PRIVATE Umbra::Warnings Umbra::Sanitizers)

add_executable(LastCurtain)
target_sources(
    LastCurtain
    PRIVATE
        Main.cpp
        MatchApp.cpp
        CafeApp.cpp
        CafeApp.hpp
        CafeRoom.cpp
        CafeRoom.hpp
        GameContext.hpp
        MatchApp.hpp
        MatchTable.cpp
        MatchTable.hpp
        Screens.cpp
        Screens.hpp
        Sounds.cpp
        Sounds.hpp
        Table.cpp
        Table.hpp
        Table.md
)
target_compile_features(LastCurtain PRIVATE cxx_std_20)
if(MSVC)
    target_link_options(LastCurtain PRIVATE "/PDBALTPATH:%_PDB%")
endif()
target_link_libraries(
    LastCurtain
    PRIVATE
        Umbra::Core
        Umbra::Diagnostics
        Umbra::Platform
        Umbra::Render
        Umbra::Assets
        Umbra::Scene
        Umbra::Ui
        Umbra::Audio
        Umbra::Night
        Umbra::NightBots
        Umbra::NightBoard
        Umbra::NightPlay
        Umbra::MatchPlay
        $<TARGET_NAME_IF_EXISTS:Umbra::TownLlama>
        Umbra::Warnings
        Umbra::Sanitizers
)
umbra_copy_asan_runtime(LastCurtain)
