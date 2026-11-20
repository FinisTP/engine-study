#pragma once

#include "CafePlay.hpp"

#include <AssetFolders.hpp>
#include <DrawList.hpp>
#include <Renderer.hpp>

#include <map>
#include <optional>
#include <string>

// The café drawn (CafeRoom.md): the room from room.json as boxes and papered walls, warm lamps,
// the ghosts at their tables and the walker as flat figures, a camera that keeps the walker in view.
class CafeRoom final
{
public:
    CafeRoom(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets, const umbra::cafe::CafeDef &def,
             const umbra::match::MatchDef &match);
    void Update(const umbra::night::CafePlay &play, float dt);
    void Draw(const umbra::night::CafePlay &play, umbra::core::DrawList &out) const;
    // Screen labels: each table's order and patience, the talk's bubble, the walker's name.
    [[nodiscard]] nlohmann::json Labels(const umbra::night::CafePlay &play, glm::vec2 pixels) const;
    // Where the mouse meets the floor, in room metres (x across, y deep); none off the floor.
    [[nodiscard]] std::optional<glm::vec2> Floor(glm::vec2 mouse, glm::vec2 pixels, glm::vec2 roomSize) const;

private:
    [[nodiscard]] umbra::core::Camera Camera() const;
    [[nodiscard]] std::optional<glm::vec2> ScreenPoint(glm::vec3 world, glm::vec2 pixels) const;
    void Figure(const std::string &sprite, glm::vec3 feet, float height, float facing, umbra::core::DrawList &out) const;
    umbra::core::TextureHandle Picture(const std::string &sprite);

    const umbra::cafe::CafeDef &def_;
    const umbra::match::MatchDef &match_;
    umbra::render::Renderer &renderer_;
    umbra::assets::AssetFolders assets_;
    umbra::core::MeshHandle box_, quad_, mirrorQuad_;
    umbra::core::TextureHandle wall_, floor_;
    std::map<std::string, umbra::core::TextureHandle> pictures_; // by sprite id (body_calm)
    std::map<std::string, float> aspects_;
    glm::vec3 look_{6.0F, 0.0F, 4.0F}; // the camera's target, easing after the walker
    glm::vec2 last_{0.0F};
    float facing_ = 1.0F, clock_ = 0.0F, walk_ = 0.0F;
};
