#include "Unease.hpp"

#include <algorithm>
#include <cmath>

float Unease::Next(float least, float most)
{
    // xorshift32: the same unease on every run.
    random_ ^= random_ << 13, random_ ^= random_ >> 17, random_ ^= random_ << 5;
    return least + (most - least) * static_cast<float>(random_ & 0xFFFF) / 65535.0F;
}

void Unease::Update(float dt, bool enemyTurn, bool heavy, bool reduced, int stripSize)
{
    clock_ += dt;
    // 1. The tear: an enemy's turn or a heavy blow; it fades in about a third of a second.
    if (enemyTurn || heavy)
        tear = std::max(tear, (heavy ? 0.6F : 0.45F) * (reduced ? 0.4F : 1.0F));
    tear = std::max(0.0F, tear - dt * 1.8F);
    // 2. The lamps: every 8-14 s they stutter for a third of a second (darker, unevenly).
    if ((lampIn_ -= dt) <= 0.0F)
        lampLeft_ = 0.35F, lampIn_ = Next(8.0F, 14.0F);
    lampLeft_ = std::max(0.0F, lampLeft_ - dt);
    dim = lampLeft_ > 0.0F ? (reduced ? 0.25F : 0.6F) * std::abs(std::sin(clock_ * 37.0F)) + 0.01F : 0.0F;
    // 3. A portrait: every 9-16 s one of the waiting ones looks wrong for an eighth of a second.
    if ((faceIn_ -= dt) <= 0.0F && stripSize > 1)
        faceLeft_ = 0.12F, faceIn_ = Next(9.0F, 16.0F) * (reduced ? 2.0F : 1.0F), twitch = 1 + static_cast<int>(Next(0.0F, 0.999F) * static_cast<float>(stripSize - 1));
    if ((faceLeft_ -= dt) <= 0.0F)
        twitch = -1;
}
