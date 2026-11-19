#pragma once

#include "Motion.hpp"
#include "TableCamera.hpp"

#include <Match.hpp>

#include <AssetFolders.hpp>
#include <DrawList.hpp>
#include <Renderer.hpp>

#include <nlohmann/json.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <map>
#include <optional>
#include <string>

// The faction match drawn as a table (MatchTable.md): tiles by kind, the stage glowing, the player's
// reach, targets and range, figures as flat sprites, and the points where each unit's card goes.
class MatchTable final
{
public:
    MatchTable(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets, const umbra::match::MatchDef &def);
    MatchTable(const MatchTable &) = delete;
    MatchTable &operator=(const MatchTable &) = delete;

    // ui: MatchPlay::UiData() (reach, path, targets, armed, the acting unit).
    void Draw(const umbra::match::MatchState &s, const nlohmann::json &ui, int hover, umbra::core::DrawList &out) const;
    // One card per unit on the board, at its screen point above the figure (match-labels.ui.json).
    [[nodiscard]] nlohmann::json Labels(const umbra::match::MatchState &s, glm::vec2 pixels) const;
    [[nodiscard]] int Pick(const umbra::match::Board &b, glm::vec2 mouse, glm::vec2 pixels) const; // the square under the mouse, -1 none
    void SetCamera(float distance, float minDistance, float maxDistance, float tilt) { view_.Set(distance, minDistance, maxDistance, tilt); }
    void Pan(glm::vec2 pixels) { view_.Pan(pixels); }
    void Zoom(float steps) { view_.Zoom(steps); }
    void Update(float dt) { clock_ += dt, view_.Update(dt); }
    void SetLook(nlohmann::json look) { look_ = std::move(look); } // assets/match-look.json
    // Motions (motions.json): a unit plays one (down holds until revive), then rests in idle.
    void Play(int unit, const std::string &motion);
    // Also follows the action (the acting unit, a battle's middle) and opens each turn with a ring.
    void Animate(const umbra::match::MatchState &s, float dt);
    // A scene (MatchTable.md "Scenes"): "open" sweeps down onto the board, "end" circles the winner's hub.
    void StartScene(const std::string &name, int cell) { scene_ = name, sceneT_ = 0.0F, sceneCell_ = cell; }
    [[nodiscard]] bool InScene() const { return !scene_.empty(); }
    void StopFollow() { follow_ = false; } // a drag frees the camera until the next turn

private:
    [[nodiscard]] glm::vec3 Centre(const umbra::match::Board &b, int cell) const;          // a square's middle on the table
    [[nodiscard]] glm::vec3 Feet(const umbra::match::Board &b, const umbra::match::Unit &u) const; // the middle of its footprint
    [[nodiscard]] float Height(const umbra::match::Unit &u) const;
    [[nodiscard]] std::optional<glm::vec2> ScreenPoint(glm::vec3 world, glm::vec2 pixels) const;
    void Sprite(int index, const umbra::match::Unit &u, glm::vec3 feet, umbra::core::DrawList &out) const;

    const umbra::match::MatchDef &def_;
    umbra::core::MeshHandle box_, quad_, mirrorQuad_;
    std::map<std::string, umbra::core::TextureHandle> sprites_; // by "folder/face": body_<face>.png
    umbra::night::Motions motions_;
    std::vector<umbra::night::SpriteState> states_; // by unit
    std::map<std::string, float> aspects_;                      // ... its width per height
    [[nodiscard]] float Look(const char *group, const char *key, float t, float fallback) const; // start..end at t
    void Shadow(glm::vec3 feet, float width, umbra::core::DrawList &out) const;
    umbra::core::MeshHandle disc_; // a flat round disc for shadows
    nlohmann::json look_;
    TableCamera view_;
    float clock_ = 0.0F;
    int acting_ = -1, ringUnit_ = -1;
    float ringT_ = 1.0F, sceneT_ = 0.0F;
    bool follow_ = true;
    std::string scene_;
    int sceneCell_ = -1;
};
