#pragma once

#include <SlotMap.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

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
        bool Destroy(Entity entity);
        void Clear();

        [[nodiscard]] bool IsAlive(Entity entity) const noexcept;
        [[nodiscard]] Entity Find(std::string_view id) const;
        [[nodiscard]] const std::string *GetId(Entity entity) const noexcept;
        [[nodiscard]] const std::string *GetName(Entity entity) const noexcept;
        [[nodiscard]] std::size_t Count() const noexcept { return entities_.Size(); }
        [[nodiscard]] std::uint32_t GetWorldId() const noexcept { return worldId_; }

    private:
        struct Record final
        {
            std::string id;
            std::string name;
        };

        [[nodiscard]] const Record *Get(Entity entity) const noexcept;

        std::uint32_t worldId_ = 0;
        core::SlotMap<Record, EntityTag> entities_;
        std::unordered_map<std::string, Entity> byId_;
    };
}
