#include "TableCamera.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

void TableCamera::Aim(glm::vec3 target, float seconds)
{
    // From where the camera looks now (on the table, whatever the view) to the new point.
    from_ = Target(), to_ = target;
    seconds_ = seconds, elapsed_ = 0.0F;
}

void TableCamera::Pan(glm::vec2 pixels)
{
    // A pixel covers about distance / 700 metres; in the room view the rooms are roomScale times
    // larger, so the same drag moves the table that much less.
    // Screen right and down follow the view: straight on the table; slanted, as the corner sees it, in
    // the room view (right = forward x up, both flat on the floor).
    const float metres = roomT_ > 0.0F ? roomDistance_ / roomScale : distance_;
    const glm::vec3 corner = glm::normalize(glm::vec3{-5.2F, 0.0F, -7.5F});
    const float e = roomT_ * roomT_ * (3.0F - 2.0F * roomT_);
    const glm::vec3 forward = glm::normalize(glm::mix(glm::vec3{0.0F, 0.0F, -1.0F}, corner, e));
    const glm::vec3 right{-forward.z, 0.0F, forward.x};
    from_ = to_ = Target() - (right * pixels.x - forward * pixels.y) * (metres / 700.0F);
    elapsed_ = seconds_;
}

void TableCamera::Zoom(float steps)
{
    if (wantRoom_)
        roomDistance_ = std::clamp(roomDistance_ - steps * 2.0F, 6.0F, 45.0F);
    else
        distance_ = std::clamp(distance_ - steps, minDistance_, maxDistance_);
}

void TableCamera::Update(float dt)
{
    elapsed_ = std::min(elapsed_ + dt, seconds_);
    roomT_ = std::clamp(roomT_ + (wantRoom_ ? dt : -dt) / viewSeconds, 0.0F, 1.0F);
}

glm::vec3 TableCamera::Target() const
{
    // Ease in and out (smoothstep) from the old target to the new.
    const float t = seconds_ > 0.0F ? elapsed_ / seconds_ : 1.0F;
    return from_ + (to_ - from_) * (t * t * (3.0F - 2.0F * t));
}

umbra::core::Camera TableCamera::Camera() const
{
    // 1. The table: up and back at `tilt`.
    umbra::core::Camera camera;
    camera.target = Target();
    if (shot_ && roomT_ <= 0.0F)
    {
        // A scene's shot: the seat at its tilt, turned yaw degrees around the target.
        const float t = glm::radians(shotTilt_), y = glm::radians(shotYaw_);
        camera.position = camera.target + shotDistance_ * glm::vec3{std::cos(t) * std::sin(y), std::sin(t), std::cos(t) * std::cos(y)};
        camera.farPlane = std::max(camera.farPlane, 2.0F * shotDistance_ + 60.0F);
        return camera;
    }
    const glm::vec3 up{0.0F, std::sin(glm::radians(tilt_)), std::cos(glm::radians(tilt_))};
    camera.position = camera.target + distance_ * up;
    if (roomT_ <= 0.0F)
        return camera;
    // 2. The room view: the same place roomScale times larger, from the front corner; on the way the
    //    seat glides from the table's (scaled, so the rooms first appear where the tiles were).
    const float e = roomT_ * roomT_ * (3.0F - 2.0F * roomT_);
    const glm::vec3 target = camera.target * roomScale;
    camera.target = target + glm::vec3{0.0F, 0.7F * e, 0.0F};
    camera.position = glm::mix(target + up * (distance_ * roomScale), camera.target + glm::normalize(glm::vec3{5.2F, 5.1F, 7.5F}) * roomDistance_, e);
    camera.farPlane = std::max(camera.farPlane, 400.0F);
    return camera;
}
