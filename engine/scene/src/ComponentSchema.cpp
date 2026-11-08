#include <ComponentSchema.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>

namespace umbra::scene
{
    namespace
    {
        using nlohmann::json;
        const std::set<std::string> kinds{"number", "int", "bool", "string", "asset", "actor", "scene", "color"};

        // The default a field gets when the schema gives none.
        json ZeroOf(const std::string &kind)
        {
            if (kind == "number" || kind == "int")
                return 0;
            if (kind == "bool")
                return false;
            if (kind == "color")
                return json::array({1.0, 1.0, 1.0});
            return "";
        }

        std::string Number(double v)
        {
            std::string text = std::to_string(v);
            text.erase(text.find_last_not_of('0') + 1);
            if (text.back() == '.')
                text.pop_back();
            return text;
        }

        ComponentField Field(std::string name, std::string kind, std::string label, json value, double min = 0.0, double max = 1.0)
        {
            ComponentField f;
            f.name = std::move(name);
            f.kind = std::move(kind);
            f.label = std::move(label);
            f.defaultValue = std::move(value);
            f.min = min;
            f.max = max;
            return f;
        }
    }

    const std::map<std::string, ComponentSchema> &EngineComponentSchemas()
    {
        static const std::map<std::string, ComponentSchema> schemas = [] {
            std::map<std::string, ComponentSchema> out;
            ComponentSchema light;
            light.type = "light";
            light.label = "Light";
            light.description = "A point light (torch, fire, crystal) at this entity. It fades to nothing at its range.";
            light.fields = {Field("color", "color", "Color", json::array({1.0, 0.75, 0.45}), 0.0, 1.0),
                            Field("intensity", "number", "Intensity", 3.0, 0.0, 50.0),
                            Field("range", "number", "Range (m)", 8.0, 0.1, 100.0),
                            Field("flicker", "number", "Flicker (fire)", 0.0, 0.0, 1.0)};
            light.file = "engine: light";
            out["light"] = light;
            ComponentSchema environment;
            environment.type = "environment";
            environment.label = "Environment";
            environment.description = "The scene's sun, sky and fog, and the time of day. One per scene.";
            environment.fields = {Field("hour", "number", "Hour (0-24)", 14.0, 0.0, 24.0),
                                  Field("dayLength", "number", "Day length (s, 0 = still)", 0.0, 0.0, 86400.0),
                                  Field("sunHeading", "number", "Sun heading (deg)", 30.0, 0.0, 360.0),
                                  Field("indoor", "bool", "Indoor (no sun or sky)", false),
                                  Field("ambient", "number", "Ambient", 1.0, 0.0, 4.0),
                                  Field("fog", "number", "Fog", 0.3, 0.0, 1.0),
                                  Field("tint", "color", "Tint", json::array({1.0, 1.0, 1.0}), 0.0, 2.0),
                                  Field("shadows", "bool", "Sun shadows", true)};
            environment.file = "engine: environment";
            out["environment"] = environment;
            return out;
        }();
        return schemas;
    }

    json ComponentSchema::Defaults() const
    {
        json out = json::object();
        for (const ComponentField &field : fields)
            out[field.name] = field.defaultValue;
        return out;
    }

