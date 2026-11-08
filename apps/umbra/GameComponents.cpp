#include "GameComponents.hpp"

#include <algorithm>
#include <map>

namespace umbra::game
{
    namespace
    {
        // A function-local map, so registrations from other files' static objects are safe in any order.
        std::map<std::string, ComponentFactory> &Registry()
        {
            static std::map<std::string, ComponentFactory> registry;
            return registry;
        }
    }

    bool RegisterComponent(const std::string &type, ComponentFactory factory)
    {
        return Registry().emplace(type, std::move(factory)).second;
    }

    bool IsRegistered(const std::string &type)
    {
        return Registry().contains(type);
    }

    std::vector<std::string> ComponentSet::Create(world::World &world)
    {
        items_.clear();
        std::vector<std::string> missing;
        world.ForEach([&](world::Entity entity) {
            for (const world::CustomComponent &custom : *world.GetCustoms(entity))
            {
                const auto factory = Registry().find(custom.type);
                if (factory == Registry().end())
                {
                    if (std::find(missing.begin(), missing.end(), custom.type) == missing.end())
                        missing.push_back(custom.type);
                    continue;
                }
                items_.push_back({entity, factory->second(nlohmann::json::parse(custom.data, nullptr, false))});
            }
        });
        for (Item &item : items_)
        {
            ComponentContext context{world, item.entity, 0.0F};
            item.behaviour->Start(context);
        }
        return missing;
    }

    void ComponentSet::Update(world::World &world, float seconds)
    {
        for (Item &item : items_)
            if (world.IsAlive(item.entity))
            {
                ComponentContext context{world, item.entity, seconds};
                item.behaviour->Update(context);
            }
    }
}
