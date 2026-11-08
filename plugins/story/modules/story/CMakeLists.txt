add_library(UmbraStory STATIC)
add_library(Umbra::Story ALIAS UmbraStory)

target_sources(
    UmbraStory
    PRIVATE
        src/QuestLog.cpp
        src/Conversation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/QuestLog.hpp
            include/Conversation.hpp
)

target_compile_features(UmbraStory PUBLIC cxx_std_20)

# Quests and conversations: shared rules only, no world, physics or renderer.
target_link_libraries(
    UmbraStory
    PUBLIC
        Umbra::Rpg
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
