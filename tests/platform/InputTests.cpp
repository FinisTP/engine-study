#include <catch2/catch_test_macros.hpp>

#include <Input.hpp>

TEST_CASE("Input tracks key transitions across frames")
{
	umbra::platform::Input input;
	constexpr auto key = umbra::platform::InputKey::W;

	REQUIRE_FALSE(input.IsKeyPressed(key));
	REQUIRE_FALSE(input.IsKeyHeld(key));
	REQUIRE_FALSE(input.IsKeyReleased(key));

	input.HandleKeyDown(key, false);
	REQUIRE(input.IsKeyPressed(key));
	REQUIRE(input.IsKeyHeld(key));
	REQUIRE_FALSE(input.IsKeyReleased(key));

	input.BeginFrame();
	REQUIRE_FALSE(input.IsKeyPressed(key));
	REQUIRE(input.IsKeyHeld(key));
	REQUIRE_FALSE(input.IsKeyReleased(key));

	input.HandleKeyDown(key, true);
	REQUIRE_FALSE(input.IsKeyPressed(key));
	REQUIRE(input.IsKeyHeld(key));

	input.HandleKeyUp(key);
	REQUIRE_FALSE(input.IsKeyHeld(key));
	REQUIRE(input.IsKeyReleased(key));

	input.BeginFrame();
	REQUIRE_FALSE(input.IsKeyReleased(key));
}

TEST_CASE("Input preserves same-frame press and release edges")
{
	umbra::platform::Input input;
	constexpr auto key = umbra::platform::InputKey::Space;

	input.HandleKeyDown(key, false);
	input.HandleKeyUp(key);

	REQUIRE(input.IsKeyPressed(key));
	REQUIRE_FALSE(input.IsKeyHeld(key));
	REQUIRE(input.IsKeyReleased(key));
}

TEST_CASE("Input ignores unknown keys and clears all state")
{
	umbra::platform::Input input;

	input.HandleKeyDown(umbra::platform::InputKey::Unknown, false);
	REQUIRE(input.GetKeyState(umbra::platform::InputKey::Unknown) == umbra::platform::InputKeyState::Unknown);

	input.HandleKeyDown(umbra::platform::InputKey::A, false);
	input.ClearKeyStates();

	REQUIRE_FALSE(input.IsKeyPressed(umbra::platform::InputKey::A));
	REQUIRE_FALSE(input.IsKeyHeld(umbra::platform::InputKey::A));
	REQUIRE_FALSE(input.IsKeyReleased(umbra::platform::InputKey::A));
}

TEST_CASE("Mouse motion accumulates within a frame and resets each frame")
{
    umbra::platform::Input input;
    input.HandleMouseMotion(3.0F, -1.0F);
    input.HandleMouseMotion(2.0F, -1.0F);
    REQUIRE(input.GetMouseDeltaX() == 5.0F);
    REQUIRE(input.GetMouseDeltaY() == -2.0F);
    input.BeginFrame();
    REQUIRE(input.GetMouseDeltaX() == 0.0F);
}

TEST_CASE("Mouse buttons report presses once and focus loss clears everything")
{
    umbra::platform::Input input;
    input.HandleMouseButton(0, true);
    REQUIRE(input.IsMouseButtonPressed(0));
    REQUIRE(input.IsMouseButtonHeld(0));
    input.BeginFrame();
    REQUIRE_FALSE(input.IsMouseButtonPressed(0));
    REQUIRE(input.IsMouseButtonHeld(0));

    input.HandleKeyDown(umbra::platform::InputKey::W, false);
    input.HandleMouseMotion(10.0F, 10.0F);
    input.ClearKeyStates(); // what focus loss and suspend do
    REQUIRE_FALSE(input.IsMouseButtonHeld(0));
    REQUIRE_FALSE(input.IsKeyHeld(umbra::platform::InputKey::W));
    REQUIRE(input.GetMouseDeltaX() == 0.0F);

    input.HandleMouseButton(7, true); // out of range: ignored
    REQUIRE_FALSE(input.IsMouseButtonHeld(7));
}

TEST_CASE("Input adds up wheel steps for one frame")
{
    umbra::platform::Input input;
    input.HandleWheel(1.0F);
    input.HandleWheel(0.5F);
    CHECK(input.Wheel() == 1.5F);
    input.BeginFrame();
    CHECK(input.Wheel() == 0.0F);
}
