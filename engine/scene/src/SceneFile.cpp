#include <SceneFile.hpp>

#include <ModelCache.hpp>
#include <ModelInstance.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <unordered_map>

namespace umbra::scene
{
    namespace
    {
        using nlohmann::json;
        using core::Diagnostic;
        using core::Severity;

        // Limits so a hostile or broken file can't make the loader allocate without bound.
        constexpr std::uintmax_t maxFileBytes = 64ull * 1024 * 1024;
        constexpr std::size_t maxEntities = 100000;

        // One entity as written in the file, after validation.
        struct EntityDesc final
        {
            std::string id;
            std::string name;
            std::string parent;
            world::Transform transform;
            std::string model;
            std::uint32_t mesh = 0;
            glm::vec4 color{1.0F};
            std::string instance;
            std::optional<world::Collider> collider;
            std::optional<world::Interactable> interactable;
            std::optional<world::Portal> portal;
            std::optional<world::Spawner> spawner;
        };

        struct ParsedScene final
        {
            std::vector<EntityDesc> entities;
            std::unordered_map<std::string, const assets::Model *> instanceModels;
        };

        // Collects diagnostics for one file.
        struct Report final
        {
            std::string file;
            std::vector<Diagnostic> &out;
            bool failed = false;

            void Error(std::string code, std::string field, std::string message)
            {
                out.push_back({Severity::Error, std::move(code), file, std::move(field), std::move(message)});
                failed = true;
            }
            void Warning(std::string code, std::string field, std::string message)
            {
                out.push_back({Severity::Warning, std::move(code), file, std::move(field), std::move(message)});
            }
        };

        // Reads an optional array of `count` finite numbers into `values`.
        bool ReadFloats(const json &entity, const char *key, std::size_t count, float *values, const std::string &where, Report &report)
        {
            const auto it = entity.find(key);
            if (it == entity.end())
                return true;
            const std::string field = where + "." + key;
            if (!it->is_array() || it->size() != count)
            {
                report.Error("entity.field.type", field, "expected an array of " + std::to_string(count) + " numbers");
                return false;
            }
            for (std::size_t i = 0; i < count; ++i)
            {
                const json &value = (*it)[i];
                if (!value.is_number() || !std::isfinite(value.get<double>()))
                {
                    report.Error("entity.field.nonfinite", field, "element " + std::to_string(i) + " is not a finite number");
                    return false;
                }
                values[i] = value.get<float>();
            }
            return true;
        }

        // Reads an optional string field.
        bool ReadString(const json &entity, const char *key, std::string &value, const std::string &where, Report &report)
        {
            const auto it = entity.find(key);
            if (it == entity.end())
                return true;
            if (!it->is_string())
            {
                report.Error("entity.field.type", where + "." + key, "expected a string");
                return false;
            }
            value = it->get<std::string>();
            return true;
        }

        void CheckAssetPath(const std::string &path, const std::string &field, const SceneLoadOptions &options, Report &report)
        {
            if (!assets::IsSafeAssetPath(path))
                report.Error("asset.path.unsafe", field, "'" + path + "' must be relative to the asset folder and must not contain '..'");
            else if (!options.assetRoot.empty() && !std::filesystem::is_regular_file(options.assetRoot / path))
                report.Error("asset.missing", field, "'" + path + "' does not exist in " + options.assetRoot.string());
        }

