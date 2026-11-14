add_library(UmbraNightBots STATIC)
add_library(Umbra::NightBots ALIAS UmbraNightBots)

target_sources(
    UmbraNightBots
    PRIVATE
        src/NightBots.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/NightBots.hpp
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
