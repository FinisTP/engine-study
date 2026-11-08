#include <World.hpp>

#include <algorithm>
#include <atomic>
#include <utility>

namespace umbra::world
{
    namespace
    {
        // Every world gets a unique id so handles from one world are rejected by another.
        std::atomic<std::uint32_t> nextWorldId{1};

        glm::mat4 ToMatrix(const Transform &t)
        {
            // M = T * R * S (scale first, then rotate, then move)
            glm::mat4 matrix = glm::mat4_cast(t.rotation);
            matrix[0] *= t.scale.x;
            matrix[1] *= t.scale.y;
            matrix[2] *= t.scale.z;
            matrix[3] = glm::vec4(t.position, 1.0F);
            return matrix;
        }
    }

    World::World() : worldId_(nextWorldId.fetch_add(1)) {}
    World::~World() = default;

    Entity World::Create(std::string id, std::string name)
    {
        if (id.empty() || byId_.contains(id))
        {
            return {};
        }
        Record record;
        record.id = id;
        record.name = std::move(name);
        const Entity entity{entities_.Insert(std::move(record)), worldId_};
        byId_.emplace(std::move(id), entity);
        return entity;
    }

    bool World::Destroy(Entity entity)
    {
        Record *record = Get(entity);
        if (record == nullptr)
        {
            return false;
        }

        // 1. Destroy children first (copy: the list shrinks as they detach).
        const std::vector<Entity> children = record->children;
        for (const Entity child : children)
        {
            Destroy(child);
        }

        // 2. Unlink from the parent, then free the slot.
        record = Get(entity);
        Detach(entity, *record);
        byId_.erase(record->id);
        return entities_.Remove(entity.slot);
    }

