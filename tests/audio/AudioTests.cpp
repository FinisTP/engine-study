#include "assets/TestFiles.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Mixer.hpp>
#include <SoundBank.hpp>
#include <Wav.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

using Catch::Approx;
using namespace umbra;

namespace
{
    const std::filesystem::path assetDir{UMBRA_TEST_ASSET_DIRECTORY};

    void Put16(std::vector<std::byte> &out, std::uint16_t v)
    {
        out.push_back(std::byte(v & 0xFF));
        out.push_back(std::byte(v >> 8));
    }
    void Put32(std::vector<std::byte> &out, std::uint32_t v)
    {
        Put16(out, static_cast<std::uint16_t>(v & 0xFFFF));
        Put16(out, static_cast<std::uint16_t>(v >> 16));
    }
    void PutTag(std::vector<std::byte> &out, const char *tag)
    {
        for (int i = 0; i < 4; ++i)
            out.push_back(std::byte(tag[i]));
    }

    // A WAV file in memory, with an extra "LIST" chunk the parser must skip.
    std::vector<std::byte> MakeWav(const std::vector<std::int16_t> &samples, std::uint16_t channels = 1,
                                   std::uint32_t rate = 48000, std::uint16_t bits = 16)
    {
        std::vector<std::byte> out;
        PutTag(out, "RIFF");
        Put32(out, 0); // size is not checked
        PutTag(out, "WAVE");
        PutTag(out, "LIST");
        Put32(out, 3);
        out.insert(out.end(), 4, std::byte{0}); // 3 bytes + 1 pad byte
        PutTag(out, "fmt ");
        Put32(out, 16);
        Put16(out, 1);
        Put16(out, channels);
        Put32(out, rate);
        Put32(out, rate * channels * 2);
        Put16(out, static_cast<std::uint16_t>(channels * 2));
        Put16(out, bits);
        PutTag(out, "data");
        Put32(out, static_cast<std::uint32_t>(samples.size() * 2));
        for (const std::int16_t s : samples)
            Put16(out, static_cast<std::uint16_t>(s));
        return out;
    }

    std::shared_ptr<const audio::AudioClip> Clip(std::vector<float> samples, int channels = 1)
    {
        return std::make_shared<const audio::AudioClip>(audio::AudioClip{channels, std::move(samples)});
    }
}

TEST_CASE("16-bit PCM WAV decodes to floats, skipping unknown chunks")
{
    std::string error;
    const auto clip = audio::ParseWav(MakeWav({0, 16384, -32768, 32767}), error);
    REQUIRE(clip);
    REQUIRE(clip->channels == 1);
    REQUIRE(clip->Frames() == 4);
    REQUIRE(clip->samples[1] == Approx(0.5F));
    REQUIRE(clip->samples[2] == Approx(-1.0F));

    const auto stereo = audio::ParseWav(MakeWav({100, 200, 300, 400}, 2), error);
    REQUIRE(stereo);
    REQUIRE(stereo->Frames() == 2);
}

TEST_CASE("Unsupported or broken WAV files are rejected with a reason")
{
    std::string error;
    REQUIRE_FALSE(audio::ParseWav(MakeWav({1, 2}, 1, 44100), error));
    REQUIRE(error.find("48000") != std::string::npos);
    REQUIRE_FALSE(audio::ParseWav(MakeWav({1, 2}, 1, 48000, 8), error));
    REQUIRE_FALSE(audio::ParseWav(MakeWav({1, 2}, 3), error));
    REQUIRE_FALSE(audio::ParseWav(MakeWav({}), error));

    std::vector<std::byte> truncated = MakeWav({1, 2, 3, 4});
    truncated.resize(truncated.size() - 4); // data chunk now claims more than the file has
    REQUIRE_FALSE(audio::ParseWav(truncated, error));
    REQUIRE(error.find("past the end") != std::string::npos);

    const std::byte junk[5]{};
    REQUIRE_FALSE(audio::ParseWav(junk, error));
}

TEST_CASE("The shipped sounds load")
{
    for (const char *name : {"footstep", "jump", "interact", "ui_click"})
    {
        std::string error;
        INFO(name << ": " << error);
        REQUIRE(audio::LoadWav(test::ReferenceAssets() / ("audio/" + std::string{name} + ".wav"), error));
    }
}

TEST_CASE("The mixer adds voices, clamps, and drops finished voices")
{
    audio::Mixer mixer;
    REQUIRE(mixer.Play(Clip({0.5F, 0.5F, 0.5F}), 1.0F));
    REQUIRE(mixer.Play(Clip({0.75F, -0.25F}, 2), 1.0F));
    std::vector<float> out(2 * 4);
    mixer.Mix(out.data(), 4);
    REQUIRE(out[0] == Approx(1.0F));   // 0.5 + 0.75 clamped
    REQUIRE(out[1] == Approx(0.25F));  // 0.5 - 0.25
    REQUIRE(out[2] == Approx(0.5F));   // stereo clip has ended
    REQUIRE(out[6] == Approx(0.0F));   // both ended
    REQUIRE(mixer.VoiceCount() == 0);

    REQUIRE_FALSE(mixer.Play(nullptr));
    REQUIRE_FALSE(mixer.Play(Clip({})));
}

TEST_CASE("Pausing freezes game voices but not UI voices; voices are capped")
{
    audio::Mixer mixer;
    mixer.Play(Clip({0.1F, 0.2F}), 1.0F, audio::Bus::Game);
    mixer.Play(Clip({0.3F}), 1.0F, audio::Bus::Ui);
    mixer.SetGamePaused(true);
    float out[2];
    mixer.Mix(out, 1);
    REQUIRE(out[0] == Approx(0.3F)); // only the UI click
    REQUIRE(mixer.VoiceCount() == 1); // the game voice waits

    mixer.SetGamePaused(false);
    mixer.Mix(out, 1);
    REQUIRE(out[0] == Approx(0.1F)); // resumes from its first sample

    for (std::size_t i = 0; i < audio::Mixer::maxVoices + 5; ++i)
        mixer.Play(Clip({0.0F}));
    REQUIRE(mixer.VoiceCount() == audio::Mixer::maxVoices);
    mixer.StopAll();
    REQUIRE(mixer.VoiceCount() == 0);
}

TEST_CASE("A playing voice keeps its clip alive after the bank forgets it")
{
    audio::SoundBank bank(test::ReferenceAssets()); // the sounds come from the third-person plugin
    audio::Mixer mixer;
    std::weak_ptr<const audio::AudioClip> watch = bank.Get("audio/ui_click.wav");
    REQUIRE(mixer.Play(watch.lock()));
    bank.Clear();
    REQUIRE_FALSE(watch.expired()); // still playing

    std::vector<float> out(2 * watch.lock()->Frames());
    mixer.Mix(out.data(), out.size() / 2);
    REQUIRE(watch.expired()); // finished -> freed
}

TEST_CASE("Missing sounds are reported once and return nothing")
{
    audio::SoundBank bank(test::ReferenceAssets()); // the sounds come from the third-person plugin
    REQUIRE(bank.Get("audio/missing.wav") == nullptr);
    REQUIRE(bank.Get("audio/missing.wav") == nullptr);
    REQUIRE(bank.Get("../outside.wav") == nullptr);
    const auto problems = bank.TakeDiagnostics();
    REQUIRE(problems.size() == 2);
    REQUIRE(problems[0].code == "sound.load");
    REQUIRE(bank.TakeDiagnostics().empty());
    REQUIRE(bank.Get("audio/jump.wav") == bank.Get("audio/jump.wav")); // cached, shared
}
