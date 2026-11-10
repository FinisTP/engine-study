#include "GameFeel.hpp"

#include "GameUi.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr float numberSeconds = 0.9F; // how long a number floats
    constexpr float freezeSeconds = 0.07F; // hit-stop on a heavy blow
    constexpr float shakeSeconds = 0.25F, shakeMeters = 0.12F;
    constexpr float flashSeconds = 0.3F;
}

void GameFeel::Add(const GameFlow::Impact &impact)
{
    numbers_.push_back({impact.at, impact.amount, impact.heal, impact.heavy, 0.0F});
    if (impact.heal)
        return;
    if (impact.heavy)
    {
        freeze_ = freezeSeconds;
        shake_ = 1.0F;
    }
    if (impact.onPlayer)
        flash_ = 1.0F;
}

float GameFeel::Update(float realSeconds)
{
    time_ += realSeconds;
    for (Number &number : numbers_)
        number.age += realSeconds;
    std::erase_if(numbers_, [](const Number &number) { return number.age >= numberSeconds; });
    shake_ = std::max(0.0F, shake_ - realSeconds / shakeSeconds);
    flash_ = std::max(0.0F, flash_ - realSeconds / flashSeconds);
    const bool frozen = freeze_ > 0.0F;
    freeze_ = std::max(0.0F, freeze_ - realSeconds);
    return frozen ? 0.0F : 1.0F;
}

void GameFeel::Shake(umbra::core::Camera &camera) const
{
    // Two fast sine waves that die away (GameFeel.md): eye and target move together.
    const glm::vec3 offset = glm::vec3{std::sin(time_ * 71.0F), std::sin(time_ * 97.0F + 1.3F), 0.0F} * (shakeMeters * shake_ * shake_);
    camera.position += offset;
    camera.target += offset;
}

void GameFeel::Draw(const umbra::core::Camera &camera, glm::vec2 screen, const umbra::ui::FontAtlas &fonts, umbra::core::TextureHandle atlas,
                    umbra::core::UiDrawList &out) const
{
    umbra::ui::Painter painter(out, fonts, atlas);
    // 1. The red flash, strongest at the start.
    if (flash_ > 0.0F)
        painter.Fill({0.0F, 0.0F, screen.x, screen.y}, {0.8F, 0.05F, 0.05F, 0.25F * flash_});
    // 2. Numbers rise 0.6 m and fade in their last third; heavy ones are gold, in the strong font.
    for (const Number &number : numbers_)
    {
        const float t = number.age / numberSeconds;
        const auto at = GameUi::ScreenPoint(camera, screen, number.at + glm::vec3{0.0F, 0.3F + 0.6F * t, 0.0F});
        const umbra::ui::Face *strong = fonts.Find("strong");
        const umbra::ui::Face *face = number.heavy && strong ? strong : fonts.Find("body");
        if (!at || face == nullptr)
            continue;
        const float alpha = std::min(1.0F, (1.0F - t) * 3.0F);
        const glm::vec4 color = number.heal ? glm::vec4{0.45F, 1.0F, 0.5F, alpha} : number.heavy ? glm::vec4{1.0F, 0.8F, 0.25F, alpha} : glm::vec4{1.0F, 1.0F, 1.0F, alpha};
        const std::string text = (number.heal ? "+" : "") + std::to_string(number.amount);
        const umbra::ui::Rect box{at->x - 100.0F, at->y - 20.0F, 200.0F, 40.0F};
        painter.Text(*face, {box.x + 2.0F, box.y + 2.0F, box.w, box.h}, text, {0.0F, 0.0F, 0.0F, alpha * 0.7F}, false, 0.5F); // shadow
        painter.Text(*face, box, text, color, false, 0.5F);
    }
}