        // "collider": { "shape": "box", "motion": "dynamic", "halfExtents": [..], "radius": r, "halfHeight": h }
        std::optional<world::Collider> ParseCollider(const json &value, const std::string &where, Report &report)
        {
            if (!value.is_object())
            {
                report.Error("entity.field.type", where, "expected an object");
                return std::nullopt;
            }
            static const std::set<std::string> knownKeys{"shape", "motion", "halfExtents", "radius", "halfHeight"};
            for (const auto &[key, unused] : value.items())
            {
                if (!knownKeys.contains(key))
                    report.Warning("entity.field.unknown", where + "." + key, "unknown field is ignored (typo?)");
            }

            world::Collider collider;
            std::string shape = "box", motion = "static";
            ReadString(value, "shape", shape, where, report);
            ReadString(value, "motion", motion, where, report);
            if (shape == "box")
                collider.shape = world::ColliderShape::Box;
            else if (shape == "sphere")
                collider.shape = world::ColliderShape::Sphere;
            else if (shape == "capsule")
                collider.shape = world::ColliderShape::Capsule;
            else
                report.Error("entity.collider.shape", where + ".shape", "expected \"box\", \"sphere\" or \"capsule\"");
            if (motion == "static")
                collider.motion = world::ColliderMotion::Static;
            else if (motion == "kinematic")
                collider.motion = world::ColliderMotion::Kinematic;
            else if (motion == "dynamic")
                collider.motion = world::ColliderMotion::Dynamic;
            else
                report.Error("entity.collider.motion", where + ".motion", "expected \"static\", \"kinematic\" or \"dynamic\"");

            // Sizes are optional (0 = from the entity's scale) but must be positive when given.
            float half[3] = {0, 0, 0};
            ReadFloats(value, "halfExtents", 3, half, where, report);
            collider.halfExtents = {half[0], half[1], half[2]};
            for (const char *key : {"radius", "halfHeight"})
            {
                const auto it = value.find(key);
                if (it == value.end())
                    continue;
                if (!it->is_number() || !std::isfinite(it->get<double>()))
                    report.Error("entity.field.nonfinite", where + "." + key, "expected a finite number");
                else
                    (std::string{key} == "radius" ? collider.radius : collider.halfHeight) = it->get<float>();
            }
            if (collider.halfExtents.x < 0 || collider.halfExtents.y < 0 || collider.halfExtents.z < 0 || collider.radius < 0 || collider.halfHeight < 0)
                report.Error("entity.collider.size", where, "collider sizes must not be negative");
            return collider;
        }

        // "interactable": { "prompt": "Read sign", "message": "...", "sound": "audio/x.wav" }
        // A missing sound file is only a warning: the game can run without it.
        std::optional<world::Interactable> ParseInteractable(const json &value, const std::string &where,
                                                             const SceneLoadOptions &options, Report &report)
        {
            if (!value.is_object())
            {
                report.Error("entity.field.type", where, "expected an object");
                return std::nullopt;
            }
            static const std::set<std::string> knownKeys{"prompt", "message", "sound", "conversation", "save"};
            for (const auto &[key, unused] : value.items())
            {
                if (!knownKeys.contains(key))
                    report.Warning("entity.field.unknown", where + "." + key, "unknown field is ignored (typo?)");
            }
            world::Interactable interactable;
            ReadString(value, "prompt", interactable.prompt, where, report);
            ReadString(value, "message", interactable.message, where, report);
            ReadString(value, "sound", interactable.sound, where, report);
            ReadString(value, "conversation", interactable.conversation, where, report);
            if (const auto save = value.find("save"); save != value.end())
            {
                if (save->is_boolean())
                    interactable.save = save->get<bool>();
                else
                    report.Error("entity.field.type", where + ".save", "expected true or false");
            }
            if (interactable.prompt.empty())
                report.Error("entity.interactable.prompt", where + ".prompt", "needs a non-empty prompt");
            if (!interactable.sound.empty())
            {
                if (!assets::IsSafeAssetPath(interactable.sound))
                    report.Error("asset.path.unsafe", where + ".sound", "'" + interactable.sound + "' must be relative to the asset folder and must not contain '..'");
                else if (!options.assetRoot.empty() && !std::filesystem::is_regular_file(options.assetRoot / interactable.sound))
                    report.Warning("entity.interactable.sound", where + ".sound", "'" + interactable.sound + "' does not exist; it will be silent");
            }
            return interactable;
        }

        // "spawner": { "pool": ["wolf", "bandit"], "count": 3, "radius": 4, "chase": true, "group": 3 }
        // Actor ids are checked against the rpg content by the game when the area loads.
        std::optional<world::Spawner> ParseSpawner(const json &value, const std::string &where, Report &report)
        {
            if (!value.is_object())
            {
                report.Error("entity.field.type", where, "expected an object");
                return std::nullopt;
            }
            static const std::set<std::string> knownKeys{"pool", "count", "radius", "chase", "group"};
            for (const auto &[key, unused] : value.items())
            {
                if (!knownKeys.contains(key))
                    report.Warning("entity.field.unknown", where + "." + key, "unknown field is ignored (typo?)");
            }
            world::Spawner spawner;
            const auto pool = value.find("pool");
            if (pool != value.end() && pool->is_array())
            {
                for (const json &actor : *pool)
                {
                    if (actor.is_string() && !actor.get<std::string>().empty())
                        spawner.pool.push_back(actor.get<std::string>());
                    else
                        report.Error("entity.spawner.pool", where + ".pool", "pool entries must be actor ids");
                }
            }
            if (spawner.pool.empty())
                report.Error("entity.spawner.pool", where + ".pool", "needs at least one actor id");
            const auto number = [&](const char *key, double min, double max, double fallback) {
                const auto it = value.find(key);
                if (it == value.end())
                    return fallback;
                if (!it->is_number() || it->get<double>() < min || it->get<double>() > max)
                {
                    report.Error("entity.spawner.range", where + "." + key, "out of range");
                    return fallback;
                }
                return it->get<double>();
            };
            spawner.count = static_cast<int>(number("count", 0, 20, 1));
            spawner.radius = static_cast<float>(number("radius", 0, 50, 3));
            spawner.group = static_cast<int>(number("group", 1, 3, 3));
            if (const auto chase = value.find("chase"); chase != value.end())
                spawner.chase = chase->is_boolean() && chase->get<bool>();
            return spawner;
        }

