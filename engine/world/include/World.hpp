#pragma once

#include <SlotMap.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace umbra::world
{
    struct EntityTag;

    // Transient reference to an entity. Valid only in the world that created it.
    struct Entity final
    {
        core::Handle<EntityTag> slot;
        std::uint32_t world = 0;

        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(slot); }
        friend bool operator==(const Entity &, const Entity &) = default;
    };

    // Local transform relative to the parent (or the world, for roots). See Transform.md.
    struct Transform final
    {
        glm::vec3 position{0.0F};
        glm::quat rotation = glm::identity<glm::quat>();
        glm::vec3 scale{1.0F};
    };

    // Owns entities. Each entity has a durable string id (saved in files) and a
    // transient Entity handle (used at runtime). See World.md.
    class World final
    {
    public:
        World();
        ~World();

        World(const World &) = delete;
        World &operator=(const World &) = delete;
        World(World &&) = delete;
        World &operator=(World &&) = delete;

        // Returns an invalid Entity if id is empty or already used.
        [[nodiscard]] Entity Create(std::string id, std::string name = {});
        // Also destroys all children.
        bool Destroy(Entity entity);
        void Clear();

        [[nodiscard]] bool IsAlive(Entity entity) const noexcept;
        [[nodiscard]] Entity Find(std::string_view id) const;
        [[nodiscard]] const std::string *GetId(Entity entity) const noexcept;
        [[nodiscard]] const std::string *GetName(Entity entity) const noexcept;
        [[nodiscard]] std::size_t Count() const noexcept { return entities_.Size(); }
        [[nodiscard]] std::uint32_t GetWorldId() const noexcept { return worldId_; }

        // Hierarchy. An invalid parent makes the entity a root.
        // Rejects dead entities, self-parenting and cycles. Keeps the local transform.
        bool SetParent(Entity child, Entity parent);
        [[nodiscard]] Entity GetParent(Entity entity) const noexcept;
        [[nodiscard]] const std::vector<Entity> *GetChildren(Entity entity) const noexcept;

        bool SetLocalTransform(Entity entity, const Transform &transform);
        [[nodiscard]] const Transform *GetLocalTransform(Entity entity) const noexcept;
        // Recomputes world matrices of changed entities and their descendants.
        void UpdateTransforms();
        // Valid after UpdateTransforms().
        [[nodiscard]] const glm::mat4 *GetWorldMatrix(Entity entity) const noexcept;

        // Calls function(entity) for every live entity.
        template <typename Function>
        void ForEach(Function &&function) const
        {
            entities_.ForEach([&](core::Handle<EntityTag> slot, const Record &) { function(Entity{slot, worldId_}); });
        }

    private:
        struct Record final
        {
            std::string id;
            std::string name;
            Entity parent;
            std::vector<Entity> children;
            Transform local;
            glm::mat4 worldMatrix{1.0F};
            bool dirty = true;
        };

        [[nodiscard]] const Record *Get(Entity entity) const noexcept;
        [[nodiscard]] Record *Get(Entity entity) noexcept;
        void Detach(Entity child, Record &record);
        void UpdateRecursive(Entity entity, const glm::mat4 &parentMatrix, bool parentChanged);

        std::uint32_t worldId_ = 0;
        core::SlotMap<Record, EntityTag> entities_;
        std::unordered_map<std::string, Entity> byId_;
    };
}
