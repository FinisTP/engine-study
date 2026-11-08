find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraUi STATIC)
add_library(Umbra::Ui ALIAS UmbraUi)

target_sources(
    UmbraUi
    PRIVATE
        src/Font.cpp
        src/Painter.cpp
        src/Skin.cpp
        src/Screen.cpp
        src/Ui.cpp
        src/Raster.cpp
        third_party/stb_truetype.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Font.hpp
            include/Painter.hpp
            include/Skin.hpp
            include/Screen.hpp
            include/Ui.hpp
            include/Raster.hpp
)

target_compile_features(UmbraUi PUBLIC cxx_std_20)

# The game UI (S3, UI.md): screens from data, drawn as triangles into core::UiDrawList. Headless:
# no renderer, window or ImGui; the renderers draw the triangles.
target_link_libraries(
    UmbraUi
    PUBLIC
        Umbra::Core
        Umbra::Assets
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
