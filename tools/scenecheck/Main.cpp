// UmbraSceneCheck: validates scene files and the models they use, without a window, GPU or editor.
// usage: UmbraSceneCheck <scene.json>... [--assets <asset folder>] [--json]
// exit code: 0 = no errors, 1 = errors found, 2 = bad usage
// --json prints one JSON object (see engine/scene/include/SceneFormat.md, "Diagnostics").

#include <GltfImporter.hpp>
#include <SceneFile.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
    using umbra::core::Diagnostic;
    using umbra::core::Severity;

    // Imports every model a loaded scene draws and turns importer problems into diagnostics.
    void CheckModels(const umbra::world::World &world,
                     std::map<std::string, umbra::assets::ImportResult> &imported, const std::filesystem::path &assetRoot,
                     std::vector<Diagnostic> &diagnostics)
    {
        std::set<std::string> models;
        world.ForEach([&](umbra::world::Entity entity) {
            if (const umbra::world::Renderable *r = world.GetRenderable(entity))
                models.insert(r->model);
        });
        for (const std::string &model : models)
        {
            auto it = imported.find(model);
            if (it == imported.end())
                it = imported.emplace(model, umbra::assets::ImportGltf(assetRoot / model)).first;
            for (const std::string &error : it->second.errors)
                diagnostics.push_back({Severity::Error, "model.import", (assetRoot / model).string(), "", error});
            for (const std::string &warning : it->second.warnings)
                diagnostics.push_back({Severity::Warning, "model.import", (assetRoot / model).string(), "", warning});
        }
    }
}

int main(int argc, char **argv)
{
    // 1. Arguments.
    std::vector<std::filesystem::path> scenes;
    umbra::scene::SceneLoadOptions options;
    bool json = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--assets" && i + 1 < argc)
            options.assetRoot = argv[++i];
        else if (arg == "--json")
            json = true;
        else if (!arg.empty() && arg[0] != '-')
            scenes.emplace_back(arg);
        else
        {
            std::cerr << "unknown option " << arg << "\n";
            return 2;
        }
    }
    if (scenes.empty())
    {
        std::cerr << "usage: UmbraSceneCheck <scene.json>... [--assets <asset folder>] [--json]\n";
        return 2;
    }

    // 2. Instance models are imported straight from glTF (no cache needed for checking).
    std::map<std::string, umbra::assets::ImportResult> imported;
    options.models = [&](const std::string &path) -> const umbra::assets::Model * {
        auto it = imported.find(path);
        if (it == imported.end())
            it = imported.emplace(path, umbra::assets::ImportGltf(options.assetRoot / path)).first;
        return it->second.Ok() ? &it->second.model : nullptr;
    };

    // 3. Validate each scene by loading it into a throwaway world, then check its models.
    std::vector<Diagnostic> diagnostics;
    for (const auto &scene : scenes)
    {
        umbra::world::World world;
        if (umbra::scene::LoadScene(scene, options, world, diagnostics))
            CheckModels(world, imported, options.assetRoot, diagnostics);
    }

    // 4. Report.
    int errors = 0, warnings = 0;
    for (const auto &d : diagnostics)
        (d.severity == Severity::Error ? errors : warnings)++;
    if (json)
    {
        nlohmann::json out{{"scenes", scenes.size()}, {"errors", errors}, {"warnings", warnings}, {"diagnostics", nlohmann::json::array()}};
        for (const auto &d : diagnostics)
            out["diagnostics"].push_back({{"severity", d.severity == Severity::Error ? "error" : "warning"},
                                          {"code", d.code},
                                          {"file", d.file},
                                          {"field", d.field},
                                          {"message", d.message}});
        std::cout << out.dump(2) << "\n";
    }
    else
    {
        for (const auto &d : diagnostics)
            std::cout << umbra::core::Format(d) << "\n";
        std::cout << scenes.size() << " scene(s): " << errors << " error(s), " << warnings << " warning(s)\n";
    }
    return errors > 0 ? 1 : 0;
}
