#include <World.hpp>

#include <atomic>
#include <utility>

namespace umbra::world
{
    namespace
    {
        // Every world gets a unique id so handles from one world are rejected by another.
        std::atomic<std::uint32_t> nextWorldId{1};
    }

    World::World() : worldId_(nextWorldId.fetch_add(1)) {}
    World::~World() = default;

    Entity World::Create(std::string id, std::string name)
    {
        if (id.empty() || byId_.contains(id))
        {
            return {};
        }
        const Entity entity{entities_.Insert({id, std::move(name)}), worldId_};
        byId_.emplace(std::move(id), entity);
        return entity;
    }

    bool World::Destroy(Entity entity)
    {
        const Record *record = Get(entity);
        if (record == nullptr)
        {
            return false;
        }
        byId_.erase(record->id);
        return entities_.Remove(entity.slot);
    }

    void World::Clear()
    {
        entities_.Clear();
        byId_.clear();
    }

    bool World::IsAlive(Entity entity) const noexcept { return Get(entity) != nullptr; }

    Entity World::Find(std::string_view id) const
    {
        const auto it = byId_.find(std::string{id});
        return it == byId_.end() ? Entity{} : it->second;
    }

    const std::string *World::GetId(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record ? &record->id : nullptr;
    }

    const std::string *World::GetName(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record ? &record->name : nullptr;
    }

    const World::Record *World::Get(Entity entity) const noexcept
    {
        return entity.world == worldId_ ? entities_.Get(entity.slot) : nullptr;
    }
}
