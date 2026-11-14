# LastCurtain.exe: the night on a table (docs/design/2026-11-14-board-on-screen.md). Its screens and
# skin live in assets/ here, not in the plugin's assets, which the Umbra game also loads.
add_executable(LastCurtain)
target_sources(
    LastCurtain
    PRIVATE
        Main.cpp
        Screens.cpp
        Screens.hpp
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
        Umbra::Ui
        Umbra::Night
        Umbra::NightBots
        Umbra::NightBoard
        Umbra::Warnings
        Umbra::Sanitizers
)
umbra_copy_asan_runtime(LastCurtain)
