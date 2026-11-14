add_library(UmbraNightBoard STATIC)
add_library(Umbra::NightBoard ALIAS UmbraNightBoard)

target_sources(
    UmbraNightBoard
    PRIVATE
        src/BoardView.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/BoardView.hpp
            include/Board.md
)

target_compile_features(UmbraNightBoard PUBLIC cxx_std_20)

# What the player may see of the night, and its narration: headless, tested without a window.
find_package(glm CONFIG REQUIRED)
target_link_libraries(
    UmbraNightBoard
    PUBLIC
        Umbra::Night
        glm::glm
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
