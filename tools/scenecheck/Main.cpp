// UmbraSceneCheck: validates scene files without a window, GPU or editor.
// usage: UmbraSceneCheck <scene.json>... [--assets <asset folder>]
// exit code: 0 = no errors, 1 = errors found, 2 = bad usage

#include <GltfImporter.hpp>
#include <SceneFile.hpp>

#include <iostream>
#include <map>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    // 1. Arguments.
    std::vector<std::filesystem::path> scenes;
    umbra::scene::SceneLoadOptions options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--assets" && i + 1 < argc)
            options.assetRoot = argv[++i];
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
        std::cerr << "usage: UmbraSceneCheck <scene.json>... [--assets <asset folder>]\n";
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

    // 3. Validate and report.
    std::vector<umbra::core::Diagnostic> diagnostics;
    for (const auto &scene : scenes)
        umbra::scene::ValidateScene(scene, options, diagnostics);
    int errors = 0, warnings = 0;
    for (const auto &d : diagnostics)
    {
        std::cout << umbra::core::Format(d) << "\n";
        (d.severity == umbra::core::Severity::Error ? errors : warnings)++;
    }
    std::cout << scenes.size() << " scene(s): " << errors << " error(s), " << warnings << " warning(s)\n";
    return errors > 0 ? 1 : 0;
}
