#include <ModelAssistant.hpp>

#include <GltfImporter.hpp>
#include <Material.hpp>
#include <TextureTools.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <ctime>
#include <fstream>
#include <set>

namespace umbra::localai
{
    namespace
    {
        bool WriteBytes(const std::filesystem::path &file, const std::string &bytes)
        {
            std::error_code error;
            std::filesystem::create_directories(file.parent_path(), error);
            const std::filesystem::path temporary = file.string() + ".tmp";
            {
                std::ofstream out(temporary, std::ios::binary);
                if (!out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
                    return false;
            }
            std::filesystem::rename(temporary, file, error);
            return !error;
        }

        float Round(float value)
        {
            return std::round(value * 1000.0F) / 1000.0F;
        }
    }

    TextureStyle LoadObjectStyle(const std::filesystem::path &assetRoot)
    {
        TextureStyle style{"single game prop, centered, whole object in view, plain white background, soft even lighting, stylized hand-painted game art",
                           "text, watermark, cropped, cut off, multiple objects, people, busy background, floor, shadow, blurry, photo"};
        std::ifstream in(assetRoot / "materials/style.json");
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_object())
        {
            style.prompt = j.value("objectPrompt", style.prompt);
            style.negative = j.value("objectNegative", style.negative);
        }
        return style;
    }

    std::vector<std::filesystem::path> FindModelWorkflows(const std::filesystem::path &folder)
    {
        std::vector<std::filesystem::path> found;
        std::error_code error;
        for (const auto &entry : std::filesystem::directory_iterator(folder, error))
        {
            nlohmann::json graph;
            std::vector<core::Diagnostic> ignored;
            if (entry.path().extension() == ".json" && LoadModelWorkflow(entry.path(), graph, ignored))
                found.push_back(entry.path());
        }
        std::sort(found.begin(), found.end());
        return found;
    }

    std::filesystem::path DefaultModelWorkflow(const std::filesystem::path &assetRoot)
    {
        const std::filesystem::path folder = assetRoot.parent_path() / "tools/comfyui";
        const std::vector<std::filesystem::path> found = FindModelWorkflows(folder);
        for (const auto &file : found)
            if (file.filename() == "image-to-3d.api.json")
                return file;
        return found.empty() ? folder / "image-to-3d.api.json" : found.front();
    }

    bool InspectModel(const std::filesystem::path &file, ModelInfo &info, std::vector<core::Diagnostic> &diagnostics)
    {
        const assets::ImportResult imported = assets::ImportGltf(file);
        for (const std::string &error : imported.errors)
            diagnostics.push_back({core::Severity::Error, "model.import", file.string(), "", error});
        if (!imported.Ok())
            return false;
        const assets::Model &model = imported.model;
        info = {};
        for (const auto &mesh : model.meshes)
            info.triangles += mesh.data.indices.size() / 3;
        info.meshes = model.meshes.size();
        for (const auto &texture : model.textures)
            info.textureSize = std::max({info.textureSize, texture.width, texture.height});

        // Bounds in model space: each node's world matrix (parents come first) applied to its meshes.
        std::vector<glm::mat4> world(model.nodes.size(), glm::mat4(1.0F));
        bool first = true;
        for (std::size_t i = 0; i < model.nodes.size(); ++i)
        {
            const assets::ModelNode &node = model.nodes[i];
            const glm::mat4 local = glm::translate(glm::mat4(1.0F), node.position) * glm::mat4_cast(node.rotation) * glm::scale(glm::mat4(1.0F), node.scale);
            world[i] = node.parent >= 0 ? world[static_cast<std::size_t>(node.parent)] * local : local;
            for (const std::uint32_t m : node.meshes)
                for (const core::Vertex &v : model.meshes[m].data.vertices)
                {
                    const glm::vec3 p = glm::vec3(world[i] * glm::vec4(v.position, 1.0F));
                    info.min = first ? p : glm::min(info.min, p);
                    info.max = first ? p : glm::max(info.max, p);
                    first = false;
                }
        }
        return true;
    }

