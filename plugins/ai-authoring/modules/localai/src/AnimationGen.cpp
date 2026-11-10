#include <AnimationGen.hpp>

#include <ModelTools.hpp>
#include <TextureAssistant.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <random>

namespace umbra::localai
{
    namespace
    {
        bool WriteFile(const std::filesystem::path &file, const std::string &bytes)
        {
            std::error_code error;
            std::filesystem::create_directories(file.parent_path(), error);
            std::ofstream out(file, std::ios::binary);
            return static_cast<bool>(out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())));
        }

        // Blender's program: UMBRA_BLENDER, the newest one in the usual Windows folder, else "blender".
        std::string FindBlender()
        {
            if (const char *set = std::getenv("UMBRA_BLENDER"); set != nullptr && *set != '\0')
                return set;
#ifdef _WIN32
            std::vector<std::filesystem::path> found;
            std::error_code error;
            for (const auto &entry : std::filesystem::directory_iterator("C:/Program Files/Blender Foundation", error))
                if (std::filesystem::exists(entry.path() / "blender.exe"))
                    found.push_back(entry.path() / "blender.exe");
            std::sort(found.begin(), found.end());
            if (!found.empty())
                return found.back().string();
#endif
            return "blender";
        }
    }

    std::vector<std::filesystem::path> FindAnimWorkflows(const std::filesystem::path &folder, AnimKind kind)
    {
        std::vector<std::filesystem::path> found;
        std::error_code error;
        for (const auto &entry : std::filesystem::directory_iterator(folder, error))
        {
            nlohmann::json graph;
            std::vector<core::Diagnostic> ignored;
            if (entry.path().extension() == ".json" && LoadAnimWorkflow(entry.path(), kind, graph, ignored))
                found.push_back(entry.path());
        }
        std::sort(found.begin(), found.end());
        return found;
    }

    std::filesystem::path DefaultAnimWorkflow(const std::filesystem::path &assetRoot, AnimKind kind)
    {
        // A file named for its kind wins; a motion workflow can also pass the rig check, so the
        // first-found fallback skips the other kind's default file.
        const std::filesystem::path folder = assetRoot.parent_path() / "tools/comfyui";
        const std::string mine = kind == AnimKind::Rig ? "rig.api.json" : "motion.api.json";
        const std::string other = kind == AnimKind::Rig ? "motion.api.json" : "rig.api.json";
        const std::vector<std::filesystem::path> found = FindAnimWorkflows(folder, kind);
        for (const auto &file : found)
            if (file.filename() == mine)
                return file;
        for (const auto &file : found)
            if (file.filename() != other)
                return file;
        return folder / mine;
    }

    bool FbxToGlb(const std::vector<std::uint8_t> &fbx, std::vector<std::uint8_t> &glb, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. A private folder with the FBX and a small Blender script (no quoting of paths on the command line).
        std::random_device random;
        const std::filesystem::path folder = std::filesystem::temp_directory_path() / ("umbra-fbx-" + std::to_string(random()));
        const auto in = folder / "in.fbx", out = folder / "out.glb", script = folder / "convert.py";
        const std::string code = "import bpy\n"
                                 "bpy.ops.wm.read_factory_settings(use_empty=True)\n"
                                 "bpy.ops.import_scene.fbx(filepath=r'" + in.generic_string() + "')\n"
                                 "bpy.ops.export_scene.gltf(filepath=r'" + out.generic_string() + "', export_format='GLB', export_animations=True)\n";
        auto done = [&](bool ok, const std::string &message) {
            std::error_code ignored;
            std::filesystem::remove_all(folder, ignored);
            if (!ok)
                diagnostics.push_back({core::Severity::Error, "ai.convert", "blender", "", message});
            return ok;
        };
        if (!WriteFile(in, std::string(fbx.begin(), fbx.end())) || !WriteFile(script, code))
            return done(false, "couldn't write a temporary file for the FBX conversion");

        // 2. Blender in the background; its output is ignored, the GLB file is the answer.
        std::string command = "\"" + FindBlender() + "\" -b --factory-startup --python \"" + script.string() + "\"";
#ifdef _WIN32
        command = "\"" + command + "\" >NUL 2>&1"; // cmd /c drops the outer quotes
#else
        command += " >/dev/null 2>&1";
#endif
        const int status = std::system(command.c_str());
        std::ifstream result(out, std::ios::binary);
        if (status != 0 || !result)
            return done(false, "ComfyUI saved an FBX file and Blender couldn't convert it to GLB (install Blender, or set UMBRA_BLENDER "
                               "to blender.exe; or end the workflow with a node that saves .glb)");
        glb.assign(std::istreambuf_iterator<char>(result), {});
        result.close();
        return done(!glb.empty(), "Blender wrote an empty GLB file");
    }

    std::string SaveAnimResult(const std::filesystem::path &assetRoot, AnimKind kind, const std::string &name, const std::vector<std::uint8_t> &glb,
                               const nlohmann::ordered_json &source, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. A free name: models/<name>.glb for a rig, animations/<name>.glb for a motion.
        const std::string folder = kind == AnimKind::Rig ? "models/" : "animations/";
        const std::string base = TextureName(name);
        std::string free = base;
        for (int n = 2; std::filesystem::exists(assetRoot / (folder + free + ".glb")); ++n)
            free = base + "_" + std::to_string(n);
        const std::string path = folder + free + ".glb";
        const std::filesystem::path sourceFile = assetRoot / (folder + free + ".source.json");

        // 2. The files, with the date in the provenance.
        const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char date[16];
        std::strftime(date, sizeof(date), "%Y-%m-%d", std::localtime(&now));
        nlohmann::ordered_json provenance = source;
        provenance["provider"] = "comfyui";
        provenance["date"] = date;
        if (!WriteFile(assetRoot / path, std::string(glb.begin(), glb.end())) || !WriteFile(sourceFile, provenance.dump(2) + "\n"))
        {
            diagnostics.push_back({core::Severity::Error, "save.write", (assetRoot / path).string(), "", "couldn't write the files"});
            return "";
        }

        // 3. The importer's verdict: a rig needs a skeleton, a motion a clip.
        authoring::ModelInfo info;
        std::string problem;
        if (!authoring::InspectModel(assetRoot / path, info, diagnostics))
            problem = "the game can't read it";
        else if (info.joints == 0)
            problem = "it has no skeleton";
        else if (kind == AnimKind::Motion && info.clips.empty())
            problem = "it has no animation";
        if (problem.empty())
            return path;
        diagnostics.push_back({core::Severity::Error, "model.rig", path, "", "ComfyUI's result was not kept: " + problem});
        std::error_code ignored;
        std::filesystem::remove(assetRoot / path, ignored);
        std::filesystem::remove(sourceFile, ignored);
        return "";
    }
}
