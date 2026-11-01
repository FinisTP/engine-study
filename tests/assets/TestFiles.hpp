#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace test
{
    // Fresh empty folder under the system temp folder, unique per name.
    inline std::filesystem::path TempDir(const std::string &name)
    {
        const auto dir = std::filesystem::temp_directory_path() / ("umbra-tests-" + name);
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        return dir;
    }

    inline void WriteText(const std::filesystem::path &path, const std::string &text)
    {
        std::ofstream(path, std::ios::binary) << text;
    }

    inline std::string ReadText(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), {}};
    }

    // Writes tri.bin: 3 float3 positions (36 bytes) + 3 uint16 indices (6 bytes) + 2 padding bytes.
    inline void WriteTriangleBin(const std::filesystem::path &path)
    {
        const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        const unsigned short indices[4] = {0, 1, 2, 0};
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(positions), sizeof(positions));
        file.write(reinterpret_cast<const char *>(indices), sizeof(indices));
    }

    struct TriangleGltf
    {
        std::string nodes = R"([{"name":"Tri","mesh":0}])";
        std::string uri = "tri.bin";
        int positionCount = 3;
        std::string indices = R"("indices":1,)";
        std::string extra; // extra top-level members, each followed by a comma
    };

    // Writes a minimal one-triangle glTF that reads tri.bin.
    inline std::string TriangleJson(const TriangleGltf &g)
    {
        return R"({"asset":{"version":"2.0"},)" + g.extra + R"("scenes":[{"nodes":[0]}],"nodes":)" + g.nodes +
               R"(,"meshes":[{"primitives":[{)" + g.indices + R"("attributes":{"POSITION":0}}]}],)" +
               R"("buffers":[{"byteLength":44,"uri":")" + g.uri + R"("}],)" +
               R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)" +
               R"("accessors":[{"bufferView":0,"componentType":5126,"count":)" + std::to_string(g.positionCount) +
               R"(,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}]})";
    }
}
