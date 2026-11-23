add_library(UmbraTalkMind STATIC)
add_library(Umbra::TalkMind ALIAS UmbraTalkMind)

target_sources(
    UmbraTalkMind
    PRIVATE
        src/TalkData.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/TalkMind.hpp
            include/TalkMind.md
)

target_compile_features(UmbraTalkMind PUBLIC cxx_std_20)

# The model's Vietnamese test (campaign plan 1). No model here: a model is any town::Mind
# (the living town's interface); only UmbraTool's talk-mind links llama.cpp.
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(
    UmbraTalkMind
    PUBLIC
        Umbra::Core
        Umbra::TownMind
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
