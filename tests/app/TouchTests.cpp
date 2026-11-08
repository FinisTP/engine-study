#include <TouchControls.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using Catch::Approx;
using umbra::platform::InputKey;

namespace
{
    // One frame: new frame, the finger events, then the controls (1280x720 screen).
    struct Phone final
    {
        umbra::platform::Input input;
        TouchControls touch;
        umbra::ui::FontAtlas fonts; // no fonts: buttons draw without labels
        umbra::core::UiDrawList drawn;
        TouchControls::State state{true, false, false};

        TouchControls::Output Frame(const std::function<void(umbra::platform::Input &)> &events = {})
        {
            input.BeginFrame();
            if (events)
                events(input);
            drawn.Clear();
            return touch.Update(input, {1280, 720}, state, fonts, {}, [](const std::string &) { return umbra::ui::ImageRef{}; }, drawn);
        }
    };
}

TEST_CASE("Touch: hidden until a finger touches; a stick on the left, camera drag on the right (P11)")
{
    Phone phone;
    CHECK(phone.Frame().move == glm::vec2{0, 0});
    CHECK_FALSE(phone.touch.Visible()); // a PC never shows them
    CHECK(phone.drawn.vertices.empty());

    // Left thumb down at (200, 500), slides 60 px right and 30 px up (the stick's radius is 90 px).
    phone.Frame([](auto &in) { in.HandleFinger(7, 200 / 1280.0F, 500 / 720.0F, 0, 0, 0); });
    CHECK(phone.touch.Visible());
    CHECK_FALSE(phone.drawn.vertices.empty());
    const auto out = phone.Frame([](auto &in) { in.HandleFinger(7, 260 / 1280.0F, 470 / 720.0F, 60 / 1280.0F, -30 / 720.0F, 1); });
    CHECK(out.move.x == Approx(2.0F / 3.0F).margin(0.01F));
    CHECK(out.move.y == Approx(1.0F / 3.0F).margin(0.01F));
    // Past the edge it stays on the circle: a long slide right is full right.
    CHECK(phone.Frame([](auto &in) { in.HandleFinger(7, 500 / 1280.0F, 500 / 720.0F, 240 / 1280.0F, 30 / 720.0F, 1); }).move.x == Approx(1.0F).margin(0.01F));
    CHECK(phone.Frame([](auto &in) { in.HandleFinger(7, 500 / 1280.0F, 500 / 720.0F, 0, 0, 2); }).move == glm::vec2{0, 0});

    // A finger on the right (not on a button) turns the camera by how far it moved.
    phone.Frame([](auto &in) { in.HandleFinger(9, 900 / 1280.0F, 300 / 720.0F, 0, 0, 0); });
    const auto look = phone.Frame([](auto &in) { in.HandleFinger(9, 930 / 1280.0F, 290 / 720.0F, 30 / 1280.0F, -10 / 720.0F, 1); });
    CHECK(look.look.x == Approx(30.0F).margin(0.5F));
    CHECK(look.look.y == Approx(-10.0F).margin(0.5F));
}

TEST_CASE("Touch buttons press the same keys as a keyboard and let go with the finger (P11)")
{
    Phone phone;
    const auto layout = TouchControls::Layout({1280, 720}, phone.state);
    const auto jump = *std::find_if(layout.begin(), layout.end(), [](const auto &b) { return b.key == InputKey::Space; });
    phone.Frame([&](auto &in) { in.HandleFinger(3, jump.center.x / 1280.0F, jump.center.y / 720.0F, 0, 0, 0); });
    CHECK(phone.input.IsKeyPressed(InputKey::Space));
    phone.Frame();
    CHECK(phone.input.IsKeyHeld(InputKey::Space)); // held while the finger stays
    phone.Frame([&](auto &in) { in.HandleFinger(3, jump.center.x / 1280.0F, jump.center.y / 720.0F, 0, 0, 2); });
    CHECK(phone.input.IsKeyReleased(InputKey::Space));

    // A menu takes the screen: controls hide and nothing stays pressed.
    phone.Frame([&](auto &in) { in.HandleFinger(4, jump.center.x / 1280.0F, jump.center.y / 720.0F, 0, 0, 0); });
    phone.state.menuOpen = true;
    phone.Frame();
    CHECK_FALSE(phone.touch.Visible());
    CHECK_FALSE(phone.input.IsKeyHeld(InputKey::Space));

    // Real-time fights show hotkeys 1-4 and Flee; exploring shows Jump and Use.
    const auto fight = TouchControls::Layout({1280, 720}, {false, true, false});
    for (InputKey key : {InputKey::Num1, InputKey::Num2, InputKey::Num3, InputKey::Num4, InputKey::F})
        CHECK(std::any_of(fight.begin(), fight.end(), [&](const auto &b) { return b.key == key; }));
    CHECK(std::none_of(fight.begin(), fight.end(), [](const auto &b) { return b.key == InputKey::E; }));
    // Every button stays on the screen.
    for (const auto &b : fight)
        CHECK((b.center.x - b.radius >= 0 && b.center.y - b.radius >= 0 && b.center.x + b.radius <= 1280 && b.center.y + b.radius <= 720));
}

TEST_CASE("Gamepad sticks have a dead zone and reach 1 at full tilt (P11)")
{
    umbra::platform::Input input;
    input.HandleAxis(0, 0.1F);
    CHECK(input.Axis(0) == 0.0F);
    input.HandleAxis(0, 1.0F);
    CHECK(input.Axis(0) == Approx(1.0F));
    input.HandleAxis(1, -0.6F);
    CHECK(input.Axis(1) == Approx(-0.5F));
}