    std::map<std::string, ComponentSchema> LoadComponentSchemas(const std::filesystem::path &folder, std::vector<core::Diagnostic> &diagnostics)
    {
        std::map<std::string, ComponentSchema> schemas;
        std::error_code ec;
        if (folder.empty() || !std::filesystem::is_directory(folder, ec))
            return schemas;
        for (const auto &entry : std::filesystem::directory_iterator(folder, ec))
        {
            const std::string name = entry.path().filename().string();
            if (!name.ends_with(".component.json"))
                continue;
            const auto bad = [&](const std::string &message) {
                diagnostics.push_back({core::Severity::Warning, "component.schema", entry.path().generic_string(), "", message});
            };
            // 1. The header.
            json root = json::parse(std::ifstream(entry.path()), nullptr, false);
            if (!root.is_object() || root.value("format", "") != "umbra.component")
            {
                bad("expected \"format\": \"umbra.component\"");
                continue;
            }
            ComponentSchema schema;
            schema.file = entry.path();
            schema.type = root.value("type", "");
            schema.label = root.value("label", schema.type);
            schema.description = root.value("description", "");
            if (schema.type.empty() || std::find(builtInComponents.begin(), builtInComponents.end(), schema.type) != builtInComponents.end() ||
                EngineComponentSchemas().contains(schema.type))
            {
                bad("needs a \"type\" that is not a built-in or engine component name");
                continue;
            }
            // 2. The fields, in file order.
            bool ok = true;
            const json fieldList = root.value("fields", json::object()); // a local: items() must not outlive it
            for (const auto &[key, spec] : fieldList.items())
            {
                ComponentField field;
                field.name = key;
                field.kind = spec.value("kind", "number");
                field.label = spec.value("label", key);
                field.min = spec.value("min", field.min);
                field.max = spec.value("max", field.max);
                field.folder = spec.value("folder", "");
                field.defaultValue = spec.contains("default") ? spec["default"] : ZeroOf(field.kind);
                if (!kinds.contains(field.kind))
                {
                    bad("field '" + key + "': unknown kind '" + field.kind + "'");
                    ok = false;
                }
                schema.fields.push_back(field);
            }
            if (!ok)
                continue;
            if (schemas.contains(schema.type))
                bad("type '" + schema.type + "' is described twice");
            schemas[schema.type] = std::move(schema);
        }
        return schemas;
    }

    std::map<std::string, ComponentSchema> LoadComponentSchemas(const assets::AssetFolders &assets, std::vector<core::Diagnostic> &diagnostics)
    {
        // Plugins first (last one first), then the project, so later folders replace earlier types.
        std::map<std::string, ComponentSchema> schemas = EngineComponentSchemas();
        for (auto it = assets.All().rbegin(); it != assets.All().rend(); ++it)
            for (auto &[type, schema] : LoadComponentSchemas(*it / "components", diagnostics))
                schemas[type] = std::move(schema);
        return schemas;
    }

    void CheckComponent(const ComponentSchema &schema, const json &value, const std::string &file, const std::string &where,
                        std::vector<core::Diagnostic> &diagnostics)
    {
        const auto error = [&](const std::string &field, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "entity.component.field", file, where + (field.empty() ? "" : "." + field), message});
        };
        if (!value.is_object())
        {
            error("", "expected an object of fields");
            return;
        }
        for (const auto &[key, v] : value.items())
        {
            const auto field = std::find_if(schema.fields.begin(), schema.fields.end(), [&](const ComponentField &f) { return f.name == key; });
            if (field == schema.fields.end())
            {
                diagnostics.push_back({core::Severity::Warning, "entity.component.unknown-field", file, where + "." + key,
                                       "'" + schema.type + "' has no field '" + key + "' (" + schema.file.filename().string() + ")"});
                continue;
            }
            const std::string &kind = field->kind;
            if (kind == "number" || kind == "int")
            {
                if (!v.is_number() || (kind == "int" && !v.is_number_integer()))
                    error(key, "expected " + std::string(kind == "int" ? "a whole number" : "a number"));
                else if (v.get<double>() < field->min || v.get<double>() > field->max)
                    error(key, "out of range");
            }
            else if (kind == "color")
            {
                bool ok = v.is_array() && v.size() == 3;
                for (std::size_t c = 0; ok && c < 3; ++c)
                    ok = v[c].is_number() && v[c].get<double>() >= field->min && v[c].get<double>() <= field->max;
                if (!ok)
                    error(key, "expected [r, g, b], each " + Number(field->min) + " to " + Number(field->max));
            }
            else if (kind == "bool" ? !v.is_boolean() : !v.is_string())
                error(key, "expected " + std::string(kind == "bool" ? "true or false" : "text"));
        }
    }
}
