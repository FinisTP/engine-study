#pragma once

#include "Motion.hpp"
#include "TableCamera.hpp"

#include <array>
#include <Match.hpp>

#include <AssetFolders.hpp>
#include <DrawList.hpp>
#include <Renderer.hpp>

#include <nlohmann/json.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <map>
#include <set>
#include <optional>
#include <string>

// The faction match drawn as a table (MatchTable.md): tiles by kind, the stage glowing, the player's
// reach, targets and range, figures as flat sprites, and the points where each unit's card goes.
class MatchTable final
{
public:
    MatchTable(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets, const umbra::match::MatchDef &def);
    ~MatchTable(); // gives the renderer back every texture and mesh (a retry builds a new table)
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
    void LookAt(const umbra::match::MatchState &s, int unit); // the camera glides to it (frees it from following)
    void CloseUp(bool on); // a scene's camera: down close; off goes back to the distance before
    [[nodiscard]] std::optional<glm::vec2> HeadOnScreen(const umbra::match::MatchState &s, int unit, glm::vec2 pixels) const;
    // The battle view (MatchTable.md): the two fighters stand apart, facing, the camera comes down into their room.
    void SetFight(const std::vector<int> &places, int picking); // by place, two a group (MatchPlay "fight"); empty: none
    // From MatchPlay: the fallen not yet told (still drawn), and HP as told (the cards over figures).
    void SetTold(std::vector<int> standing, std::vector<int> hp) { standing_ = std::move(standing), toldHp_ = std::move(hp); }
    // Whom the told line is about (MatchPlay "focus"): the camera frames them all.
    void SetFocus(std::vector<int> units) { focus_ = std::move(units); }
    // A campaign stage: each unit's shown name and height (1 a person, 0.8 a ghost); empty keeps the match's.
    // A stage's cast: names, ghost heights and each unit's own figure ("" keeps its class's), loaded on first use.
    void SetCast(std::vector<std::string> names, std::vector<float> heights, std::vector<std::string> figures = {});

private:
    [[nodiscard]] glm::vec3 Centre(const umbra::match::Board &b, int cell) const;          // a square's middle on the table
    [[nodiscard]] glm::vec3 Feet(const umbra::match::Board &b, const umbra::match::Unit &u) const; // the middle of its footprint
    [[nodiscard]] float Height(const umbra::match::Unit &u, int index) const;
    [[nodiscard]] int Fighter(int unit) const; // its place on the stage, -1 none
    [[nodiscard]] bool Staged() const { return staged_[0] >= 0 && staged_[2] >= 0; }
    [[nodiscard]] bool Present(const umbra::match::MatchState &s, int unit) const; // on the board, or fallen but not yet told
    [[nodiscard]] glm::vec3 Head(const umbra::match::MatchState &s, int unit) const; // the top of its figure, as drawn
    [[nodiscard]] glm::vec3 SpriteUp() const;
    std::array<int, 4> fight_{-1, -1, -1, -1}, staged_{-1, -1, -1, -1}; // asked for; being staged (kept while the view eases out)
    int picking_ = -1, stageCell_ = -1;
    glm::vec3 stageMiddle_{0.0F}; // where the stage stands: fixed when the fight begins
    std::vector<int> standing_, toldHp_;
    std::vector<std::string> names_;
    std::vector<float> heights_;
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
    std::vector<std::string> figures_;                          // per unit, from SetCast
    std::set<std::string> tried_;                               // folders already loaded (or missing)
    void Load(const std::string &sprite);
    umbra::night::Motions motions_;
    std::vector<umbra::night::SpriteState> states_; // by unit
    std::map<std::string, float> feet_;                         // where its feet are across the picture (FootCentre)
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
    float before_ = -1.0F; // the seat's distance before a close-up
    float clock_ = 0.0F;
    int acting_ = -1, ringUnit_ = -1;
    float ringT_ = 1.0F, sceneT_ = 0.0F;
    bool follow_ = true;
    std::string scene_;
    int sceneCell_ = -1;
};
