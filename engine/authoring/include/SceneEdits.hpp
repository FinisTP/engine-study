#pragma once

#include <Document.hpp>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <optional>
#include <string>
#include <vector>

namespace umbra::authoring
{
    // Ready-made changes for scene documents (format in engine/scene/include/SceneFormat.md).
    // They only build Change lists; Document::Apply checks and applies them.

    // Index of the entity with this id in "/entities", if any.
    [[nodiscard]] std::optional<std::size_t> FindEntity(const Json &scene, const std::string &id);
    [[nodiscard]] std::string EntityPath(std::size_t index); // "/entities/<index>"
    // `base`, or base_2, base_3 ... whichever is not used yet.
    [[nodiscard]] std::string UniqueEntityId(const Json &scene, const std::string &base);
    // Ids of the entity's children (entities whose "parent" is `id`).
    [[nodiscard]] std::vector<std::string> ChildrenOf(const Json &scene, const std::string &id);

    // A new entity at the end of the list.
    [[nodiscard]] std::vector<Change> AddEntity(const Json &entity);
    // The entity and everything under it (children first in the file would break parents).
    [[nodiscard]] std::vector<Change> RemoveEntity(const Json &scene, const std::string &id);
    // A new id, also updating children that point at it.
    [[nodiscard]] std::vector<Change> RenameEntity(const Json &scene, const std::string &id, const std::string &newId);
    // Position, rotation (quaternion [x, y, z, w] in the file) and scale in one change list.
    [[nodiscard]] std::vector<Change> SetTransform(std::size_t index, const glm::vec3 &position, const glm::quat &rotation,
                                                   const glm::vec3 &scale);
}
