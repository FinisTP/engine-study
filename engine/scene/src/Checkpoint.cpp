#include <Checkpoint.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>

namespace umbra::scene
{
    using nlohmann::json;

    bool SaveCheckpoint(const world::World &world, const Checkpoint &checkpoint, const std::filesystem::path &directory,
                        std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. Player and area first: a checkpoint.json without a world file is caught on load.
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        const json info{{"format", "umbra.checkpoint"},
                        {"version", 1},
                        {"scene", checkpoint.scene},
                        {"feet", {checkpoint.feet.x, checkpoint.feet.y, checkpoint.feet.z}},
                        {"yaw", checkpoint.yaw}};
        std::ofstream file(directory / "checkpoint.json", std::ios::binary | std::ios::trunc);
        file << info.dump(2) << '\n';
        if (!file.flush())
        {
            diagnostics.push_back({core::Severity::Error, "checkpoint.write", (directory / "checkpoint.json").string(), "", "cannot write file"});
            return false;
        }

        // 2. The world, through the normal scene writer (atomic).
        return SaveScene(world, directory / "world.scene.json", diagnostics);
    }

    bool LoadCheckpoint(const std::filesystem::path &directory, const SceneLoadOptions &options, world::World &world,
                        Checkpoint &checkpoint, std::vector<core::Diagnostic> &diagnostics)
    {
        const std::string path = (directory / "checkpoint.json").string();
        const auto fail = [&](const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "checkpoint.read", path, "", message});
            return false;
        };

        // 1. checkpoint.json: header, area path, finite position and yaw.
        std::ifstream file(directory / "checkpoint.json", std::ios::binary);
        if (!file)
            return fail("no checkpoint saved yet");
        const json info = json::parse(file, nullptr, false);
        if (info.is_discarded() || !info.is_object() || info.value("format", "") != "umbra.checkpoint" || info.value("version", 0) != 1)
            return fail("not a version 1 umbra.checkpoint file");
        const auto scene = info.find("scene");
        const auto feet = info.find("feet");
        const auto yaw = info.find("yaw");
        if (scene == info.end() || !scene->is_string() || feet == info.end() || !feet->is_array() || feet->size() != 3 ||
            yaw == info.end() || !yaw->is_number())
            return fail("needs \"scene\", \"feet\" [x, y, z] and \"yaw\"");
        Checkpoint loaded{scene->get<std::string>(), {}, yaw->get<float>()};
        for (int i = 0; i < 3; ++i)
        {
            if (!(*feet)[i].is_number())
                return fail("\"feet\" must hold numbers");
            loaded.feet[i] = (*feet)[i].get<float>();
        }
        if (!std::isfinite(loaded.yaw) || !std::isfinite(loaded.feet.x) || !std::isfinite(loaded.feet.y) || !std::isfinite(loaded.feet.z))
            return fail("position or yaw is not finite");

        // 2. The world file (validated like any scene). Only now is anything handed back.
        if (!LoadScene(directory / "world.scene.json", options, world, diagnostics))
            return false;
        checkpoint = loaded;
        return true;
    }
}