        // "portal": { "scene": "scenes/dungeon.scene.json", "spawn": "entry" }
        std::optional<world::Portal> ParsePortal(const json &value, const std::string &where,
                                                 const SceneLoadOptions &options, Report &report)
        {
            if (!value.is_object())
            {
                report.Error("entity.field.type", where, "expected an object");
                return std::nullopt;
            }
            static const std::set<std::string> knownKeys{"scene", "spawn"};
            for (const auto &[key, unused] : value.items())
            {
                if (!knownKeys.contains(key))
                    report.Warning("entity.field.unknown", where + "." + key, "unknown field is ignored (typo?)");
            }
            world::Portal portal;
            ReadString(value, "scene", portal.scene, where, report);
            ReadString(value, "spawn", portal.spawn, where, report);
            if (portal.scene.empty())
                report.Error("entity.portal.scene", where + ".scene", "needs the destination scene path");
            else
                CheckAssetPath(portal.scene, where + ".scene", options, report);
            if (portal.spawn.empty())
                report.Error("entity.portal.spawn", where + ".spawn", "spawn id must not be empty (omit it to use \"player\")");
            return portal;
        }

        EntityDesc ParseEntity(const json &entity, const std::string &where, const SceneLoadOptions &options, Report &report)
        {
            static const std::set<std::string> knownKeys{"id", "name", "parent", "position", "rotation", "scale", "model", "mesh", "color", "instance", "collider", "interactable", "portal", "spawner"};
            EntityDesc desc;
            for (const auto &[key, value] : entity.items())
            {
                if (!knownKeys.contains(key))
                    report.Warning("entity.field.unknown", where + "." + key, "unknown field is ignored (typo?)");
            }

            ReadString(entity, "id", desc.id, where, report);
            if (desc.id.empty())
                report.Error("entity.id.missing", where + ".id", "every entity needs a non-empty string id");
            ReadString(entity, "name", desc.name, where, report);
            ReadString(entity, "parent", desc.parent, where, report);

            // Transform: rotation is [x, y, z, w] and is normalized on load.
            float position[3] = {0, 0, 0}, rotation[4] = {0, 0, 0, 1}, scale[3] = {1, 1, 1}, color[4] = {1, 1, 1, 1};
            ReadFloats(entity, "position", 3, position, where, report);
            ReadFloats(entity, "scale", 3, scale, where, report);
            ReadFloats(entity, "color", 4, color, where, report);
            if (ReadFloats(entity, "rotation", 4, rotation, where, report))
            {
                const glm::quat q(rotation[3], rotation[0], rotation[1], rotation[2]);
                if (glm::length(q) < 1e-6F)
                    report.Error("entity.rotation.zero", where + ".rotation", "rotation quaternion has zero length");
                else
                    desc.transform.rotation = glm::normalize(q);
            }
            desc.transform.position = {position[0], position[1], position[2]};
            desc.transform.scale = {scale[0], scale[1], scale[2]};
            desc.color = {color[0], color[1], color[2], color[3]};

            // Renderable and model instance.
            if (ReadString(entity, "model", desc.model, where, report) && entity.contains("model"))
                CheckAssetPath(desc.model, where + ".model", options, report);
            if (const auto it = entity.find("mesh"); it != entity.end())
            {
                if (!it->is_number_unsigned() || it->get<std::uint64_t>() > UINT32_MAX)
                    report.Error("entity.field.type", where + ".mesh", "expected a non-negative 32-bit integer");
                else if (desc.model.empty())
                    report.Error("entity.mesh.nomodel", where + ".mesh", "'mesh' needs a 'model'");
                else
                    desc.mesh = it->get<std::uint32_t>();
            }
            if (ReadString(entity, "instance", desc.instance, where, report) && entity.contains("instance"))
                CheckAssetPath(desc.instance, where + ".instance", options, report);
            if (const auto it = entity.find("collider"); it != entity.end())
                desc.collider = ParseCollider(*it, where + ".collider", report);
            if (const auto it = entity.find("interactable"); it != entity.end())
                desc.interactable = ParseInteractable(*it, where + ".interactable", options, report);
            if (const auto it = entity.find("portal"); it != entity.end())
                desc.portal = ParsePortal(*it, where + ".portal", options, report);
            if (const auto it = entity.find("spawner"); it != entity.end())
                desc.spawner = ParseSpawner(*it, where + ".spawner", report);
            return desc;
        }

