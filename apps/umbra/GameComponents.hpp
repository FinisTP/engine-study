#pragma once

#include <World.hpp>

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

// C++ behaviour for a project's own components (engine/scene/include/Components.md).
// A scene entity's "components": {"spin": {"speed": 90}} creates the class registered as "spin"
// when the scene loads; Update runs every frame while the scene is active.
namespace umbra::game
{
    struct ComponentContext final
    {
        world::World &world;
        world::Entity entity;
        float seconds = 0.0F; // since the last frame
    };

    class GameComponent
    {
    public:
        virtual ~GameComponent() = default;
        virtual void Start(ComponentContext &) {}
        virtual void Update(ComponentContext &) {}
    };

    // Makes the behaviour from the component's fields (already checked against its schema).
    using ComponentFactory = std::function<std::unique_ptr<GameComponent>(const nlohmann::json &fields)>;

    // Registers a type; returns false if the name is already taken. Use UMBRA_COMPONENT.
    bool RegisterComponent(const std::string &type, ComponentFactory factory);
    [[nodiscard]] bool IsRegistered(const std::string &type);

    // The live behaviours of one world.
    class ComponentSet final
    {
    public:
        // Creates a behaviour for every custom component with a registered type, then calls Start.
        // Returns the types that have no C++ behaviour (reported once by the caller).
        std::vector<std::string> Create(world::World &world);
        void Update(world::World &world, float seconds);
        void Clear() { items_.clear(); }

    private:
        struct Item final
        {
            world::Entity entity;
            std::unique_ptr<GameComponent> behaviour;
        };
        std::vector<Item> items_;
    };
}

// Put in a .cpp of the game or a project (compiled into the game program, not a library, so the
// registration is never dropped by the linker): UMBRA_COMPONENT("spin", Spin);
#define UMBRA_COMPONENT(TYPE, CLASS)                                                                                                       \
    static const bool umbraRegistered##CLASS = ::umbra::game::RegisterComponent(                                                           \
        TYPE, [](const nlohmann::json &fields) -> std::unique_ptr<::umbra::game::GameComponent> { return std::make_unique<CLASS>(fields); })
