#pragma once

#include <Diagnostic.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace umbra::scene
{
    // A project's own component type, described by "<assets>/components/<type>.component.json"
    // (see Components.md). The editor draws its fields from this; the scene loader checks them.
    struct ComponentField final
    {
        std::string name;
        std::string kind = "number"; // number, int, bool, string, asset, actor, scene
        std::string label;           // shown in the editor (default: name)
        nlohmann::json defaultValue;
        double min = -1e9, max = 1e9; // number and int
        std::string folder;           // asset: only files under this asset folder
    };

    struct ComponentSchema final
    {
        std::string type;  // the key in an entity's "components"
        std::string label; // shown in the editor's "Add component" menu
        std::string description;
        std::vector<ComponentField> fields;
        std::filesystem::path file;

        // An object with every field at its default (what "Add component" puts in the scene).
        [[nodiscard]] nlohmann::json Defaults() const;
    };

    // The names of the built-in components (handled by the engine itself, no schema file).
    inline const std::vector<std::string> builtInComponents{"collider", "interactable", "portal", "spawner"};

    // Reads every *.component.json in `folder` (no folder = none). Bad files are reported and skipped.
    std::map<std::string, ComponentSchema> LoadComponentSchemas(const std::filesystem::path &folder, std::vector<core::Diagnostic> &diagnostics);

    // Checks one component's fields against its schema; `where` names it in diagnostics.
    void CheckComponent(const ComponentSchema &schema, const nlohmann::json &value, const std::string &file, const std::string &where,
                        std::vector<core::Diagnostic> &diagnostics);
}
