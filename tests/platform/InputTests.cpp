#include <catch2/catch_test_macros.hpp>

#include <umbra/platform/Input.hpp>

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