        // Every parent must exist, and following parents must never loop back.
        void CheckHierarchy(const std::vector<EntityDesc> &entities, Report &report)
        {
            std::unordered_map<std::string, std::size_t> index;
            for (std::size_t i = 0; i < entities.size(); ++i)
                index.emplace(entities[i].id, i);

            std::vector<int> parent(entities.size(), -1);
            for (std::size_t i = 0; i < entities.size(); ++i)
            {
                if (entities[i].parent.empty())
                    continue;
                const auto it = index.find(entities[i].parent);
                if (it == index.end())
                    report.Error("entity.parent.missing", "entities[" + std::to_string(i) + "].parent",
                                 "no entity has id '" + entities[i].parent + "'");
                else
                    parent[i] = static_cast<int>(it->second);
            }

            // Walk up from each entity; meeting an entity of the current walk again is a cycle.
            std::vector<int> state(entities.size(), 0); // 0 new, 1 on current walk, 2 finished
            for (std::size_t start = 0; start < entities.size(); ++start)
            {
                std::vector<int> walk;
                int node = static_cast<int>(start);
                while (node >= 0 && state[static_cast<std::size_t>(node)] == 0)
                {
                    state[static_cast<std::size_t>(node)] = 1;
                    walk.push_back(node);
                    node = parent[static_cast<std::size_t>(node)];
                }
                if (node >= 0 && state[static_cast<std::size_t>(node)] == 1)
                {
                    std::string chain;
                    const auto from = std::find(walk.begin(), walk.end(), node);
                    for (auto it = from; it != walk.end(); ++it)
                        chain += entities[static_cast<std::size_t>(*it)].id + " -> ";
                    report.Error("entity.parent.cycle", "entities[" + std::to_string(node) + "].parent",
                                 "parent cycle: " + chain + entities[static_cast<std::size_t>(node)].id);
                }
                for (const int visited : walk)
                    state[static_cast<std::size_t>(visited)] = 2;
            }
        }

