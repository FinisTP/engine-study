add_library(UmbraNightBots STATIC)
add_library(Umbra::NightBots ALIAS UmbraNightBots)

target_sources(
    UmbraNightBots
    PRIVATE
        src/NightBots.cpp
        src/NightSoak.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/NightBots.hpp
            include/NightSoak.hpp
            include/Balance.md
)

target_compile_features(UmbraNightBots PUBLIC cxx_std_20)

# The bots and the soak test: headless, built on the rules only.
target_link_libraries(
    UmbraNightBots
    PUBLIC
        Umbra::Night
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)

# The plugin's own data folder, the tools' default (night-run, night-soak without --data).
target_compile_definitions(UmbraNightBots PUBLIC UMBRA_NIGHT_DEFAULT_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../../assets/night")