    std::string SaveGeneratedModel(const std::filesystem::path &assetRoot, const std::string &name, const std::vector<std::uint8_t> &glb,
                                   const core::TextureData &picture, const ModelSource &source, const nlohmann::json &workflow,
                                   std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. A free name for all three files.
        const std::string base = TextureName(name);
        std::string free = base;
        for (int n = 2; std::filesystem::exists(assetRoot / ("models/" + free + ".glb")); ++n)
            free = base + "_" + std::to_string(n);
        const std::string modelPath = "models/" + free + ".glb";

        // 2. Provenance: the 3D models the workflow loaded, with what is known about their licenses.
        nlohmann::ordered_json models = nlohmann::ordered_json::array();
        auto isModelFile = [](const std::string &file) {
            for (const char *extension : {".safetensors", ".ckpt", ".pt", ".pth", ".bin", ".gguf"})
                if (file.ends_with(extension))
                    return true;
            return false;
        };
        for (const auto &[id, node] : workflow.items())
        {
            const nlohmann::json inputs = node.value("inputs", nlohmann::json::object()); // a copy: items() must not outlive it
            if (node.value("class_type", "") != "LoadImage")
                for (const auto &[key, value] : inputs.items())
                    if (value.is_string() && isModelFile(value.get<std::string>()))
                    {
                        const assets::ModelLicenseInfo license = assets::ModelLicense(value.get<std::string>());
                        models.push_back({{"file", value}, {"license", license.name}, {"commercial", license.known && license.commercial}});
                    }
        }
        const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char date[16];
        std::strftime(date, sizeof(date), "%Y-%m-%d", std::localtime(&now));
        const nlohmann::ordered_json provenance{{"provider", "comfyui"}, {"workflow", source.workflow}, {"picture", source.picture},
                                                {"seed", source.seed}, {"faces", source.faces}, {"models", models}, {"date", date}};

        // 3. Files, then the importer's verdict; a model the game can't read is removed again.
        std::vector<std::uint8_t> png;
        const bool havePicture = !picture.rgba.empty() && assets::EncodePng(picture, png); // none when fetched by job id
        if (!WriteBytes(assetRoot / modelPath, std::string(glb.begin(), glb.end())) ||
            (havePicture && !WriteBytes(assetRoot / ("models/" + free + ".concept.png"), std::string(png.begin(), png.end()))) ||
            !WriteBytes(assetRoot / ("models/" + free + ".source.json"), provenance.dump(2) + "\n"))
        {
            diagnostics.push_back({core::Severity::Error, "save.write", (assetRoot / modelPath).string(), "", "couldn't write the model files"});
            return "";
        }
        ModelInfo info;
        if (!InspectModel(assetRoot / modelPath, info, diagnostics))
        {
            std::error_code ignored;
            for (const char *suffix : {".glb", ".concept.png", ".source.json"})
                std::filesystem::remove(assetRoot / ("models/" + free + suffix), ignored);
            return "";
        }
        for (const auto &model : models)
            if (!model["commercial"].get<bool>())
                diagnostics.push_back({core::Severity::Warning, "material.license", modelPath, "",
                                       "made with " + model["file"].get<std::string>() + " (" + model["license"].get<std::string>() + "): check before selling"});
        return modelPath;
    }

    nlohmann::ordered_json PlacedModelEntity(const nlohmann::ordered_json &scene, const std::string &name, const std::string &modelPath,
                                             const ModelInfo &info, glm::vec3 at, float height)
    {
        // 1. A free id.
        std::set<std::string> ids;
        for (const auto &entity : scene.value("entities", nlohmann::ordered_json::array()))
            ids.insert(entity.value("id", ""));
        const std::string base = TextureName(name);
        std::string id = base;
        for (int n = 2; ids.contains(id); ++n)
            id = base + "_" + std::to_string(n);

        // 2. Scale to the asked height; lift so the lowest point touches the ground.
        const float modelHeight = std::max(info.max.y - info.min.y, 0.001F);
        const float scale = height / modelHeight;
        nlohmann::ordered_json entity{{"id", id}, {"name", name},
                                      {"position", {Round(at.x), Round(at.y - info.min.y * scale), Round(at.z)}},
                                      {"scale", {Round(scale), Round(scale), Round(scale)}}};
        if (info.meshes == 1)
        {
            entity["model"] = modelPath;
            entity["mesh"] = 0;
        }
        else
            entity["instance"] = modelPath;
        return entity;
    }
}