        bool ParseScene(const std::filesystem::path &path, const SceneLoadOptions &options, ParsedScene &scene,
                        std::vector<Diagnostic> &diagnostics)
        {
            Report report{path.string(), diagnostics};

            // 1. Read the file (bounded).
            std::error_code ec;
            const std::uintmax_t size = std::filesystem::file_size(path, ec);
            if (ec)
            {
                report.Error("scene.read", "", "cannot read file: " + ec.message());
                return false;
            }
            if (size > maxFileBytes)
            {
                report.Error("scene.limit", "", "file is larger than 64 MiB");
                return false;
            }
            std::ifstream file(path, std::ios::binary);
            const std::string text{std::istreambuf_iterator<char>(file), {}};

            // 2. Parse JSON.
            json root;
            try
            {
                root = json::parse(text);
            }
            catch (const json::exception &error)
            {
                report.Error("scene.parse", "", std::string{"invalid JSON: "} + error.what());
                return false;
            }

            // 3. Header.
            if (!root.is_object() || root.value("format", "") != "umbra.scene")
            {
                report.Error("scene.format", "format", "expected \"format\": \"umbra.scene\"");
                return false;
            }
            const auto version = root.find("version");
            if (version == root.end() || !version->is_number_integer())
            {
                report.Error("scene.version", "version", "missing integer version");
                return false;
            }
            if (version->get<int>() != sceneFormatVersion)
            {
                report.Error("scene.version", "version", "version " + std::to_string(version->get<int>()) +
                                                             " is not supported (this build reads version " +
                                                             std::to_string(sceneFormatVersion) + ")");
                return false;
            }
            const auto entities = root.find("entities");
            if (entities == root.end() || !entities->is_array())
            {
                report.Error("scene.entities", "entities", "expected an array");
                return false;
            }
            if (entities->size() > maxEntities)
            {
                report.Error("scene.limit", "entities", "more than 100000 entities");
                return false;
            }

            // 4. Entities, duplicate ids, hierarchy.
            std::unordered_map<std::string, std::size_t> firstUse;
            for (std::size_t i = 0; i < entities->size(); ++i)
            {
                const std::string where = "entities[" + std::to_string(i) + "]";
                if (!(*entities)[i].is_object())
                {
                    report.Error("entity.type", where, "expected an object");
                    continue;
                }
                EntityDesc desc = ParseEntity((*entities)[i], where, options, report);
                if (!desc.id.empty() && !firstUse.emplace(desc.id, i).second)
                    report.Error("entity.id.duplicate", where + ".id",
                                 "id '" + desc.id + "' is already used by entities[" + std::to_string(firstUse[desc.id]) + "]");
                scene.entities.push_back(std::move(desc));
            }
            CheckHierarchy(scene.entities, report); // run even after other errors: report everything at once
            for (std::size_t i = 0; i < scene.entities.size(); ++i)
            {
                const EntityDesc &desc = scene.entities[i];
                if (desc.collider && desc.collider->motion == world::ColliderMotion::Dynamic && !desc.parent.empty())
                    report.Error("entity.collider.dynamicchild", "entities[" + std::to_string(i) + "].collider",
                                 "dynamic colliders must be on root entities (the simulation owns their transform)");
                if (desc.interactable && !desc.collider)
                    report.Warning("entity.interactable.nocollider", "entities[" + std::to_string(i) + "].interactable",
                                   "the player finds interactables by ray, so this one needs a collider to be usable");
            }

            // 5. Models used by "instance" must load.
            for (std::size_t i = 0; i < scene.entities.size() && !report.failed; ++i)
            {
                const std::string &instance = scene.entities[i].instance;
                if (instance.empty() || scene.instanceModels.contains(instance))
                    continue;
                const assets::Model *model = options.models ? options.models(instance) : nullptr;
                if (model == nullptr)
                    report.Error("instance.failed", "entities[" + std::to_string(i) + "].instance", "model '" + instance + "' could not be loaded");
                scene.instanceModels[instance] = model;
            }
            return !report.failed;
        }

        // Rounds to 6 significant digits so saved files stay readable (0.6, not 0.6000000238).
        double Tidy(float value)
        {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.6g", static_cast<double>(value));
            return std::strtod(buffer, nullptr);
        }

        json EntityJson(const world::World &world, world::Entity entity)
        {
            json out;
            out["id"] = *world.GetId(entity);
            if (!world.GetName(entity)->empty())
                out["name"] = *world.GetName(entity);
            if (const world::Entity parent = world.GetParent(entity))
                out["parent"] = *world.GetId(parent);

            // Only non-default values are written, so files stay short.
            const world::Transform &t = *world.GetLocalTransform(entity);
            if (t.position != glm::vec3{0.0F})
                out["position"] = {Tidy(t.position.x), Tidy(t.position.y), Tidy(t.position.z)};
            if (t.rotation != glm::identity<glm::quat>())
                out["rotation"] = {Tidy(t.rotation.x), Tidy(t.rotation.y), Tidy(t.rotation.z), Tidy(t.rotation.w)};
            if (t.scale != glm::vec3{1.0F})
                out["scale"] = {Tidy(t.scale.x), Tidy(t.scale.y), Tidy(t.scale.z)};
            if (const world::Renderable *r = world.GetRenderable(entity))
            {
                out["model"] = r->model;
                out["mesh"] = r->mesh;
                if (r->color != glm::vec4{1.0F})
                    out["color"] = {Tidy(r->color.r), Tidy(r->color.g), Tidy(r->color.b), Tidy(r->color.a)};
            }
            if (const world::Collider *c = world.GetCollider(entity))
            {
                static const char *shapes[] = {"box", "sphere", "capsule"};
                static const char *motions[] = {"static", "kinematic", "dynamic"};
                json collider{{"shape", shapes[static_cast<int>(c->shape)]}, {"motion", motions[static_cast<int>(c->motion)]}};
                if (c->halfExtents != glm::vec3{0.0F})
                    collider["halfExtents"] = {Tidy(c->halfExtents.x), Tidy(c->halfExtents.y), Tidy(c->halfExtents.z)};
                if (c->radius != 0.0F)
                    collider["radius"] = Tidy(c->radius);
                if (c->halfHeight != 0.0F)
                    collider["halfHeight"] = Tidy(c->halfHeight);
                out["collider"] = collider;
            }
            if (const world::Interactable *i = world.GetInteractable(entity))
            {
                json interactable{{"prompt", i->prompt}};
                if (!i->message.empty())
                    interactable["message"] = i->message;
                if (!i->sound.empty())
                    interactable["sound"] = i->sound;
                if (!i->conversation.empty())
                    interactable["conversation"] = i->conversation;
                if (i->save)
                    interactable["save"] = true;
                out["interactable"] = interactable;
            }
            if (const world::Portal *p = world.GetPortal(entity))
            {
                json portal{{"scene", p->scene}};
                if (p->spawn != "player")
                    portal["spawn"] = p->spawn;
                out["portal"] = portal;
            }
            if (const world::Spawner *sp = world.GetSpawner(entity))
                out["spawner"] = {{"pool", sp->pool}, {"count", sp->count}, {"radius", sp->radius}, {"chase", sp->chase}, {"group", sp->group}};
            return out;
        }

