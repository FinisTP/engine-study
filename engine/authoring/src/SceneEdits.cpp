#include <SceneEdits.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::authoring
{
    namespace
    {
        // Round to 1/10000 so files stay readable ("0.3", not "0.30000001").
        double Tidy(float value) { return std::round(static_cast<double>(value) * 10000.0) / 10000.0; }
    }

    std::optional<std::size_t> FindEntity(const Json &scene, const std::string &id)
    {
        const Json &entities = scene.at("entities");
        for (std::size_t i = 0; i < entities.size(); ++i)
            if (entities[i].value("id", "") == id)
                return i;
        return std::nullopt;
    }

    std::string EntityPath(std::size_t index) { return "/entities/" + std::to_string(index); }

    std::string UniqueEntityId(const Json &scene, const std::string &base)
    {
        std::string id = base;
        for (int n = 2; FindEntity(scene, id); ++n)
            id = base + "_" + std::to_string(n);
        return id;
    }

    std::vector<std::string> ChildrenOf(const Json &scene, const std::string &id)
    {
        std::vector<std::string> children;
        for (const Json &entity : scene.at("entities"))
            if (entity.value("parent", "") == id)
                children.push_back(entity.value("id", ""));
        return children;
    }

    std::vector<Change> AddEntity(const Json &entity) { return {{Change::Op::Insert, "/entities/-", entity}}; }

    std::vector<Change> RemoveEntity(const Json &scene, const std::string &id)
    {
        // 1. Collect the entity and all its descendants.
        std::vector<std::size_t> indices;
        std::vector<std::string> pending{id};
        while (!pending.empty())
        {
            const std::string current = pending.back();
            pending.pop_back();
            if (const std::optional<std::size_t> index = FindEntity(scene, current))
                indices.push_back(*index);
            for (const std::string &child : ChildrenOf(scene, current))
                pending.push_back(child);
        }
        // 2. Remove from the back, so earlier indices stay valid.
        std::sort(indices.rbegin(), indices.rend());
        std::vector<Change> changes;
        for (std::size_t index : indices)
            changes.push_back({Change::Op::Remove, EntityPath(index), {}});
        return changes;
    }

    std::vector<Change> RenameEntity(const Json &scene, const std::string &id, const std::string &newId)
    {
        const std::optional<std::size_t> index = FindEntity(scene, id);
        if (!index)
            return {};
        std::vector<Change> changes{{Change::Op::Set, EntityPath(*index) + "/id", newId}};
        const Json &entities = scene.at("entities");
        for (std::size_t i = 0; i < entities.size(); ++i)
            if (entities[i].value("parent", "") == id)
                changes.push_back({Change::Op::Set, EntityPath(i) + "/parent", newId});
        return changes;
    }

    std::vector<Change> SetTransform(std::size_t index, const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale)
    {
        const std::string at = EntityPath(index);
        return {
            {Change::Op::Set, at + "/position", Json::array({Tidy(position.x), Tidy(position.y), Tidy(position.z)})},
            {Change::Op::Set, at + "/rotation", Json::array({Tidy(rotation.x), Tidy(rotation.y), Tidy(rotation.z), Tidy(rotation.w)})},
            {Change::Op::Set, at + "/scale", Json::array({Tidy(scale.x), Tidy(scale.y), Tidy(scale.z)})},
        };
    }
}
