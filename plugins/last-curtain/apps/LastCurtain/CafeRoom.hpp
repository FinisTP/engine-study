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
class CampaignRoom;

class CafeRoom final
{
public:
    CafeRoom(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets, const umbra::cafe::CafeDef &def,
             const umbra::match::MatchDef &match);
    ~CafeRoom(); // gives the renderer back its textures and meshes (each café visit builds a new room)
    CafeRoom(const CafeRoom &) = delete;
    CafeRoom &operator=(const CafeRoom &) = delete;
    void Update(const umbra::night::CafePlay &play, float dt);
    void Draw(const umbra::night::CafePlay &play, umbra::core::DrawList &out) const;
    // Screen labels: each table's order and patience, the talk's bubble, the walker's name.
    [[nodiscard]] nlohmann::json Labels(const umbra::night::CafePlay &play, glm::vec2 pixels) const;
    // Where the mouse meets the floor, in room metres (x across, y deep); none off the floor.
    [[nodiscard]] std::optional<glm::vec2> Floor(glm::vec2 mouse, glm::vec2 pixels, glm::vec2 roomSize) const;

    // The campaign's café in the same room (CampaignRoom.hpp): sprites by cast id, the spot E would use.
    void Load(const std::string &sprite) { (void)Picture(sprite); }
    void Follow(glm::vec2 at, glm::vec2 roomSize, float dt); // the camera and the walker's facing
    // A scene in the room: the camera eases close onto a floor point (close 1), or back (close 0).
    void Look(glm::vec2 at, float close, float dt);
    // focus: whoever speaks in a talk; anyone standing in front of them is drawn faded so they stay in view.
    void DrawCampaign(const CampaignRoom &room, const std::map<std::string, std::string> &sprites, const std::string &near, umbra::core::DrawList &out,
                      std::optional<glm::vec2> focus = std::nullopt) const;
    // A point above the floor (metres) on the screen, for a name over a head.
    [[nodiscard]] std::optional<glm::vec2> Above(glm::vec2 floor, float height, glm::vec2 pixels) const;

private:
    [[nodiscard]] umbra::core::Camera Camera() const;
    float close_ = 0.0F; // 0 the walking view, 1 a scene's close-up
    void DrawShell(const nlohmann::json &room, umbra::core::DrawList &out) const; // light, floor, walls, counter, tables
    [[nodiscard]] std::optional<glm::vec2> ScreenPoint(glm::vec3 world, glm::vec2 pixels) const;
    void Figure(const std::string &sprite, glm::vec3 feet, float height, float facing, umbra::core::DrawList &out, float opacity = 1.0F) const;
    umbra::core::TextureHandle Picture(const std::string &sprite);

    const umbra::cafe::CafeDef &def_;
    const umbra::match::MatchDef &match_;
    umbra::render::Renderer &renderer_;
    umbra::assets::AssetFolders assets_;
    umbra::core::MeshHandle box_, quad_, mirrorQuad_;
    umbra::core::TextureHandle wall_, floor_, backWall_; // backWall_: the painted café wall (art/cafe/back_wall.jpg)
    std::map<std::string, umbra::core::TextureHandle> pictures_; // by sprite id (body_calm)
    std::map<std::string, float> aspects_;
    glm::vec3 look_{6.0F, 0.0F, 4.0F}; // the camera's target, easing after the walker
    glm::vec2 last_{0.0F};
    float facing_ = 1.0F, clock_ = 0.0F, walk_ = 0.0F;
};
