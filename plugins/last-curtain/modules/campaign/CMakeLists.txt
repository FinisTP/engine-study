add_library(UmbraCampaign STATIC)
add_library(Umbra::Campaign ALIAS UmbraCampaign)

target_sources(
    UmbraCampaign
    PRIVATE
        src/Cast.cpp
        src/Scene.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Campaign.hpp
            include/Campaign.md
)

target_compile_features(UmbraCampaign PUBLIC cxx_std_20)

# The campaign: cast, scenes; later stages, the save and the cafe (Campaign.md). Headless.
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(
    UmbraCampaign
    PUBLIC
        Umbra::Core
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Night
        Umbra::Warnings
        Umbra::Sanitizers
)
