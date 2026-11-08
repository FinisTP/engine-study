// Example project component: turns its entity around the up axis.
// Fields come from assets/components/spin.component.json ("speed" in degrees per second).
#include <GameComponents.hpp>

#include <glm/gtc/quaternion.hpp>

namespace
{
    class Spin final : public umbra::game::GameComponent
    {
    public:
        explicit Spin(const nlohmann::json &fields) : speed_(fields.value("speed", 45.0F)) {}

        void Update(umbra::game::ComponentContext &context) override
        {
            umbra::world::Transform t = *context.world.GetLocalTransform(context.entity);
            t.rotation = glm::angleAxis(glm::radians(speed_ * context.seconds), glm::vec3{0.0F, 1.0F, 0.0F}) * t.rotation;
            context.world.SetLocalTransform(context.entity, t);
        }

    private:
        float speed_;
    };
}

UMBRA_COMPONENT("spin", Spin);
