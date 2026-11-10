#pragma once

#include "GameFlow.hpp"

#include <DrawList.hpp>
#include <Painter.hpp>

#include <vector>

// The screen's answer to a blow (S9, GameFeel.md): a number that floats up from whoever was hit,
// a short freeze and a camera shake on a heavy one, a red flash when the player is hit.
// It only reads impacts and draws; the rules never know about it.
class GameFeel final
{
public:
    void Add(const GameFlow::Impact &impact);
    // Advances the timers with real time; returns the game's time scale (0 during a freeze).
    float Update(float realSeconds);
    // Shakes the camera for this frame (call after the scene set it).
    void Shake(umbra::core::Camera &camera) const;
    // Draws the numbers and the flash on top of the scene (under the game screens).
    void Draw(const umbra::core::Camera &camera, glm::vec2 screen, const umbra::ui::FontAtlas &fonts, umbra::core::TextureHandle atlas,
              umbra::core::UiDrawList &out) const;

private:
    struct Number final
    {
        glm::vec3 at{0.0F};
        int amount = 0;
        bool heal = false;
        bool heavy = false;
        float age = 0.0F;
    };
    std::vector<Number> numbers_;
    float freeze_ = 0.0F; // seconds of hit-stop left
    float shake_ = 0.0F;  // 1 -> 0
    float flash_ = 0.0F;  // 1 -> 0, red over the screen
    float time_ = 0.0F;
};