    void World::Clear()
    {
        entities_.Clear();
        byId_.clear();
        SetEnvironment({});
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

    bool World::SetParent(Entity child, Entity parent)
    {
        Record *record = Get(child);
        if (record == nullptr || (parent && !IsAlive(parent)) || child == parent)
        {
            return false;
        }

        // Walk up from the new parent: finding the child there would make a cycle.
        for (Entity up = parent; up; up = Get(up)->parent)
        {
            if (up == child)
            {
                return false;
            }
        }

        Detach(child, *record);
        record->parent = parent;
        record->dirty = true;
        if (parent)
        {
            Get(parent)->children.push_back(child);
        }
        return true;
    }

    Entity World::GetParent(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record ? record->parent : Entity{};
    }

    const std::vector<Entity> *World::GetChildren(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record ? &record->children : nullptr;
    }

    bool World::SetLocalTransform(Entity entity, const Transform &transform)
    {
        Record *record = Get(entity);
        if (record == nullptr)
        {
            return false;
        }
        record->local = transform;
        record->dirty = true;
        return true;
    }

    const Transform *World::GetLocalTransform(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record ? &record->local : nullptr;
    }

    void World::UpdateTransforms()
    {
        // Start at every root; children are reached through their parents.
        std::vector<Entity> roots;
        ForEach([&](Entity entity) {
            if (!Get(entity)->parent)
            {
                roots.push_back(entity);
            }
        });
        for (const Entity root : roots)
        {
            UpdateRecursive(root, glm::mat4{1.0F}, false);
        }
    }

    const glm::mat4 *World::GetWorldMatrix(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record ? &record->worldMatrix : nullptr;
    }

    bool World::SetRenderable(Entity entity, Renderable renderable)
    {
        Record *record = Get(entity);
        if (record == nullptr)
        {
            return false;
        }
        record->renderable = std::move(renderable);
        return true;
    }

    bool World::RemoveRenderable(Entity entity)
    {
        Record *record = Get(entity);
        if (record == nullptr || !record->renderable)
        {
            return false;
        }
        record->renderable.reset();
        return true;
    }

    const Renderable *World::GetRenderable(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record && record->renderable ? &*record->renderable : nullptr;
    }

    Renderable *World::GetRenderable(Entity entity) noexcept
    {
        return const_cast<Renderable *>(std::as_const(*this).GetRenderable(entity));
    }

    bool World::SetCollider(Entity entity, Collider collider)
    {
        Record *record = Get(entity);
        if (record == nullptr)
            return false;
        record->collider = collider;
        return true;
    }

    bool World::RemoveCollider(Entity entity)
    {
        Record *record = Get(entity);
        if (record == nullptr || !record->collider)
            return false;
        record->collider.reset();
        return true;
    }

    const Collider *World::GetCollider(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record && record->collider ? &*record->collider : nullptr;
    }

    Collider *World::GetCollider(Entity entity) noexcept
    {
        return const_cast<Collider *>(std::as_const(*this).GetCollider(entity));
    }

    bool World::SetInteractable(Entity entity, Interactable interactable)
    {
        Record *record = Get(entity);
        if (record == nullptr)
            return false;
        record->interactable = std::move(interactable);
        return true;
    }

    const Interactable *World::GetInteractable(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record && record->interactable ? &*record->interactable : nullptr;
    }

    bool World::SetSpawner(Entity entity, Spawner spawner)
    {
        Record *record = Get(entity);
        if (record == nullptr)
            return false;
        record->spawner = std::move(spawner);
        return true;
    }

    const Spawner *World::GetSpawner(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record && record->spawner ? &*record->spawner : nullptr;
    }

    bool World::SetPortal(Entity entity, Portal portal)
    {
        Record *record = Get(entity);
        if (record == nullptr)
            return false;
        record->portal = std::move(portal);
        return true;
    }

    const Portal *World::GetPortal(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record && record->portal ? &*record->portal : nullptr;
    }

    bool World::SetLight(Entity entity, Light light)
    {
        Record *record = Get(entity);
        if (record == nullptr)
            return false;
        record->light = light;
        return true;
    }

    const Light *World::GetLight(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record && record->light ? &*record->light : nullptr;
    }

    void World::SetEnvironment(const core::EnvironmentSettings &settings, Entity owner)
    {
        environment_ = settings;
        environmentOwner_ = owner;
        hour_ = settings.hour;
    }

    void World::AdvanceTime(float seconds)
    {
        hour_ = core::AdvanceHour(hour_, seconds, environment_.dayLength);
    }

    bool World::SetCustom(Entity entity, CustomComponent component)
    {
        Record *record = Get(entity);
        if (record == nullptr || component.type.empty())
            return false;
        for (CustomComponent &existing : record->customs)
            if (existing.type == component.type)
            {
                existing = std::move(component);
                return true;
            }
        record->customs.push_back(std::move(component));
        return true;
    }

    const std::vector<CustomComponent> *World::GetCustoms(Entity entity) const noexcept
    {
        const Record *record = Get(entity);
        return record ? &record->customs : nullptr;
    }

    void World::ExtractDrawList(core::DrawList &drawList) const
    {
        entities_.ForEach([&](core::Handle<EntityTag>, const Record &record) {
            if (record.renderable && record.renderable->meshHandle)
            {
                const Renderable &r = *record.renderable;
                drawList.items.push_back({r.meshHandle, r.textureHandle, record.worldMatrix, r.color * r.tint, r.metersPerRepeat, r.emissive});
            }
            if (record.light)
            {
                const Light &l = *record.light;
                drawList.lights.push_back({glm::vec3(record.worldMatrix[3]), l.color, l.intensity, l.range, l.flicker});
            }
        });
        drawList.environment = core::EvaluateEnvironment(environment_, hour_);
    }

    const World::Record *World::Get(Entity entity) const noexcept
    {
        return entity.world == worldId_ ? entities_.Get(entity.slot) : nullptr;
    }

    World::Record *World::Get(Entity entity) noexcept
    {
        return const_cast<Record *>(std::as_const(*this).Get(entity));
    }

    void World::Detach(Entity child, Record &record)
    {
        if (Record *parent = Get(record.parent))
        {
            std::erase(parent->children, child);
        }
        record.parent = {};
        record.dirty = true;
    }

    void World::UpdateRecursive(Entity entity, const glm::mat4 &parentMatrix, bool parentChanged)
    {
        // A node is recomputed if it changed itself or any ancestor changed.
        Record &record = *Get(entity);
        const bool changed = record.dirty || parentChanged;
        if (changed)
        {
            record.worldMatrix = parentMatrix * ToMatrix(record.local);
            record.dirty = false;
        }
        for (const Entity child : record.children)
        {
            UpdateRecursive(child, record.worldMatrix, changed);
        }
    }
}
