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
    void SetLook(nlohmann::json look); // assets/match-look.json: the grade, shadows, scenes, painted floors
    // Motions (motions.json): a unit plays one (down holds until revive), then rests in idle.
    void Play(int unit, const std::string &motion);
    // Also follows the action (the acting unit, a battle's middle) and opens each turn with a ring.
    void Animate(const umbra::match::MatchState &s, float dt);
    // A scene (MatchTable.md "Scenes"): "open" sweeps down onto the board, "end" circles the winner's hub.
    void StartScene(const std::string &name, int cell) { scene_ = name, sceneT_ = 0.0F, sceneCell_ = cell; }
    [[nodiscard]] bool InScene() const { return !scene_.empty(); }
    void StopFollow() { follow_ = false; } // a drag frees the camera until the next turn
    // The battle view (MatchTable.md): the two fighters stand apart, facing, the camera comes down into their room.
    void SetFight(int attacker, int defender) { fight_[0] = attacker, fight_[1] = defender; }
    // Whom the told line is about (MatchPlay "focus"): the camera frames them all.
    void SetFocus(std::vector<int> units) { focus_ = std::move(units); }

private:
    [[nodiscard]] glm::vec3 Centre(const umbra::match::Board &b, int cell) const;          // a square's middle on the table
    [[nodiscard]] glm::vec3 Feet(const umbra::match::Board &b, const umbra::match::Unit &u) const; // the middle of its footprint
    [[nodiscard]] float Height(const umbra::match::Unit &u, int index) const;
    [[nodiscard]] int Fighter(int unit) const { return unit == staged_[0] ? 0 : unit == staged_[1] ? 1 : -1; }
    int fight_[2] = {-1, -1}, staged_[2] = {-1, -1}; // asked for; the pair being staged (kept while the view eases out)
    float fightT_ = 0.0F, focusT_ = 0.0F;
    std::vector<int> focus_, framed_; // asked for; framed (kept while the camera eases back)
    [[nodiscard]] glm::vec3 FightNear() const; // how far toward the camera the fighters stand on their stage
    void DrawFight(const umbra::match::MatchState &s, umbra::core::DrawList &out) const; // the fight's own stage
    std::map<std::string, umbra::core::TextureHandle> backdrops_; // by square kind (match-look.json battle.backdrops)
    [[nodiscard]] std::optional<glm::vec2> ScreenPoint(glm::vec3 world, glm::vec2 pixels) const;
    void Sprite(int index, const umbra::match::Unit &u, glm::vec3 feet, umbra::core::DrawList &out) const;

    const umbra::match::MatchDef &def_;
    umbra::render::Renderer &renderer_;
    umbra::assets::AssetFolders assets_;
    umbra::core::MeshHandle box_, quad_, mirrorQuad_;
    std::map<std::string, umbra::core::TextureHandle> sprites_; // by "folder/face": body_<face>.png
    umbra::night::Motions motions_;
    std::vector<umbra::night::SpriteState> states_; // by unit
    std::map<std::string, float> aspects_;                      // ... its width per height
    [[nodiscard]] float Look(const char *group, const char *key, float t, float fallback) const; // start..end at t
    void Shadow(glm::vec3 feet, float width, umbra::core::DrawList &out) const;
    umbra::core::MeshHandle disc_; // a flat round disc for shadows
    umbra::core::TextureHandle eventIcon_, chestIcon_; // the optional rooms' marks (art/ui/match/icons)
    void Mark(glm::vec3 at, umbra::core::TextureHandle icon, glm::vec3 color, float strength, umbra::core::DrawList &out) const;
    std::map<std::string, std::vector<umbra::core::TextureHandle>> floors_; // by square kind
    [[nodiscard]] umbra::core::TextureHandle FloorArt(const umbra::match::Board &b, int cell, bool stage) const;
    [[nodiscard]] float RoomT() const; // 0 the board, 1 close enough for walls
    [[nodiscard]] float FigureScale() const; // figures keep their size on screen as the camera zooms
    [[nodiscard]] glm::vec3 Shown(const umbra::match::MatchState &s, int unit) const; // where the figure stands now
    void Walk(const umbra::match::MatchState &s, float dt);
    void Arrow(glm::vec3 tip, float size, umbra::core::DrawList &out) const; // a downward gold triangle facing the camera
    umbra::core::MeshHandle triangle_;
    umbra::core::TextureHandle wallArt_; // match-look.json rooms.wallArt, repeated every wallRepeat metres
    void Edge(glm::vec3 centre, int dir, glm::vec4 color, float glow, umbra::core::DrawList &out) const;
    std::vector<glm::vec3> shown_;            // by unit: the figure's feet, eased along its route
    std::vector<std::vector<glm::vec3>> route_; // by unit: the squares' points still to walk through
    std::vector<int> lastCell_;
    nlohmann::json look_;
    TableCamera view_;
    float clock_ = 0.0F;
    int acting_ = -1, ringUnit_ = -1;
    float ringT_ = 1.0F, sceneT_ = 0.0F;
    bool follow_ = true;
    std::string scene_;
    int sceneCell_ = -1;
};