        void AppendSubtree(const world::World &world, world::Entity entity, json &list)
        {
            list.push_back(EntityJson(world, entity));
            for (const world::Entity child : *world.GetChildren(entity))
                AppendSubtree(world, child, list);
        }
    }

    bool ValidateScene(const std::filesystem::path &path, const SceneLoadOptions &options, std::vector<core::Diagnostic> &diagnostics)
    {
        ParsedScene scene;
        return ParseScene(path, options, scene, diagnostics);
    }

    bool LoadScene(const std::filesystem::path &path, const SceneLoadOptions &options, world::World &world,
                   std::vector<core::Diagnostic> &diagnostics)
    {
        ParsedScene scene;
        if (world.Count() != 0)
        {
            diagnostics.push_back({Severity::Error, "scene.target", path.string(), "", "LoadScene needs an empty world"});
            return false;
        }
        if (!ParseScene(path, options, scene, diagnostics))
            return false;

        // Validation passed, so nothing below can fail.
        // 1. Explicit entities first (their ids are known unique), then parents.
        std::vector<world::Entity> created;
        for (const EntityDesc &desc : scene.entities)
        {
            const world::Entity entity = world.Create(desc.id, desc.name);
            world.SetLocalTransform(entity, desc.transform);
            if (!desc.model.empty())
                world.SetRenderable(entity, world::Renderable{desc.model, desc.mesh, desc.color});
            if (desc.collider)
                world.SetCollider(entity, *desc.collider);
            if (desc.interactable)
                world.SetInteractable(entity, *desc.interactable);
            if (desc.portal)
                world.SetPortal(entity, *desc.portal);
            if (desc.spawner)
                world.SetSpawner(entity, *desc.spawner);
            created.push_back(entity);
        }
        for (std::size_t i = 0; i < scene.entities.size(); ++i)
        {
            if (!scene.entities[i].parent.empty())
                world.SetParent(created[i], world.Find(scene.entities[i].parent));
        }

        // 2. Expand instances under their anchor entity (generated ids never clash, see UniqueId).
        for (std::size_t i = 0; i < scene.entities.size(); ++i)
        {
            const std::string &instance = scene.entities[i].instance;
            if (!instance.empty())
                InstantiateModel(world, *scene.instanceModels[instance], instance, scene.entities[i].id, created[i]);
        }
        return true;
    }

    bool SaveScene(const world::World &world, const std::filesystem::path &path, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. Roots in creation order, each followed by its subtree (parents before children).
        json root{{"format", "umbra.scene"}, {"version", sceneFormatVersion}, {"entities", json::array()}};
        world.ForEach([&](world::Entity entity) {
            if (!world.GetParent(entity))
                AppendSubtree(world, entity, root["entities"]);
        });

        // 2. Atomic write: temp file, then rename over the target.
        const std::filesystem::path temp = path.string() + ".tmp";
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file << root.dump(2) << '\n';
            if (!file.flush())
            {
                diagnostics.push_back({Severity::Error, "scene.write", path.string(), "", "cannot write " + temp.string()});
                return false;
            }
        }
        std::error_code ec;
        std::filesystem::rename(temp, path, ec);
        if (ec)
        {
            diagnostics.push_back({Severity::Error, "scene.write", path.string(), "", "cannot replace file: " + ec.message()});
            std::filesystem::remove(temp, ec);
            return false;
        }
        return true;
    }
}
