find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraRpg STATIC)
add_library(Umbra::Rpg ALIAS UmbraRpg)

target_sources(
    UmbraRpg
    PRIVATE
        src/RpgContent.cpp
        src/StoryContent.cpp
        src/Character.cpp
        src/Inventory.cpp
        src/SkillTree.cpp
        src/Rules.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/RpgContent.hpp
            include/Character.hpp
            include/Inventory.hpp
            include/SkillTree.hpp
            include/Rules.hpp
)

target_compile_features(UmbraRpg PUBLIC cxx_std_20)

target_link_libraries(
    UmbraRpg
    PUBLIC
        Umbra::Core
    PRIVATE
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)
