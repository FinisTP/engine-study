#include <Validate.hpp>

#include <GltfImporter.hpp>
#include <Material.hpp>
#include <RpgContent.hpp>
#include <SaveGame.hpp>
#include <SceneFile.hpp>

#include <fstream>
#include <map>
#include <set>

namespace umbra::authoring
{
    namespace
    {
        using core::Diagnostic;
        using core::Severity;

        // Loads the scene into a throwaway world, then imports every model it draws.
        bool ValidateScene(const std::filesystem::path &file, const assets::AssetFolders &assetRoot,
                           std::vector<Diagnostic> &diagnostics)
        {
            std::map<std::string, assets::ImportResult> imported;
            const auto import = [&](const std::string &path) -> const assets::ImportResult & {
                auto it = imported.find(path);
                if (it == imported.end())
                    it = imported.emplace(path, assets::ImportGltf(assetRoot / path)).first;
                return it->second;
            };
            scene::SceneLoadOptions options;
            options.assetRoot = assetRoot;
            options.models = [&](const std::string &path) -> const assets::Model * {
                const assets::ImportResult &result = import(path);
                return result.Ok() ? &result.model : nullptr;
            };
            world::World world;
            if (!scene::LoadScene(file, options, world, diagnostics))
                return false;

            std::set<std::string> models;
            world.ForEach([&](world::Entity entity) {
                if (const world::Renderable *r = world.GetRenderable(entity))
                    models.insert(r->model);
            });
            bool ok = true;
            for (const std::string &model : models)
            {
                const assets::ImportResult &result = import(model);
                for (const std::string &error : result.errors)
                    diagnostics.push_back({Severity::Error, "model.import", (assetRoot / model).string(), "", error});
                for (const std::string &warning : result.warnings)
                    diagnostics.push_back({Severity::Warning, "model.import", (assetRoot / model).string(), "", warning});
                ok &= result.errors.empty();
            }
            return ok;
        }
    }

    FileKind DetectKind(const std::filesystem::path &file)
    {
        std::ifstream in(file, std::ios::binary);
        const nlohmann::json root = nlohmann::json::parse(in, nullptr, false);
        if (root.is_discarded() || !root.is_object())
            return FileKind::Unknown;
        const std::string format = root.value("format", "");
        if (format == "umbra.scene")
            return FileKind::Scene;
        if (format == "umbra.rpg")
            return FileKind::Rpg;
        if (format == "umbra.save")
            return FileKind::Save;
        if (format == "umbra.material")
            return FileKind::Material;
        return FileKind::Unknown;
    }

    const char *ToString(FileKind kind)
    {
        switch (kind)
        {
        case FileKind::Scene:
            return "scene";
        case FileKind::Rpg:
            return "rpg";
        case FileKind::Save:
            return "save";
        case FileKind::Material:
            return "material";
        case FileKind::Unknown:
            break;
        }
        return "unknown";
    }

    bool ValidateFile(const std::filesystem::path &file, const ValidateOptions &options, std::vector<Diagnostic> &diagnostics)
    {
        switch (DetectKind(file))
        {
        case FileKind::Scene:
            return ValidateScene(file, options.assetRoot, diagnostics);
        case FileKind::Rpg:
        {
            rpg::Content content;
            return rpg::LoadContent(file, content, diagnostics);
        }
        case FileKind::Save:
        {
            // A save is checked against the content it was made with.
            rpg::Content content;
            if (options.content.empty())
            {
                diagnostics.push_back({Severity::Error, "save.content.missing", file.string(), "", "checking a save needs --content <rpg file>"});
                return false;
            }
            if (!rpg::LoadContent(options.content, content, diagnostics))
                return false;
            game::SaveData data;
            return game::ReadSave(file, content, data, diagnostics);
        }
        case FileKind::Material:
        {
            // The image (under --assets, else two folders up: assets/materials/x -> assets) and the license.
            assets::Material material;
            const assets::AssetFolders root = options.assetRoot.empty() ? assets::AssetFolders(file.parent_path().parent_path()) : options.assetRoot;
            if (!assets::LoadMaterial(file, material, diagnostics) || !assets::CheckMaterialImage(root, material, file.string(), diagnostics))
                return false;
            assets::CheckMaterialLicense(material, file.string(), diagnostics);
            return true;
        }
        case FileKind::Unknown:
            break;
        }
        diagnostics.push_back({Severity::Error, "file.format", file.string(), "format",
                               "not an Umbra content file (expected \"format\": umbra.scene, umbra.rpg, umbra.save or umbra.material)"});
        return false;
    }

    nlohmann::json ToJson(const std::vector<Diagnostic> &diagnostics)
    {
        nlohmann::json list = nlohmann::json::array();
        for (const Diagnostic &d : diagnostics)
            list.push_back({{"severity", d.severity == Severity::Error ? "error" : "warning"},
                            {"code", d.code},
                            {"file", d.file},
                            {"field", d.field},
                            {"message", d.message}});
        return list;
    }
}
