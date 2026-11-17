#pragma once

#include "Motion.hpp"
#include "TableCamera.hpp"

#include <BoardView.hpp>

#include <Animator.hpp>
#include <AssetFolders.hpp>
#include <DrawList.hpp>
#include <Model.hpp>
#include <Renderer.hpp>

#include <glm/vec2.hpp>

#include <map>
#include <memory>
#include <string>
#include <array>
#include <optional>
#include <utility>
#include <vector>

// The board drawn as a table top (Table.md): placeholder boxes for floors, walls and monsters,
// guests as flat sprites moved by motions (Motions.md), the Taken as an animated figure, one warm light, and a tilted camera that eases to where the action is.
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
    // readable: the glow of walls, figures and items (Art.hpp Readability), so they read under the mood.
    void SetMood(float blood, float flicker, float readable) { blood_ = blood, flicker_ = flicker, readable_ = readable; }
    void Aim(glm::vec3 target, float seconds) { view_.Aim(target, seconds); }
    // A scene's shot: the camera at `distance`, turned `yaw` degrees around its target, `tilt` above it.
    void SetShot(bool on, float distance, float yaw, float tilt) { view_.SetShot(on, distance, yaw, tilt); }
    void Gesture(int guest, const std::string &clip); // a figure plays a clip (talk, attack, power, hit)
    void Update(float dt);
    // The room view (Table.md "Room view"): every opened room at full size, 8 m apart where its tile
    // lies on the table; the camera zooms between the two views. Draw draws whichever is on.
    void SetRoomView(bool on) { view_.SetRoomView(on); }
    [[nodiscard]] bool InRoomView() const { return view_.RoomT() > 0.0F; }
    struct StageActor
    {
        int guest = -1, monsterKind = -1, side = 0;
        bool taken = false;
    };
    // A battle in sight: its room, and its sides standing apart (Play::StageActors); -1: none.
    void SetBattle(int cell, std::vector<StageActor> actors) { battleCell_ = cell, battleActors_ = std::move(actors); }
    // What a click in the room view lands on: a guest, a monster (state.monsters index), a floor item (state.floor index).
    enum class ThingKind { Guest, Monster, Item };
    struct Thing
    {
        ThingKind kind = ThingKind::Guest;
        int id = -1;
    };
    [[nodiscard]] std::optional<Thing> PickThing(glm::vec2 mouse, glm::vec2 pixels) const;
    // Clips from new log entries: attack, hit, power; down and dead lie down until woken.
    void React(const std::vector<umbra::night::LogEntry> &entries);
    void NewNight() { pawns_.clear(); }
    // The painted pictures (Art.md), once; a missing one leaves its box (logged).
    void LoadArt(const umbra::night::NightDef &def, const umbra::assets::AssetFolders &assets);
    // The player's goal rooms: their frames glow, pulsing (Play::GoalCells).
    void SetGoalCells(std::vector<int> cells) { goalCells_ = std::move(cells); }
    // Tonight's featured cells (FeaturedCells), set when a night starts.
    void SetFeatured(std::vector<bool> featured) { featured_ = std::move(featured); }
    // The figures: a sliding pawn faces its step and walks, the others stand idle (Table.md).
    void Animate(const umbra::night::NightDef &def, const umbra::night::BoardView &view, const Slide &slide, float dt);
    void Zoom(float steps) { view_.Zoom(steps); }
    // The camera's distance (and its zoom range) and tilt in degrees (options.csv).
    void SetCamera(float distance, float minDistance, float maxDistance, float tilt) { view_.Set(distance, minDistance, maxDistance, tilt); }
    // The guest whose turn it is: a gold arrow bobs over them (-1: none).
    void SetActing(int guest) { acting_ = guest; }
    // A turn opening: a gold ring grows under the guest and fades (t 0 to 1); the figure gestures once.
    void SetBanner(int guest, float t);
    void Pan(glm::vec2 pixels) { view_.Pan(pixels); } // a drag: the table moves with the mouse (Table.md)
    [[nodiscard]] umbra::core::Camera Camera() const { return view_.Camera(); }
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
        umbra::night::SpriteState sprite; // a guest's picture and its motion (Motions.md)
        bool taken = false;
        float yaw = 3.14159265F; // facing the camera (+Z)
    };
    Figure Load(const std::filesystem::path &model, const std::filesystem::path &graph, float scale);
    static umbra::core::Animator MakeAnimator(const Figure &figure);
    static int Crowd(const umbra::night::BoardView &view, int cell); // pawns standing in the cell
    static glm::vec3 Place(const umbra::night::NightDef &def, const umbra::night::BoardView &view, int guest, const Slide &slide);
    umbra::render::Renderer &renderer_;
    umbra::core::MeshHandle box_;  // a 1 m cube centred on the origin
    umbra::core::MeshHandle quad_; // a 1 m square facing +Z, the picture upright
    umbra::core::MeshHandle mirrorQuad_; // the same with u = 1 - u: a sprite facing left
    umbra::night::Motions motions_;      // assets/motions.json
    std::map<std::string, umbra::core::TextureHandle> spriteArt_; // by face: the template guest (art/sprites/huy)
    float spriteAspect_ = 0.35F;        // the sprite picture's width per height
    // A guest's sprite with its motion, feet at `feet`, `height` m tall; `toward`: the way it faces in
    // the world (zero: as it last faced). Table.md "Sprites".
    void Sprite(const umbra::night::NightDef &def, int guest, glm::vec3 feet, float height, glm::vec3 toward, umbra::core::DrawList &out) const;
    [[nodiscard]] glm::vec3 CameraRight() const; // the camera's right, level with the table
    Figure guest_, taken_;        // one guest model for all seven, one Taken; tinted by guests.csv's color
    std::vector<Pawn> pawns_;     // by guest
    Pawn killer_;                 // the wandering killer, in the Taken's figure (Night.md "The killer")
    std::vector<umbra::core::TextureHandle> roomArt_; // by rooms.csv row; empty handle = a box
    std::vector<umbra::core::TextureHandle> cardArt_; // by monsters.csv row (minions too)
    std::vector<umbra::core::TextureHandle> itemArt_; // by items.csv row
    std::vector<umbra::core::TextureHandle> plaqueArt_; // by rooms.csv row: the name plaque on its floor
    std::array<umbra::core::TextureHandle, 2> skyArt_{}; // the sky before and after midnight
    std::vector<bool> featured_;                      // by cell: a gold frame
    std::vector<int> goalCells_;                      // the player's goal rooms: a pulsing glow
    TableCamera view_; // the camera, the table or the room view (TableCamera.hpp)
    int battleCell_ = -1;
    std::vector<StageActor> battleActors_;
    struct HitBox
    {
        glm::vec3 at{0.0F};
        float radius = 0.0F;
        Thing thing;
    };
    mutable std::vector<HitBox> hits_; // the things drawn last frame, for PickThing
    void DrawRooms(const umbra::night::NightDef &def, const umbra::night::BoardView &view, const std::vector<int> &path, int affordable,
                   const Slide &slide, const std::vector<std::pair<int, int>> &rings, umbra::core::DrawList &out) const;
    [[nodiscard]] glm::vec3 Ray(glm::vec2 mouse, glm::vec2 pixels, glm::vec3 &origin) const; // the mouse's ray (direction)
    // The stage's parts (Table.md "The stage"): a room's floor and walls at `offset`, a monster card, a figure.
    void StageRoom(const umbra::night::CellView &room, glm::vec3 offset, float shade, bool featured, umbra::core::DrawList &out) const;
    void StageCard(int kind, glm::vec3 at, umbra::core::DrawList &out) const;
    void StageFigure(const umbra::night::NightDef &def, int guest, bool taken, glm::vec3 at, float yaw, umbra::core::DrawList &out) const;
    int bannerGuest_ = -1;
    float bannerT_ = 0.0F;
    int acting_ = -1;
    bool hunt_ = false;
    float blood_ = 0.0F, flicker_ = 0.0F, clock_ = 0.0F, readable_ = 0.2F; // clock_: seconds, for the flicker and pulse
    [[nodiscard]] float Lamp() const;                    // the lamp's strength now (flicker)
    [[nodiscard]] glm::vec3 HuntTint() const;            // the ambient's tint now (the hunt pulses red)
    void Floor(umbra::core::DrawList &out, umbra::core::TextureHandle art, const glm::mat4 &model, glm::vec4 color) const;
    void Sky(umbra::core::DrawList &out) const; // the painted sky behind the board (Table.md "Sky")
    // A room's brass plaque, flat on the floor at `at`, `width` metres across (Table.md "Plaques").
    void Plaque(umbra::core::DrawList &out, int room, glm::vec3 at, float width, float shade) const;
};
