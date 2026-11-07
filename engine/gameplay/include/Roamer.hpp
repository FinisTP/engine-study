#pragma once

#include <Physics.hpp>

#include <glm/vec3.hpp>

#include <random>
#include <string>

namespace umbra::gameplay
{
    struct RoamerSettings final
    {
        float walkSpeed = 1.5F;  // wandering, m/s
        float chaseSpeed = 3.2F; // slower than the player's sprint, so fleeing works
        float sight = 8.0F;      // meters
        float giveUp = 12.0F;    // stop chasing beyond this distance
        float radius = 0.35F;
        float halfHeight = 0.45F;
    };

    // A dungeon enemy walking around its spawner (a physics character, so it can't walk through
    // walls). Wanders between random points near home; a chaser runs at the player it can see.
    // Rules in Roamer.md.
    class Roamer final
    {
    public:
        Roamer(std::string actor, glm::vec3 home, float homeRadius, bool chase, RoamerSettings settings = {})
            : actor_(std::move(actor)), home_(home), homeRadius_(homeRadius), chase_(chase), settings_(settings) {}

        bool Spawn(physics::PhysicsWorld &physics, glm::vec3 feet);
        void Despawn(physics::PhysicsWorld &physics);
        // One fixed step: pick a direction, walk. `rng` makes wandering repeatable in tests.
        void Update(physics::PhysicsWorld &physics, glm::vec3 playerFeet, float dt, std::mt19937 &rng);

        [[nodiscard]] const std::string &Actor() const noexcept { return actor_; }
        [[nodiscard]] glm::vec3 GetFeet(const physics::PhysicsWorld &physics) const;
        [[nodiscard]] bool IsChasing() const noexcept { return chasing_; }
        [[nodiscard]] bool IsSpawned() const noexcept { return static_cast<bool>(character_); }

    private:
        std::string actor_;
        glm::vec3 home_;
        float homeRadius_;
        bool chase_;
        RoamerSettings settings_;
        core::CharacterHandle character_;
        glm::vec3 target_{0.0F};
        float retarget_ = 0.0F; // seconds until a new wander target
        bool chasing_ = false;
    };

    // True if nothing solid blocks the straight line between two eye positions within `range`.
    [[nodiscard]] bool CanSee(const physics::PhysicsWorld &physics, glm::vec3 fromEye, glm::vec3 toEye, float range);
}
