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
