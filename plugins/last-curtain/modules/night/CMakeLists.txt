add_library(UmbraNight STATIC)
add_library(Umbra::Night ALIAS UmbraNight)

target_sources(
    UmbraNight
    PRIVATE
        src/Csv.cpp
        src/NightData.cpp
        src/Night.cpp
        src/Board.cpp
        src/Turn.cpp
        src/Battle.cpp
        src/Company.cpp
        src/Hunt.cpp
        src/Killer.cpp
        src/Quests.cpp
        src/NightSave.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/NightDef.hpp
            include/Night.hpp
            include/NightSave.hpp
            include/Csv.hpp
            include/Night.md
)

target_compile_features(UmbraNight PUBLIC cxx_std_20)

# The night of The Last Curtain: rules and data only, no renderer, window, model or rpg plugin (ADR 0012).
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(
    UmbraNight
    PUBLIC
        Umbra::Core
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
