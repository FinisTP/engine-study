#pragma once

#include <BoardView.hpp>

#include <DrawList.hpp>
#include <Renderer.hpp>

#include <glm/vec2.hpp>

// The board drawn as a table top (Table.md): placeholder boxes for floors, walls, pawns and
// monsters, one warm light, and a tilted camera that eases to where the action is.
class Table final
{
public:
    explicit Table(umbra::render::Renderer &renderer);
    ~Table();
    Table(const Table &) = delete;
    Table &operator=(const Table &) = delete;

    void Draw(const umbra::night::NightDef &def, const umbra::night::BoardView &view, umbra::core::DrawList &out) const;
    void SetHunt(bool hunt) { hunt_ = hunt; } // after the Turn the ambient turns red
    void Aim(glm::vec3 target, float seconds);
    void Update(float dt);
    void Zoom(float steps);
    [[nodiscard]] umbra::core::Camera Camera() const;
    // The cell under the mouse (render pixels), -1 if none.
    [[nodiscard]] int Pick(const umbra::night::NightDef &def, glm::vec2 mouse, glm::vec2 pixels) const;

private:
    umbra::render::Renderer &renderer_;
    umbra::core::MeshHandle box_; // a 1 m cube centred on the origin
    glm::vec3 from_{0.0F}, to_{0.0F};
    float seconds_ = 0.0F, elapsed_ = 0.0F;
    float distance_ = 11.0F;
    bool hunt_ = false;
};
