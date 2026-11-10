#pragma once

#include <Animator.hpp>
#include <DrawList.hpp>
#include <SlotMap.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
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
        std::string material;         // optional "materials/x.material.json": replaces the model's texture
        glm::vec4 tint{1.0F};         // from the material, multiplied with color
        float metersPerRepeat = 0.0F; // from the material: > 0 = world-space texture coordinates
        float emissive = 0.0F;        // glows by itself: color * texture * emissive is added (flames, crystals)
        std::string surface;          // from the material: custom surface shader name ("" = plain)
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

    // Something the player can use with the interact key. Needs a collider to be found.
    struct Interactable final
    {
        std::string prompt;  // shown while in reach, e.g. "Read sign"
        std::string message; // shown after using it
        std::string sound;   // optional sound file played on use
        std::string conversation; // starts this conversation (rpg content id) instead of a message
        bool save = false;        // a save stone: writes the manual save
    };

    // An enemy area: `count` roamers of random actors from `pool` wander within `radius` of the
    // entity; touching one starts a fight with up to `group` enemies from the pool.
    struct Spawner final
    {
        std::vector<std::string> pool;
        int count = 1;
        float radius = 3.0F;
        bool chase = false; // roamers chase the player on sight
        int group = 3;
    };

    // Walking into this entity's box (its transform applied to the -1..1 cube) moves the
    // player to another scene, at the entity with id `spawn` there.
    struct Portal final
    {
        std::string scene; // asset path of the destination scene
        std::string spawn = "player";
    };

    // A point light on this entity (the "light" component, Lighting.md). It shines from the
    // entity's world position.
    struct Light final
    {
        glm::vec3 color{1.0F, 0.75F, 0.45F};
        float intensity = 3.0F;
        float range = 8.0F;
        float flicker = 0.0F;
    };

    // A project's own component (see Components.md): its type name and its fields as JSON text.
    // The world only stores it; the game creates the C++ behaviour registered for the type.
    struct CustomComponent final
    {
        std::string type; // e.g. "spin"
        std::string data; // e.g. {"speed":45}
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
        bool SetInteractable(Entity entity, Interactable interactable);
        [[nodiscard]] const Interactable *GetInteractable(Entity entity) const noexcept;
        bool SetSpawner(Entity entity, Spawner spawner);
        [[nodiscard]] const Spawner *GetSpawner(Entity entity) const noexcept;
        bool SetPortal(Entity entity, Portal portal);
        [[nodiscard]] const Portal *GetPortal(Entity entity) const noexcept;
        bool SetLight(Entity entity, Light light);
        [[nodiscard]] const Light *GetLight(Entity entity) const noexcept;
        // Skeletal animation (S8): the graph file named by the "animation" component, and the
        // animator built for it once the model is loaded (scene::BindAnimators).
        bool SetAnimationGraph(Entity entity, std::string graph);
        [[nodiscard]] const std::string *GetAnimationGraph(Entity entity) const noexcept;
        bool SetAnimator(Entity entity, std::optional<core::Animator> animator);
        [[nodiscard]] core::Animator *GetAnimator(Entity entity) noexcept;
        // Advances every animator (call once per frame, before ExtractDrawList).
        void UpdateAnimation(float seconds);
        // The scene's sun, sky and fog (the "environment" component) and the time of day.
        // `owner` is the entity whose component it is (saved there again); none = defaults.
        void SetEnvironment(const core::EnvironmentSettings &settings, Entity owner = {});
        [[nodiscard]] const core::EnvironmentSettings &GetEnvironment() const noexcept { return environment_; }
        [[nodiscard]] Entity EnvironmentOwner() const noexcept { return environmentOwner_; }
        // The scene's post-processing (the "post" component, Post.md) and its LUT image if any.
        void SetPost(const core::PostSettings &settings, std::shared_ptr<const core::TextureData> lut = {}, Entity owner = {});
        [[nodiscard]] const core::PostSettings &GetPost() const noexcept { return post_; }
        [[nodiscard]] Entity PostOwner() const noexcept { return postOwner_; }
        [[nodiscard]] float Hour() const noexcept { return hour_; }
        void SetHour(float hour) noexcept { hour_ = hour; }
        // Moves the time of day on by `seconds` of play (no change if the day length is 0).
        void AdvanceTime(float seconds);
        // Replaces a custom component of the same type, or adds it.
        bool SetCustom(Entity entity, CustomComponent component);
        [[nodiscard]] const std::vector<CustomComponent> *GetCustoms(Entity entity) const noexcept;

        // Appends one item per renderable with a valid mesh handle, one light per light component,
        // and sets the environment for the current hour. Call after UpdateTransforms().
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
            std::optional<Interactable> interactable;
            std::optional<Portal> portal;
            std::optional<Spawner> spawner;
            std::optional<Light> light;
            std::string animationGraph;
            std::optional<core::Animator> animator;
            std::vector<CustomComponent> customs;
        };

        [[nodiscard]] const Record *Get(Entity entity) const noexcept;
        [[nodiscard]] Record *Get(Entity entity) noexcept;
        void Detach(Entity child, Record &record);
        void UpdateRecursive(Entity entity, const glm::mat4 &parentMatrix, bool parentChanged);

        std::uint32_t worldId_ = 0;
        core::SlotMap<Record, EntityTag> entities_;
        std::unordered_map<std::string, Entity> byId_;
        core::EnvironmentSettings environment_;
        Entity environmentOwner_;
        core::PostSettings post_;
        std::shared_ptr<const core::TextureData> postLut_;
        Entity postOwner_;
        float hour_ = environment_.hour;
    };
}
