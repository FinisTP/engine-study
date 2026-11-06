#include <catch2/catch_test_macros.hpp>

#include <AudioDevice.hpp>
#include <Mixer.hpp>

#include <chrono>
#include <thread>

// CTest runs this with SDL_AUDIO_DRIVER=dummy, so no sound card is needed.
TEST_CASE("The audio device pulls a clip through the mixer and closes cleanly")
{
    umbra::audio::Mixer mixer;
    umbra::platform::AudioDevice device;
    REQUIRE(device.Open(mixer));
    REQUIRE(device.IsOpen());

    // 0.05 s of silence; the device thread should finish it well within 2 s.
    auto clip = std::make_shared<const umbra::audio::AudioClip>(umbra::audio::AudioClip{1, std::vector<float>(2400, 0.0F)});
    REQUIRE(mixer.Play(clip));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (mixer.VoiceCount() > 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    REQUIRE(mixer.VoiceCount() == 0);

    device.Close();
    REQUIRE_FALSE(device.IsOpen());
    device.Close(); // repeated close is safe
}
