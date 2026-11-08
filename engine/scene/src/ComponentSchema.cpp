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
        const std::set<std::string> kinds{"number", "int", "bool", "string", "asset", "actor", "scene"};

        // The default a field gets when the schema gives none.
        json ZeroOf(const std::string &kind)
        {
            if (kind == "number" || kind == "int")
                return 0;
            if (kind == "bool")
                return false;
            return "";
        }
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
            if (schema.type.empty() || std::find(builtInComponents.begin(), builtInComponents.end(), schema.type) != builtInComponents.end())
            {
                bad("needs a \"type\" that is not a built-in component name");
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
            else if (kind == "bool" ? !v.is_boolean() : !v.is_string())
                error(key, "expected " + std::string(kind == "bool" ? "true or false" : "text"));
        }
    }
}
