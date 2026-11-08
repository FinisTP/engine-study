// The "music" component (assets/components/music.component.json) is data the game reads when a
// scene goes live (SceneRuntime::Music, Main.cpp crossfades to it). Registered so the game knows
// the type; it has nothing to do per frame.
#include <GameComponents.hpp>

namespace
{
    class Music final : public umbra::game::GameComponent
    {
    public:
        explicit Music(const nlohmann::json &) {}
    };
}

UMBRA_COMPONENT("music", Music);
