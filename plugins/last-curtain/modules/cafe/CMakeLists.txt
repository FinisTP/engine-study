add_library(UmbraCafe STATIC)
add_library(Umbra::Cafe ALIAS UmbraCafe)

target_sources(
    UmbraCafe
    PRIVATE
        src/CafeData.cpp
        src/Cafe.cpp
        src/Career.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Cafe.hpp
            include/Career.hpp
            include/Cafe.md
)

target_compile_features(UmbraCafe PUBLIC cxx_std_20)

# The cafe between matches: a shift's rules and the career, no renderer or window (Cafe.md).
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(
    UmbraCafe
    PUBLIC
        Umbra::Core
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Night
        Umbra::Warnings
        Umbra::Sanitizers
)
