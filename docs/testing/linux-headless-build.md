# Linux build for headless verification

Use this when the Windows toolchain isn't available, e.g. in a cloud agent session. It builds the **real** root `CMakeLists.txt` on Linux and runs every CTest. It is not a replacement for the Windows gate: no MSVC, no DXC, no GPU.

## Dependencies

vcpkg archive downloads may be blocked, but `git clone` usually works. Build each dependency from source into one prefix (e.g. `$HOME/prefix`) with `cmake -S <src> -B <build> -DCMAKE_INSTALL_PREFIX=$HOME/prefix && cmake --build <build> && cmake --install <build>`:

| Library | Tag used 2026-11-01 | Notes |
|---|---|---|
| Catch2 | v3.15.3 | |
| spdlog | v1.15.3 | |
| SDL | release-3.2.24 | run tests with `SDL_VIDEO_DRIVER=dummy` |
| glm | 1.0.3 | `-DGLM_BUILD_TESTS=OFF` |
| nlohmann/json | v3.12.0 | `-DJSON_BuildTests=OFF` |
| tinygltf | v3.0.0 | copy `tiny_gltf.h`, `json.hpp`, `stb_image.h`, `stb_image_write.h` into `prefix/include` |
| Vulkan-Headers, Vulkan-Utility-Libraries | v1.4.328 | the second one provides `vk_enum_string_helper.h` |
| volk | vulkan-sdk-1.4.328.0 | `-DVOLK_INSTALL=ON` |
| VulkanMemoryAllocator | v3.3.0 | |
| imgui | v1.92.4 | no CMake of its own: a 10-line wrapper that builds the core `.cpp` files and exports `imgui::imgui` |
| glslang | 15.4.0 | `-DENABLE_OPT=OFF`; stands in for DXC |
| cpp-httplib | v0.53.0 (the vcpkg baseline's version) | header-only; configure with `-DHTTPLIB_USE_OPENSSL_IF_AVAILABLE=OFF -DHTTPLIB_USE_ZLIB_IF_AVAILABLE=OFF -DHTTPLIB_USE_BROTLI_IF_AVAILABLE=OFF -DHTTPLIB_USE_ZSTD_IF_AVAILABLE=OFF` and install |
| JoltPhysics | v5.6.0 | build from `Build/` in **Debug** (same config as the engine, so Jolt's feature defines match), with the vcpkg port's options: `-DTARGET_UNIT_TESTS=OFF -DTARGET_HELLO_WORLD=OFF -DTARGET_PERFORMANCE_TEST=OFF -DTARGET_SAMPLES=OFF -DTARGET_VIEWER=OFF -DCROSS_PLATFORM_DETERMINISTIC=OFF -DINTERPROCEDURAL_OPTIMIZATION=OFF -DDEBUG_RENDERER_IN_DEBUG_AND_RELEASE=OFF -DPROFILER_IN_DEBUG_AND_RELEASE=OFF -DCPP_RTTI_ENABLED=OFF` |

## Fake Vulkan SDK

`engine/render/CMakeLists.txt` looks for `dxc`, `spirv-val` and `vk_enum_string_helper.h` under `$VULKAN_SDK`. Make a folder with:

- `Include` and `include` → symlinks to `prefix/include`
- `Bin/dxc` → a script that maps `-T vs_*/ps_*`, `-E`, `-Fo` onto `glslang -D -V --target-env vulkan1.3 -e <entry> -S vert|frag <file> -o <out>`
- `Bin/spirv-val` → a script that exits 0

## Build and test

```bash
export VULKAN_SDK=$HOME/fakesdk LD_LIBRARY_PATH=$HOME/prefix/lib SDL_VIDEO_DRIVER=dummy
cmake -S . -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/prefix -DBUILD_TESTING=ON
cmake --build build-gcc && ctest --test-dir build-gcc --output-on-failure
```

Also run it with `-DCMAKE_CXX_COMPILER=clang++`, and with GCC plus `CXXFLAGS="-fsanitize=address,undefined -fno-sanitize=vptr" LDFLAGS=-fsanitize=address,undefined` set for the first configure. `-fno-sanitize=vptr` is needed because Jolt is built without RTTI (like the vcpkg port) and UBSan's vptr check requires it. Clang's ASan runtime may be missing, which is why GCC is used for the sanitizer run. `UMBRA_ENABLE_ASAN` only supports MSVC and Clang.

GCC prints many `-Wmissing-field-initializers` warnings for the existing `VkXxxCreateInfo{sType}` pattern. They are pre-existing and harmless; filter them when looking for new warnings.

## Optimized measurements

Jolt is built Debug only, so an `NDEBUG` build of the engine against it crashes (Jolt's assert-dependent class layouts differ). For the `[benchmark]` test use a custom configuration that optimizes without `NDEBUG`:

```bash
cmake -S . -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Bench -DCMAKE_CXX_FLAGS_BENCH="-O2 -UNDEBUG" -DCMAKE_MAP_IMPORTED_CONFIG_BENCH="Debug;Release;" -DCMAKE_PREFIX_PATH=$HOME/prefix -DBUILD_TESTING=ON
cmake --build build-rel --target UmbraGameplayTests && build-rel/tests/gameplay/UmbraGameplayTests "[benchmark]" -s
```
