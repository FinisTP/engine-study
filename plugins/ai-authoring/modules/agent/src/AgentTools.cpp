#include "AgentTools.hpp"

#include <ComponentSchema.hpp>
#include <Generate.hpp>
#include <GltfImporter.hpp>
#include <ModelAssistant.hpp>
#include <ModelCache.hpp>
#include <ModelTools.hpp>
#include <SceneEdits.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::agent
{
    namespace
    {
        using authoring::Change;
        using core::Diagnostic;
        using core::Severity;

        // ---- small helpers ----

        ToolResult Ok(std::string text, std::string value = "")
        {
            ToolResult r;
            r.ok = true;
            r.text = std::move(text);
            r.value = std::move(value);
            return r;
        }

        ToolResult Fail(std::string text, std::vector<Diagnostic> diagnostics = {})
        {
            ToolResult r;
            r.text = std::move(text);
            r.diagnostics = std::move(diagnostics);
            for (const Diagnostic &d : r.diagnostics)
                if (d.severity == Severity::Error)
                    r.text += "\n  " + d.code + " " + d.field + ": " + d.message;
            return r;
        }

        // A JSON schema property.
        Json Prop(const char *type, const char *description) { return {{"type", type}, {"description", description}}; }
        Json Vec3(const char *description) { return {{"type", "array"}, {"items", {{"type", "number"}}}, {"minItems", 3}, {"maxItems", 3}, {"description", description}}; }
        Json Schema(Json properties, std::vector<std::string> required = {})
        {
            return {{"type", "object"}, {"properties", std::move(properties)}, {"required", required}};
        }
        const Json sceneProp = Prop("string", "scene asset path, e.g. scenes/settlement.scene.json (omit for the open scene)");
        const Json idProp = Prop("string", "entity id");

        std::string Short(const Json &value, std::size_t limit = 4000)
        {
            std::string text = value.dump();
            return text.size() <= limit ? text : text.substr(0, limit) + "... (cut)";
        }

        // The scene document the arguments name, or nullptr with the reason in `fail`.
        authoring::Document *SceneOf(Workspace &ws, const Json &args, ToolResult &fail)
        {
            std::vector<Diagnostic> diagnostics;
            authoring::Document *doc = ws.scene ? ws.scene(args.value("scene", std::string()), diagnostics) : nullptr;
            if (!doc)
                fail = Fail("can't open the scene " + args.value("scene", std::string("(open scene)")), diagnostics);
            return doc;
        }

        // The entity's index, or nullopt with the reason in `fail`.
        std::optional<std::size_t> EntityOf(const Json &scene, const Json &args, ToolResult &fail)
        {
            const auto index = authoring::FindEntity(scene, args.value("id", std::string()));
            if (!index)
                fail = Fail("no entity with id '" + args.value("id", std::string()) + "' (use scene_outline)");
            return index;
        }

        // Rotation about the vertical axis as the file's quaternion [x, y, z, w].
        Json TurnY(double degrees)
        {
            const double half = degrees * 3.14159265358979 / 360.0;
            return Json::array({0.0, std::sin(half), 0.0, std::cos(half)});
        }

        ToolResult Commit(Edits &edits, authoring::Document &doc, const std::vector<Change> &changes, std::string text, std::string value = "")
        {
            std::vector<Diagnostic> diagnostics;
            if (!edits.Change(doc, changes, diagnostics))
                return Fail("the scene check refused this change", diagnostics);
            return Ok(std::move(text), std::move(value));
        }

        // ---- read tools ----

        ToolResult ListScenes(Workspace &ws, Edits &, const Json &)
        {
            Json list = Json::array();
            for (const std::string &file : ws.assets.ListFiles())
                if (file.ends_with(".scene.json"))
                    list.push_back(file);
            return Ok(list.dump());
        }

        ToolResult SceneOutline(Workspace &ws, Edits &edits, const Json &args)
        {
            ToolResult fail;
            authoring::Document *doc = SceneOf(ws, args, fail);
            if (!doc)
                return fail;
            Json list = Json::array();
            for (const Json &e : edits.Data(*doc)["entities"])
            {
                Json row = {{"id", e["id"]}};
                for (const char *key : {"name", "parent", "position", "model", "instance", "material"})
                    if (e.contains(key))
                        row[key] = e[key];
                // Component types, also built-ins in the older place directly on the entity.
                Json types = Json::array();
                for (const std::string &type : scene::builtInComponents)
                    if (e.contains(type))
                        types.push_back(type);
                if (e.contains("components"))
                    for (const auto &[type, value] : e["components"].items())
                        types.push_back(type);
                if (!types.empty())
                    row["components"] = types;
                list.push_back(row);
            }
            return Ok(Short(list, 12000));
        }

        ToolResult GetEntity(Workspace &ws, Edits &edits, const Json &args)
        {
            ToolResult fail;
            authoring::Document *doc = SceneOf(ws, args, fail);
            if (!doc)
                return fail;
            const Json &scene = edits.Data(*doc);
            const auto index = EntityOf(scene, args, fail);
            return index ? Ok(scene["entities"][*index].dump(), args["id"]) : fail;
        }

        ToolResult ListAssets(Workspace &ws, Edits &, const Json &args)
        {
            const std::string kind = args.value("kind", std::string());
            const auto fits = [&kind](const std::string &f) {
                if (kind == "models")
                    return f.ends_with(".glb") || f.ends_with(".gltf");
                if (kind == "sounds")
                    return f.ends_with(".wav");
                if (kind == "music")
                    return f.ends_with(".ogg");
                if (kind == "materials")
                    return f.ends_with(".material.json");
                if (kind == "textures")
                    return f.ends_with(".png") && f.starts_with("textures/");
                return kind == "screens" && f.ends_with(".ui.json");
            };
            Json list = Json::array();
            for (const std::string &file : ws.assets.ListFiles())
                if (fits(file))
                    list.push_back(file);
            return Ok(Short(list, 8000));
        }

        ToolResult ListComponents(Workspace &ws, Edits &, const Json &)
        {
            Json list = Json::array();
            for (const std::string &type : scene::builtInComponents)
                list.push_back({{"type", type}, {"builtIn", true}, {"help", "see the scene format: collider {shape, motion}, interactable {prompt, message, sound}, portal {scene, spawn}, spawner"}});
            std::vector<Diagnostic> ignored;
            for (const auto &[type, schema] : scene::LoadComponentSchemas(ws.assets, ignored))
            {
                Json fields = Json::object();
                for (const auto &f : schema.fields)
                    fields[f.name] = f.kind + (f.folder.empty() ? "" : " in " + f.folder);
                list.push_back({{"type", type}, {"description", schema.description}, {"fields", fields}});
            }
            return Ok(list.dump());
        }

        ToolResult RpgOutline(Workspace &ws, Edits &edits, const Json &)
        {
            if (!ws.rpg)
                return Fail("this project has no RPG content");
            Json out = Json::object();
            for (const auto &[key, value] : edits.Data(*ws.rpg).items())
                if (value.is_array())
                {
                    out[key] = Json::array();
                    for (const Json &entry : value)
                        out[key].push_back(entry.value("id", std::string("?")));
                }
            return Ok(out.dump());
        }

        ToolResult RpgGet(Workspace &ws, Edits &edits, const Json &args)
        {
            if (!ws.rpg)
                return Fail("this project has no RPG content");
            const Json &data = edits.Data(*ws.rpg);
            try
            {
                return Ok(Short(data.at(Json::json_pointer(args.value("path", std::string())))));
            }
            catch (const std::exception &)
            {
                return Fail("nothing at " + args.value("path", std::string()) + " (use rpg_outline; paths look like /items/2)");
            }
        }

        // ---- change tools: scenes ----

        ToolResult AddEntity(Workspace &ws, Edits &edits, const Json &args)
        {
            ToolResult fail;
            authoring::Document *doc = SceneOf(ws, args, fail);
            if (!doc)
                return fail;
            const Json &scene = edits.Data(*doc);
            Json entity = {{"id", authoring::UniqueEntityId(scene, args["id"].get<std::string>())}};
            for (const char *key : {"name", "parent", "position", "scale", "model", "mesh", "instance", "material", "color", "components"})
                if (args.contains(key))
                    entity[key] = args[key];
            if (args.contains("turn_degrees"))
                entity["rotation"] = TurnY(args["turn_degrees"].get<double>());
            const std::string id = entity["id"];
            return Commit(edits, *doc, authoring::AddEntity(entity), "added " + id, id);
        }

        ToolResult PlaceModel(Workspace &ws, Edits &edits, const Json &args)
        {
            ToolResult fail;
            authoring::Document *doc = SceneOf(ws, args, fail);
            if (!doc)
                return fail;
            const std::string model = args["model"];
            authoring::ModelInfo info;
            std::vector<Diagnostic> diagnostics;
            if (!assets::IsSafeAssetPath(model) || !authoring::InspectModel(ws.assets / model, info, diagnostics))
                return Fail("can't read the model " + model + " (use list_assets models)", diagnostics);
            const Json &p = args["position"];
            const glm::vec3 at{p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
            Json entity = localai::PlacedModelEntity(edits.Data(*doc), args.value("name", std::filesystem::path(model).stem().string()), model, info, at,
                                                     args.value("height", 2.0F));
            if (args.contains("turn_degrees"))
                entity["rotation"] = TurnY(args["turn_degrees"].get<double>());
            const std::string id = entity["id"];
            return Commit(edits, *doc, authoring::AddEntity(entity), "placed " + model + " as " + id, id);
        }

        ToolResult MoveEntity(Workspace &ws, Edits &edits, const Json &args)
        {
            ToolResult fail;
            authoring::Document *doc = SceneOf(ws, args, fail);
            if (!doc)
                return fail;
            const auto index = EntityOf(edits.Data(*doc), args, fail);
            if (!index)
                return fail;
            std::vector<Change> changes;
            const std::string at = authoring::EntityPath(*index);
            if (args.contains("position"))
                changes.push_back({Change::Op::Set, at + "/position", args["position"]});
            if (args.contains("scale"))
                changes.push_back({Change::Op::Set, at + "/scale", args["scale"]});
            if (args.contains("turn_degrees"))
                changes.push_back({Change::Op::Set, at + "/rotation", TurnY(args["turn_degrees"].get<double>())});
            if (changes.empty())
                return Fail("give position, scale or turn_degrees");
            return Commit(edits, *doc, changes, "moved " + args["id"].get<std::string>(), args["id"]);
        }

        ToolResult RemoveEntity(Workspace &ws, Edits &edits, const Json &args)
        {
            ToolResult fail;
            authoring::Document *doc = SceneOf(ws, args, fail);
            if (!doc)
                return fail;
            if (!EntityOf(edits.Data(*doc), args, fail))
                return fail;
            return Commit(edits, *doc, authoring::RemoveEntity(edits.Data(*doc), args["id"]), "removed " + args["id"].get<std::string>() + " and its children");
        }

        ToolResult SetField(Workspace &ws, Edits &edits, const Json &args)
        {
            ToolResult fail;
            authoring::Document *doc = SceneOf(ws, args, fail);
            if (!doc)
                return fail;
            const Json &scene = edits.Data(*doc);
            const auto index = EntityOf(scene, args, fail);
            if (!index)
                return fail;
            const std::string field = args["field"];
            const std::string path = authoring::EntityPath(*index) + "/" + field;
            if (args["value"].is_null())
                return scene["entities"][*index].contains(field) ? Commit(edits, *doc, {{Change::Op::Remove, path, {}}}, "cleared " + field, args["id"])
                                                                 : Ok(field + " was not set", args["id"]);
            return Commit(edits, *doc, {{Change::Op::Set, path, args["value"]}}, "set " + field, args["id"]);
        }

        ToolResult SetComponent(Workspace &ws, Edits &edits, const Json &args)
        {
            ToolResult fail;
            authoring::Document *doc = SceneOf(ws, args, fail);
            if (!doc)
                return fail;
            const Json &scene = edits.Data(*doc);
            const auto index = EntityOf(scene, args, fail);
            if (!index)
                return fail;
            // Start from the schema's defaults, then what the entity already has, so the model only
            // names the fields that change.
            const std::string type = args["type"];
            const Json &entity = scene["entities"][*index];
            Json fields = Json::object();
            std::vector<Diagnostic> ignored;
            const auto schemas = scene::LoadComponentSchemas(ws.assets, ignored);
            if (const auto it = schemas.find(type); it != schemas.end())
                fields = it->second.Defaults();
            else if (std::find(scene::builtInComponents.begin(), scene::builtInComponents.end(), type) == scene::builtInComponents.end())
                return Fail("no component type '" + type + "' (use list_components)");
            if (entity.contains("components") && entity["components"].contains(type))
                fields.update(entity["components"][type]);
            else if (entity.contains(type))
                fields.update(entity[type]);
            fields.update(args.value("fields", Json::object()));
            const std::string at = authoring::EntityPath(*index);
            std::vector<Change> changes;
            if (entity.contains(type)) // an older file keeps built-ins on the entity: move it
                changes.push_back({Change::Op::Remove, at + "/" + type, {}});
            if (entity.contains("components"))
                changes.push_back({Change::Op::Set, at + "/components/" + type, fields});
            else
                changes.push_back({Change::Op::Set, at + "/components", Json{{type, fields}}});
            return Commit(edits, *doc, changes, "set " + type + " on " + args["id"].get<std::string>(), args["id"]);
        }

        ToolResult RemoveComponent(Workspace &ws, Edits &edits, const Json &args)
        {
            ToolResult fail;
            authoring::Document *doc = SceneOf(ws, args, fail);
            if (!doc)
                return fail;
            const Json &scene = edits.Data(*doc);
            const auto index = EntityOf(scene, args, fail);
            if (!index)
                return fail;
            const std::string type = args["type"];
            const Json &entity = scene["entities"][*index];
            const std::string at = authoring::EntityPath(*index);
            if (entity.contains("components") && entity["components"].contains(type))
                return Commit(edits, *doc, {{Change::Op::Remove, at + "/components/" + type, {}}}, "removed " + type, args["id"]);
            if (entity.contains(type)) // the older place
                return Commit(edits, *doc, {{Change::Op::Remove, at + "/" + type, {}}}, "removed " + type, args["id"]);
            return Ok(args["id"].get<std::string>() + " has no " + type, args["id"]);
        }

        // ---- change tools: RPG content ----

        ToolResult RpgChange(Workspace &ws, Edits &edits, Change change, const std::string &text)
        {
            if (!ws.rpg)
                return Fail("this project has no RPG content");
            std::vector<Diagnostic> diagnostics;
            if (!edits.Change(*ws.rpg, {change}, diagnostics))
                return Fail("the RPG content check refused this change", diagnostics);
            return Ok(text);
        }
        ToolResult RpgSet(Workspace &ws, Edits &edits, const Json &args)
        {
            return RpgChange(ws, edits, {Change::Op::Set, args["path"], args["value"]}, "set " + args["path"].get<std::string>());
        }
        ToolResult RpgAdd(Workspace &ws, Edits &edits, const Json &args)
        {
            const std::string id = args["value"].is_object() ? args["value"].value("id", std::string()) : std::string();
            ToolResult r = RpgChange(ws, edits, {Change::Op::Insert, args["path"].get<std::string>() + "/-", args["value"]}, "added to " + args["path"].get<std::string>());
            r.value = id;
            return r;
        }
        ToolResult RpgRemove(Workspace &ws, Edits &edits, const Json &args)
        {
            return RpgChange(ws, edits, {Change::Op::Remove, args["path"], {}}, "removed " + args["path"].get<std::string>());
        }

        // ---- generate tools (local ComfyUI) ----

        ToolResult Made(const localai::GenerateResult &result, const std::string &what)
        {
            if (result.made.empty())
                return Fail("couldn't make the " + what, result.diagnostics);
            ToolResult r = Ok("made " + result.made[0].path + " with " + result.model, result.made[0].path);
            r.diagnostics = result.diagnostics; // warnings (license, seam) stay visible
            return r;
        }
        localai::GenerateOptions Options(Workspace &ws, const Json &args)
        {
            localai::GenerateOptions options;
            options.idea = args["idea"];
            options.name = args.value("name", std::string());
            options.assets = ws.assets;
            options.stop = ws.stop;
            return options;
        }
        ToolResult GenerateTexture(Workspace &ws, Edits &, const Json &args)
        {
            auto options = Options(ws, args);
            options.meters = args.value("meters", 2.0F);
            return Made(localai::CreateTexture(options), "texture");
        }
        ToolResult GenerateSound(Workspace &ws, Edits &, const Json &args)
        {
            auto options = Options(ws, args);
            options.kind = args["kind"] == "effect" ? localai::SoundKind::Effect : localai::SoundKind::Music;
            options.seconds = std::clamp(args.value("seconds", 0.0F), 0.0F, 300.0F);
            return Made(localai::CreateSound(options), "sound");
        }
        ToolResult GenerateModel(Workspace &ws, Edits &, const Json &args) { return Made(localai::CreateModel(Options(ws, args)), "model"); }

        // ---- the list ----

        std::vector<Tool> MakeTools()
        {
            using K = Tool::Kind;
            const Json s = sceneProp;
            const Json id = idProp;
            return {
                {"list_scenes", "List the project's scene files.", Schema(Json::object()), K::Read, ListScenes},
                {"scene_outline", "List a scene's entities: id, name, parent, position, model, component types.", Schema({{"scene", s}}), K::Read, SceneOutline},
                {"get_entity", "Show one entity's full JSON.", Schema({{"scene", s}, {"id", id}}, {"id"}), K::Read, GetEntity},
                {"list_assets", "List asset files of one kind.",
                 Schema({{"kind", {{"type", "string"}, {"enum", {"models", "sounds", "music", "materials", "textures", "screens"}}}}}, {"kind"}), K::Read, ListAssets},
                {"list_components", "List the component types an entity can have, with their fields.", Schema(Json::object()), K::Read, ListComponents},
                {"rpg_outline", "List the ids of the RPG content: statuses, skills, items, actors, quests, conversations.", Schema(Json::object()), K::Read, RpgOutline},
                {"rpg_get", "Show part of the RPG content by JSON pointer, e.g. /items/2 or /quests/0.", Schema({{"path", Prop("string", "JSON pointer")}}, {"path"}), K::Read, RpgGet},

                {"add_entity", "Add an entity (an existing model, a marker, a trigger). Gets a free id based on `id`.",
                 Schema({{"scene", s}, {"id", Prop("string", "wanted id, e.g. well")}, {"name", Prop("string", "display name")}, {"position", Vec3("meters, y up")},
                         {"turn_degrees", Prop("number", "rotation about the vertical axis")}, {"scale", Vec3("scale")}, {"model", Prop("string", "glTF asset path")},
                         {"mesh", Prop("integer", "mesh index in the model")}, {"instance", Prop("string", "glTF whose whole hierarchy is placed")},
                         {"material", Prop("string", "material asset path")}, {"color", {{"type", "array"}, {"items", {{"type", "number"}}}, {"minItems", 4}, {"maxItems", 4}}},
                         {"parent", Prop("string", "parent entity id")}, {"components", Prop("object", "{type: fields}")}},
                        {"id", "position"}),
                 K::Change, AddEntity},
                {"place_model", "Place a model standing on the ground, scaled to a height in meters, with a fitted collider.",
                 Schema({{"scene", s}, {"model", Prop("string", "glTF/GLB asset path")}, {"position", Vec3("feet position, meters")},
                         {"height", Prop("number", "meters (default 2)")}, {"turn_degrees", Prop("number", "rotation about the vertical axis")}, {"name", Prop("string", "id/name base")}},
                        {"model", "position"}),
                 K::Change, PlaceModel},
                {"move_entity", "Move, turn or scale an entity.",
                 Schema({{"scene", s}, {"id", id}, {"position", Vec3("meters")}, {"turn_degrees", Prop("number", "about the vertical axis")}, {"scale", Vec3("scale")}}, {"id"}),
                 K::Change, MoveEntity},
                {"remove_entity", "Remove an entity and its children.", Schema({{"scene", s}, {"id", id}}, {"id"}), K::Change, RemoveEntity},
                {"set_field", "Set or clear (value null) an entity field.",
                 Schema({{"scene", s}, {"id", id}, {"field", {{"type", "string"}, {"enum", {"name", "model", "mesh", "material", "color", "instance", "parent"}}}}, {"value", {{"description", "new value or null"}}}},
                        {"id", "field", "value"}),
                 K::Change, SetField},
                {"set_component", "Add a component to an entity or change some of its fields (the others keep their values or defaults).",
                 Schema({{"scene", s}, {"id", id}, {"type", Prop("string", "component type")}, {"fields", Prop("object", "field values")}}, {"id", "type"}), K::Change, SetComponent},
                {"remove_component", "Remove a component from an entity.", Schema({{"scene", s}, {"id", id}, {"type", Prop("string", "component type")}}, {"id", "type"}), K::Change,
                 RemoveComponent},
                {"rpg_set", "Replace a value in the RPG content (JSON pointer).", Schema({{"path", Prop("string", "JSON pointer, e.g. /items/2/price")}, {"value", {{"description", "new value"}}}}, {"path", "value"}),
                 K::Change, RpgSet},
                {"rpg_add", "Append an entry to an RPG list, e.g. path /items with a new item object.",
                 Schema({{"path", Prop("string", "list pointer, e.g. /items")}, {"value", {{"description", "the new entry"}}}}, {"path", "value"}), K::Change, RpgAdd},
                {"rpg_remove", "Remove a value from the RPG content.", Schema({{"path", Prop("string", "JSON pointer")}}, {"path"}), K::Change, RpgRemove},

                {"generate_texture", "Make a seamless texture and its material with the local image model. Result: the material path.",
                 Schema({{"idea", Prop("string", "what the surface looks like")}, {"name", Prop("string", "file name")}, {"meters", Prop("number", "meters per repeat")}}, {"idea"}),
                 K::Generate, GenerateTexture},
                {"generate_sound", "Make music (looping .ogg) or a sound effect (.wav) with the local audio models. Result: the sound path.",
                 Schema({{"idea", Prop("string", "what it sounds like")}, {"kind", {{"type", "string"}, {"enum", {"music", "effect"}}}}, {"seconds", Prop("number", "length")},
                         {"name", Prop("string", "file name")}},
                        {"idea", "kind"}),
                 K::Generate, GenerateSound},
                {"generate_model", "Make a 3D prop (GLB) with the local image-to-3D workflow. Slow (minutes). Result: the model path.",
                 Schema({{"idea", Prop("string", "the object")}, {"name", Prop("string", "file name")}}, {"idea"}), K::Generate, GenerateModel},
            };
        }

        bool TypeFits(const Json &value, const std::string &type)
        {
            if (type == "string")
                return value.is_string();
            if (type == "number")
                return value.is_number();
            if (type == "integer")
                return value.is_number_integer();
            if (type == "boolean")
                return value.is_boolean();
            if (type == "array")
                return value.is_array();
            return type != "object" || value.is_object();
        }
    }

    const std::vector<Tool> &Tools()
    {
        static const std::vector<Tool> tools = MakeTools();
        return tools;
    }

    const Tool *FindTool(const std::string &name)
    {
        for (const Tool &tool : Tools())
            if (tool.name == name)
                return &tool;
        return nullptr;
    }

    std::string CheckArguments(const Tool &tool, const Json &args)
    {
        if (!args.is_object())
            return "arguments must be an object";
        const Json &properties = tool.schema["properties"];
        for (const Json &name : tool.schema["required"])
            if (!args.contains(name.get<std::string>()))
                return "missing \"" + name.get<std::string>() + "\"";
        for (const auto &[key, value] : args.items())
        {
            if (!properties.contains(key))
                return "unknown argument \"" + key + "\"";
            const Json &p = properties[key];
            const bool reference = value.is_string() && value.get<std::string>().starts_with("$"); // filled in when it runs
            if (p.contains("type") && !reference && !TypeFits(value, p["type"]))
                return "\"" + key + "\" must be a " + p["type"].get<std::string>();
            if (p.contains("enum") && std::find(p["enum"].begin(), p["enum"].end(), value) == p["enum"].end())
                return "\"" + key + "\" must be one of " + p["enum"].dump();
            if (value.is_array() && p.contains("minItems") && (value.size() < p["minItems"].get<std::size_t>() || value.size() > p["maxItems"].get<std::size_t>()))
                return "\"" + key + "\" must have " + p["minItems"].dump() + " numbers";
            if (value.is_array() && p.contains("items"))
                for (const Json &item : value)
                    if (!TypeFits(item, p["items"]["type"]))
                        return "\"" + key + "\" must hold numbers";
        }
        return "";
    }

    Json DescribeTools()
    {
        Json list = Json::array();
        for (const Tool &tool : Tools())
            list.push_back({{"name", tool.name},
                            {"description", tool.description},
                            {"kind", tool.kind == Tool::Kind::Read ? "read" : tool.kind == Tool::Kind::Change ? "change" : "generate"},
                            {"inputSchema", tool.schema}});
        return list;
    }

    // ---- Edits ----

    Edits::Pending &Edits::Of(authoring::Document &doc)
    {
        for (Pending &p : pending_)
            if (p.doc == &doc)
                return p;
        pending_.push_back({&doc, doc.Revision(), doc.Data(), {}});
        return pending_.back();
    }

    const Json &Edits::Data(authoring::Document &doc)
    {
        for (const Pending &p : pending_)
            if (p.doc == &doc)
                return p.data;
        return doc.Data();
    }

    bool Edits::Change(authoring::Document &doc, const std::vector<authoring::Change> &changes, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. Apply to a copy of the working state; 2. check it like the game would load it; 3. keep it.
        Json next;
        if (!doc.IsOpen() || !authoring::ApplyChanges(Data(doc), changes, next, diagnostics, doc.File().string()) || !doc.Check(next, diagnostics))
            return false;
        Pending &p = Of(doc);
        p.data = std::move(next);
        p.changes.insert(p.changes.end(), changes.begin(), changes.end());
        return true;
    }

    bool Edits::Commit(const std::string &label, std::vector<authoring::Document *> &changed, std::vector<core::Diagnostic> &diagnostics)
    {
        changed.clear();
        for (Pending &p : pending_)
        {
            if (p.changes.empty())
                continue;
            if (!p.doc->Apply(p.changes, label, diagnostics, p.revision))
            {
                for (authoring::Document *done : changed)
                    done->Undo(); // all or none
                changed.clear();
                return false;
            }
            changed.push_back(p.doc);
        }
        pending_.clear();
        return true;
    }

    // ---- FileWorkspace ----

    struct FileWorkspace::Models final
    {
        std::map<std::string, assets::ImportResult> imported;
    };

    FileWorkspace::FileWorkspace(assets::AssetFolders assets) : models_(std::make_unique<Models>())
    {
        workspace_.assets = std::move(assets);
        // The project's RPG content: the first rpg/*.rpg.json.
        std::error_code error;
        for (const auto &entry : std::filesystem::directory_iterator(workspace_.assets.Project() / "rpg", error))
            if (entry.path().string().ends_with(".rpg.json"))
            {
                rpg_ = std::make_unique<authoring::Document>(entry.path(), authoring::RpgValidator());
                std::vector<Diagnostic> ignored;
                workspace_.rpg = rpg_->Open(ignored) ? rpg_.get() : nullptr;
                break;
            }
        workspace_.scene = [this](const std::string &asked, std::vector<Diagnostic> &diagnostics) -> authoring::Document * {
            const std::string path = asked.empty() ? openScene_ : asked;
            if (path.empty() || !path.ends_with(".scene.json") || !assets::IsSafeAssetPath(path))
            {
                diagnostics.push_back({Severity::Error, "agent.scene", path, "scene", "name a scene file, e.g. scenes/settlement.scene.json"});
                return nullptr;
            }
            if (const auto it = scenes_.find(path); it != scenes_.end())
                return it->second.get();
            // Scenes are checked like the game loads them, so their models are imported (cached).
            scene::SceneLoadOptions options;
            options.assetRoot = workspace_.assets;
            options.models = [this](const std::string &model) -> const assets::Model * {
                auto it = models_->imported.find(model);
                if (it == models_->imported.end())
                    it = models_->imported.emplace(model, assets::ImportGltf(workspace_.assets / model)).first;
                return it->second.Ok() ? &it->second.model : nullptr;
            };
            auto doc = std::make_unique<authoring::Document>(workspace_.assets.Project() / path, authoring::SceneValidator(options));
            if (!doc->Open(diagnostics))
                return nullptr;
            return scenes_.emplace(path, std::move(doc)).first->second.get();
        };
    }

    FileWorkspace::~FileWorkspace() = default;

    bool FileWorkspace::Save(std::vector<core::Diagnostic> &diagnostics)
    {
        bool ok = true;
        if (rpg_ && rpg_->IsDirty())
            ok &= rpg_->Save(diagnostics);
        for (auto &[path, doc] : scenes_)
            if (doc->IsDirty())
                ok &= doc->Save(diagnostics);
        return ok;
    }

    void FileWorkspace::Refresh()
    {
        std::vector<Diagnostic> ignored;
        if (rpg_ && !rpg_->IsDirty() && rpg_->ChangedOnDisk())
            rpg_->Open(ignored);
        for (auto &[path, doc] : scenes_)
            if (!doc->IsDirty() && doc->ChangedOnDisk())
                doc->Open(ignored);
    }
}
