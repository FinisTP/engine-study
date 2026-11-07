#include <Roamer.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

namespace umbra::gameplay
{
    bool CanSee(const physics::PhysicsWorld &physics, glm::vec3 fromEye, glm::vec3 toEye, float range)
    {
        const glm::vec3 offset = toEye - fromEye;
        const float distance = glm::length(offset);
        if (distance > range)
            return false;
        if (distance < 1e-3F)
            return true;
        // The player is a character, not a body, so the ray only meets walls, crates and floors.
        const std::optional<physics::RayHit> hit = physics.Raycast(fromEye, offset / distance, distance);
        return !hit;
    }

    bool Roamer::Spawn(physics::PhysicsWorld &physics, glm::vec3 feet)
    {
        physics::CharacterDesc desc;
        desc.position = feet + glm::vec3{0.0F, settings_.radius + settings_.halfHeight, 0.0F};
        desc.radius = settings_.radius;
        desc.halfHeight = settings_.halfHeight;
        character_ = physics.CreateCharacter(desc);
        target_ = feet;
        return static_cast<bool>(character_);
    }

    void Roamer::Despawn(physics::PhysicsWorld &physics)
    {
        if (character_)
            physics.DestroyCharacter(character_);
        character_ = {};
    }

    glm::vec3 Roamer::GetFeet(const physics::PhysicsWorld &physics) const
    {
        const std::optional<physics::CharacterState> state = physics.GetCharacter(character_);
        return state ? state->position - glm::vec3{0.0F, settings_.radius + settings_.halfHeight, 0.0F} : home_;
    }

    void Roamer::Update(physics::PhysicsWorld &physics, glm::vec3 playerFeet, float dt, std::mt19937 &rng)
    {
        if (!character_)
            return;
        const glm::vec3 feet = GetFeet(physics);
        const glm::vec3 eye{0.0F, 0.8F, 0.0F};

        // 1. Chasers start chasing a player they can see, and give up when it gets far away.
        const float toPlayer = glm::length(glm::vec2{playerFeet.x - feet.x, playerFeet.z - feet.z});
        if (chase_ && !chasing_ && CanSee(physics, feet + eye, playerFeet + eye, settings_.sight))
            chasing_ = true;
        else if (chasing_ && toPlayer > settings_.giveUp)
            chasing_ = false;

        // 2. Otherwise wander: a new random point within the home circle every few seconds.
        retarget_ -= dt;
        if (!chasing_ && (retarget_ <= 0.0F || glm::length(glm::vec2{target_.x - feet.x, target_.z - feet.z}) < 0.3F))
        {
            std::uniform_real_distribution<float> unit(0.0F, 1.0F);
            const float angle = unit(rng) * glm::two_pi<float>();
            const float distance = std::sqrt(unit(rng)) * homeRadius_; // sqrt: uniform over the disc
            target_ = home_ + glm::vec3{std::cos(angle) * distance, 0.0F, std::sin(angle) * distance};
            retarget_ = 2.0F + unit(rng) * 3.0F;
        }

        // 3. Walk toward the player or the wander point.
        const glm::vec3 goal = chasing_ ? playerFeet : target_;
        glm::vec3 direction{goal.x - feet.x, 0.0F, goal.z - feet.z};
        const float length = glm::length(direction);
        const float speed = chasing_ ? settings_.chaseSpeed : settings_.walkSpeed;
        const glm::vec3 velocity = length > 0.2F ? direction / length * speed : glm::vec3{0.0F};
        physics.MoveCharacter(character_, velocity, false, dt);
    }
}
