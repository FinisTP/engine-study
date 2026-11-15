#pragma once

#include <BoardView.hpp>

#include <Animator.hpp>
#include <AssetFolders.hpp>
#include <DrawList.hpp>
#include <Model.hpp>
#include <Renderer.hpp>

#include <glm/vec2.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

// The board drawn as a table top (Table.md): placeholder boxes for floors, walls and monsters,
// tinted animated figures for the pawns, one warm light, and a tilted camera that eases to where the action is.
class Table final
{
public:
    Table(umbra::render::Renderer &renderer, const umbra::assets::AssetFolders &assets);
    ~Table();
    Table(const Table &) = delete;
    Table &operator=(const Table &) = delete;

    struct Slide
    {
        int guest = -1, from = -1, to = -1;
        float t = 0.0F; // 0 to 1
    };
    // path: the walk being picked, gold up to `affordable` steps, red after; slide: a pawn between two cells.
    // rings: (guest, group number) from Play::Rings; numbers 1-3 have their own colour.
    void Draw(const umbra::night::NightDef &def, const umbra::night::BoardView &view, const std::vector<int> &path, int affordable,
              const Slide &slide, const std::vector<std::pair<int, int>> &rings, umbra::core::DrawList &out) const;
    // The top of a guest's pawn as drawn (a label sits above it), and a table point in render pixels.
    [[nodiscard]] glm::vec3 Above(const umbra::night::NightDef &def, const umbra::night::BoardView &view, int guest, const Slide &slide) const;
    [[nodiscard]] std::optional<glm::vec2> ScreenPoint(glm::vec3 world, glm::vec2 pixels) const;
    void SetHunt(bool hunt) { hunt_ = hunt; } // after the Turn the ambient turns red
    // The night's mood (Art.hpp NightMood): blood on the floors, how much the lamp flickers.
    void SetMood(float blood, float flicker) { blood_ = blood, flicker_ = flicker; }
    void Aim(glm::vec3 target, float seconds);
    void Update(float dt);
    // The room at full size (plan 4b/4c, Table.md "The stage"), drawn instead of the table.
    struct StageActor
    {
        int guest = -1, monsterKind = -1, side = 0;
        bool taken = false;
    };
    void DrawStage(const umbra::night::NightDef &def, const umbra::night::BoardView &view, int cell, const std::vector<StageActor> &actors,
                   umbra::core::DrawList &out) const;
    // A click on the stage floor: the point, and the doorway's side (NESW) or -1.
    bool StagePick(const umbra::night::BoardView &view, int cell, glm::vec2 mouse, glm::vec2 pixels, glm::vec3 &floor, int &door) const;
    // The player's figure on the stage: where it stands, where it walks to (Look).
    void StageStart(int guest);
    void WalkTo(glm::vec3 floor) { walkTarget_ = floor; }
    // Clips from new log entries: attack, hit, power; down and dead lie down until woken.
    void React(const std::vector<umbra::night::LogEntry> &entries);
    void NewNight() { pawns_.clear(), stageGuest_ = -1; }
    // The painted pictures (Art.md), once; a missing one leaves its box (logged).
    void LoadArt(const umbra::night::NightDef &def, const umbra::assets::AssetFolders &assets);
    // Tonight's featured cells (FeaturedCells), set when a night starts.
    void SetFeatured(std::vector<bool> featured) { featured_ = std::move(featured); }
    // The figures: a sliding pawn faces its step and walks, the others stand idle (Table.md).
    void Animate(const umbra::night::NightDef &def, const umbra::night::BoardView &view, const Slide &slide, float dt);
    void Zoom(float steps);
    void Pan(glm::vec2 pixels); // a drag: the table moves with the mouse (Table.md)
    [[nodiscard]] umbra::core::Camera Camera() const;
    // The cell under the mouse (render pixels), -1 if none.
    [[nodiscard]] int Pick(const umbra::night::NightDef &def, glm::vec2 mouse, glm::vec2 pixels) const;

private:
    // A rigged model on the GPU and its animation graph; no mesh = draw a box instead.
    struct Figure
    {
        std::shared_ptr<const umbra::assets::Model> model;
        std::shared_ptr<const umbra::core::AnimGraph> graph;
        umbra::core::MeshHandle mesh;
        umbra::core::TextureHandle texture;
        float scale = 0.5F; // the packed model is 2 units tall
    };
    struct Pawn
    {
        std::optional<umbra::core::Animator> animator;
        bool taken = false;
        float yaw = 3.14159265F; // facing the camera (+Z)
    };
    Figure Load(const std::filesystem::path &model, const std::filesystem::path &graph, float scale);
    static umbra::core::Animator MakeAnimator(const Figure &figure);
    static int Crowd(const umbra::night::BoardView &view, int cell); // pawns standing in the cell
    static glm::vec3 Place(const umbra::night::NightDef &def, const umbra::night::BoardView &view, int guest, const Slide &slide);
    umbra::render::Renderer &renderer_;
    umbra::core::MeshHandle box_; // a 1 m cube centred on the origin
    Figure guest_, taken_;        // one guest model for all seven, one Taken; tinted by guests.csv's color
    std::vector<Pawn> pawns_;     // by guest
    std::vector<umbra::core::TextureHandle> roomArt_; // by rooms.csv row; empty handle = a box
    std::vector<umbra::core::TextureHandle> cardArt_; // by monsters.csv row (minions too)
    std::vector<umbra::core::TextureHandle> itemArt_; // by items.csv row
    std::vector<bool> featured_;                      // by cell: a gold frame
    int stageGuest_ = -1;                             // the figure that walks on the stage
    glm::vec3 walkAt_{0.0F}, walkTarget_{0.0F};
    [[nodiscard]] umbra::core::Camera StageCamera() const;
    glm::vec3 from_{0.0F}, to_{0.0F};
    float seconds_ = 0.0F, elapsed_ = 0.0F;
    float distance_ = 11.0F;
    bool hunt_ = false;
    float blood_ = 0.0F, flicker_ = 0.0F, clock_ = 0.0F; // clock_: seconds, for the flicker and pulse
    [[nodiscard]] float Lamp() const;                    // the lamp's strength now (flicker)
    [[nodiscard]] glm::vec3 HuntTint() const;            // the ambient's tint now (the hunt pulses red)
    void Floor(umbra::core::DrawList &out, umbra::core::TextureHandle art, const glm::mat4 &model, glm::vec4 color) const;
};
