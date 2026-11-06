#pragma once

#include <DrawList.hpp>
#include <SlotMap.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
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

    // Something to draw. model/mesh are durable (saved); the handles are
    // transient and filled in at runtime once the asset is loaded.
    struct Renderable final
    {
        std::string model;
        std::uint32_t mesh = 0;
        glm::vec4 color{1.0F};
        core::MeshHandle meshHandle;
        core::TextureHandle textureHandle;
    };

    // Physics description of an entity. Pure data: the gameplay layer turns it into a body.
    // Zero sizes mean "derive from the entity's world scale" (see Collider in SceneFormat.md).
    enum class ColliderShape : std::uint8_t
    {
        Box,
        Sphere,
        Capsule
    };

    enum class ColliderMotion : std::uint8_t
    {
        Static,
        Kinematic,
        Dynamic
    };

    struct Collider final
    {
        ColliderShape shape = ColliderShape::Box;
        ColliderMotion motion = ColliderMotion::Static;
        glm::vec3 halfExtents{0.0F}; // Box
        float radius = 0.0F;         // Sphere, Capsule
        float halfHeight = 0.0F;     // Capsule
        core::BodyHandle body;       // transient, set by the gameplay layer
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

        bool SetRenderable(Entity entity, Renderable renderable);
        bool RemoveRenderable(Entity entity);
        [[nodiscard]] const Renderable *GetRenderable(Entity entity) const noexcept;
        [[nodiscard]] Renderable *GetRenderable(Entity entity) noexcept;
        bool SetCollider(Entity entity, Collider collider);
        bool RemoveCollider(Entity entity);
        [[nodiscard]] const Collider *GetCollider(Entity entity) const noexcept;
        [[nodiscard]] Collider *GetCollider(Entity entity) noexcept;

        // Appends one item per renderable with a valid mesh handle. Call after UpdateTransforms().
        void ExtractDrawList(core::DrawList &drawList) const;

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
            std::optional<Renderable> renderable;
            std::optional<Collider> collider;
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
