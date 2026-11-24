// The table's camera between the table and the room view (Table.md "Room view").
#include <TableCamera.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

TEST_CASE("Aiming in the room view keeps the target on the table, so the camera never drifts (T163)", "[night][camera]")
{
    TableCamera cam;
    cam.SetRoomView(true);
    cam.Update(1.0F); // fully in the room view
    for (int i = 0; i < 5; ++i)
    {
        cam.Aim({2.4F, 0.0F, -2.4F}, 0.0F);
        cam.Update(0.1F);
    }
    CHECK(glm::length(cam.Target() - glm::vec3{2.4F, 0.0F, -2.4F}) < 1e-4F);
    // The camera looks at the same place, roomScale times larger.
    const umbra::core::Camera c = cam.Camera();
    CHECK(c.target.x == Catch::Approx(2.4F * roomScale));
    CHECK(c.target.z == Catch::Approx(-2.4F * roomScale));
}

TEST_CASE("A drag in the room view moves the table target a little and stays finite (T163)", "[night][camera]")
{
    TableCamera cam;
    cam.SetRoomView(true);
    cam.Update(1.0F);
    cam.Aim({0.0F, 0.0F, 0.0F}, 0.0F);
    for (int i = 0; i < 100; ++i)
        cam.Pan({10.0F, 0.0F});
    const glm::vec3 t = cam.Target();
    CHECK(std::isfinite(t.x));
    CHECK(t.x < 0.0F);
    CHECK(t.x > -10.0F); // 1000 pixels at the room distance: a few metres of table
    cam.Aim({0.0F, 0.0F, 0.0F}, 0.0F); // Focus brings it back
    CHECK(glm::length(cam.Target()) < 1e-4F);
}

TEST_CASE("In the room view a sideways drag moves along the screen, slanted like the view (T169)", "[night][camera]")
{
    TableCamera cam;
    cam.SetRoomView(true);
    cam.Update(1.0F);
    cam.Aim({0.0F, 0.0F, 0.0F}, 0.0F);
    cam.Pan({50.0F, 0.0F});
    const glm::vec3 moved = cam.Target();
    const umbra::core::Camera c = cam.Camera();
    const glm::vec3 forward = glm::normalize(glm::vec3{c.target.x - c.position.x, 0.0F, c.target.z - c.position.z});
    CHECK(std::abs(glm::dot(glm::normalize(moved), forward)) < 1e-3F); // across the view, not into it
    CHECK(std::abs(moved.z) > 0.01F);                                   // slanted: not only along x
}

TEST_CASE("A drag stops at the board's edge (SetBounds); without bounds it is free", "[table][camera]")
{
    TableCamera cam;
    cam.SetBounds({-2.0F, -3.0F}, {2.0F, 3.0F}); // x and z on the table
    for (int i = 0; i < 100; ++i)
        cam.Pan({200.0F, 200.0F});
    const glm::vec3 t = cam.Target();
    CHECK(t.x >= -2.0F);
    CHECK(t.x <= 2.0F);
    CHECK(t.z >= -3.0F);
    CHECK(t.z <= 3.0F);
    TableCamera free;
    for (int i = 0; i < 100; ++i)
        free.Pan({200.0F, 0.0F});
    CHECK(std::abs(free.Target().x) > 10.0F);
}

TEST_CASE("The room view's seat can come nearer (a scene's close-up) and look a little higher (feedback 2026-11-24 15:10, notes 1 and 8)", "[table][camera]")
{
    TableCamera cam;
    cam.SetRoomView(true);
    cam.Update(1.0F);
    const umbra::core::Camera far = cam.Camera();
    // 1. Nearer: the seat moves toward the target.
    const float before = cam.RoomDistance();
    cam.SetRoomDistance(before * 0.5F);
    const umbra::core::Camera near = cam.Camera();
    CHECK(glm::length(near.position - near.target) < glm::length(far.position - far.target) * 0.6F);
    // 2. Higher: the look point rises by the lift, so figures sit lower and the top of the screen stays clear.
    cam.SetRoomLift(2.0F);
    CHECK(cam.Camera().target.y == Catch::Approx(near.target.y - 0.7F + 2.0F));
}

TEST_CASE("A shot can look higher than the floor (the battle view, note 8)", "[table][camera]")
{
    TableCamera cam;
    cam.SetShot(true, 7.0F, 0.0F, 14.0F);
    const umbra::core::Camera low = cam.Camera();
    cam.SetShotLift(1.5F);
    const umbra::core::Camera high = cam.Camera();
    CHECK(high.target.y == Catch::Approx(low.target.y + 1.5F));
    CHECK(high.position.y == Catch::Approx(low.position.y + 1.5F)); // the seat rises with it: the angle stays
}
