add_library(UmbraNightMind STATIC)
add_library(Umbra::NightMind ALIAS UmbraNightMind)

target_sources(
    UmbraNightMind
    PRIVATE
        src/NightMind.cpp
        src/TryLines.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/NightMind.hpp
            include/NightMind.md
)

target_compile_features(UmbraNightMind PUBLIC cxx_std_20)

# The model's lines for the night (plan 3): requests and the guard. No model here: a model is any
# town::Mind (the living town's interface; LlamaMind is linked only by LastCurtain.exe).
target_link_libraries(
    UmbraNightMind
    PUBLIC
        Umbra::Night
        Umbra::NightBoard
        Umbra::TownMind
    PRIVATE
        Umbra::NightBots
        Umbra::Warnings
        Umbra::Sanitizers
)
