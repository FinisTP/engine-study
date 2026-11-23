#pragma once

#include <CampaignCafe.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

// The campaign cafe as data (CafeHub.md): who has something to say, what can be brewed and what it lacks,
// toasts; talks and memory drinks hand back a scene to play, SceneDone settles it. No time, no money.
class CafeHub final
{
public:
    CafeHub(const umbra::match::MatchDef &def, const umbra::campaign::Cast &cast, const umbra::campaign::Recipes &recipes,
            const umbra::campaign::Memories &memories, const umbra::campaign::Talks &talks, std::vector<umbra::campaign::StageDef> stages,
            umbra::campaign::Save &save, int boostCap);

    [[nodiscard]] nlohmann::json Data() const;
    std::string Talk(const std::string &who);                                // the scene to play, "" none
    std::string Brew(const std::string &recipe, const std::string &forWho); // a memory drink: its scene; a boost: "" (done)
    std::string Give(const std::string &keepsake);                          // a keepsake from storage: its memory's scene
    void SceneDone(const umbra::campaign::Facts &chosen);                   // the scene's facts: a memory comes back, a version is picked
    void Update(float dt);

private:
    void Toast(std::string text);
    const umbra::match::MatchDef &def_;
    const umbra::campaign::Cast &cast_;
    const umbra::campaign::Recipes &recipes_;
    const umbra::campaign::Memories &memories_;
    const umbra::campaign::Talks &talks_;
    std::vector<umbra::campaign::StageDef> stages_;
    umbra::campaign::Save &save_;
    int cap_ = 3;
    const umbra::campaign::Memory *pending_ = nullptr; // the memory whose scene is playing
    struct Toasted
    {
        std::string text;
        float left = 3.0F;
    };
    std::vector<Toasted> toasts_;
};
