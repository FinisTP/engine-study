# Developer-only training bridge (roadmap P8A.2): a shared library Python loads with ctypes, so
# training steps run the engine's real turn rules. The game and the editor never link it.
add_library(UmbraPolicyEnv SHARED PolicyEnvApi.cpp)
target_compile_features(UmbraPolicyEnv PRIVATE cxx_std_20)
target_link_libraries(UmbraPolicyEnv PRIVATE Umbra::Policy Umbra::Warnings)
set_target_properties(UmbraPolicyEnv PROPERTIES CXX_VISIBILITY_PRESET hidden)
