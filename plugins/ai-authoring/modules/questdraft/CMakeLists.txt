find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraQuestDraft STATIC)
add_library(Umbra::QuestDraft ALIAS UmbraQuestDraft)

target_sources(
    UmbraQuestDraft
    PRIVATE
        src/QuestDraft.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/QuestDraft.hpp
)

target_compile_features(UmbraQuestDraft PUBLIC cxx_std_20)

# Plans and checks an AI quest draft (P8B, QuestDraft.md): headless, used by the editor and UmbraTool.
target_link_libraries(
    UmbraQuestDraft
    PUBLIC
        Umbra::Authoring
        Umbra::Game
        Umbra::Rpg
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
