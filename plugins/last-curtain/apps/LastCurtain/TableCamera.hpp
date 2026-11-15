#pragma once

#include <BoardView.hpp>
#include <DrawList.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The Last Curtain's camera (Table.md "Room view"): a target on the table, eased; a seat above the
// table at `tilt`; and the room view, the same place roomScale times larger seen from the front
// corner, with a zoom between the two. The target always stays in table space.
inline constexpr float roomScale = 8.0F / umbra::night::cellSize; // an 8 m room where a cell's tile lies
inline constexpr float viewSeconds = 0.6F;                         // the zoom between the views

class TableCamera
{
public:
    void Aim(glm::vec3 target, float seconds); // ease to a table point
    void Pan(glm::vec2 pixels);                // a drag: the table moves with the mouse
    void Zoom(float steps);                    // the wheel: the seat's distance, or the room view's
    void Update(float dt);
    void SetRoomView(bool on) { wantRoom_ = on; }
    void SetShot(bool on, float distance, float yaw, float tilt) { shot_ = on, shotDistance_ = distance, shotYaw_ = yaw, shotTilt_ = tilt; }
    void Set(float distance, float minDistance, float maxDistance, float tilt)
    {
        distance_ = distance, minDistance_ = minDistance, maxDistance_ = maxDistance, tilt_ = tilt;
    }
    [[nodiscard]] float RoomT() const { return roomT_; } // 0 the table, 1 the room view
    [[nodiscard]] bool WantsRoom() const { return wantRoom_; }
    [[nodiscard]] glm::vec3 Target() const;              // the eased target, on the table
    [[nodiscard]] umbra::core::Camera Camera() const;    // in table space, or room space while RoomT() > 0

private:
    glm::vec3 from_{0.0F}, to_{0.0F};
    float seconds_ = 0.0F, elapsed_ = 0.0F;
    float distance_ = 11.0F, minDistance_ = 5.0F, maxDistance_ = 22.0F, tilt_ = 50.0F;
    float roomDistance_ = 11.0F; // metres, in room space
    float roomT_ = 0.0F;
    bool wantRoom_ = false;
    bool shot_ = false; // a scene's shot (Play.md "Cutscenes")
    float shotDistance_ = 0.0F, shotYaw_ = 0.0F, shotTilt_ = 50.0F;
};
