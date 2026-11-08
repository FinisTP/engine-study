#include <SoundAssistant.hpp>

#include <Ogg.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include <cmath>
#include <fstream>
#include <thread>

using namespace umbra;
using namespace umbra::localai;
using Catch::Approx;
using nlohmann::json;

namespace
{
    std::vector<std::uint8_t> ReadFile(const std::filesystem::path &file)
    {
        std::ifstream in(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), {}};
    }

    // A stand-in ComfyUI that answers every audio run with the same FLAC file.
    class FakeAudioComfy final
    {
    public:
        FakeAudioComfy()
        {
            flac_ = ReadFile(std::filesystem::path(UMBRA_TEST_LOCALAI_DATA) / "beep_44k_stereo.flac");
            server_.Post("/prompt", [this](const httplib::Request &request, httplib::Response &response) {
                std::lock_guard lock(mutex_);
                graph = json::parse(request.body)["prompt"];
                response.set_content(R"({"prompt_id":"a-1","number":1,"node_errors":{}})", "application/json");
            });
            server_.Get("/history/a-1", [](const httplib::Request &, httplib::Response &response) {
                response.set_content(R"({"a-1":{"status":{"status_str":"success"},"outputs":{"8":{"audio":[{"filename":"umbra_00001.flac","subfolder":"","type":"temp"}]}}}})",
                                     "application/json");
            });
            server_.Get("/view", [this](const httplib::Request &request, httplib::Response &response) {
                if (request.get_param_value("filename") != "umbra_00001.flac" || request.get_param_value("type") != "temp")
                {
                    response.status = 404;
                    return;
                }
                response.set_content(std::string(flac_.begin(), flac_.end()), "audio/flac");
            });
            port_ = server_.bind_to_any_port("127.0.0.1");
            thread_ = std::thread([this] { server_.listen_after_bind(); });
            server_.wait_until_ready();
        }
        ~FakeAudioComfy()
        {
            server_.stop();
            thread_.join();
        }
        [[nodiscard]] std::string Url() const { return "http://127.0.0.1:" + std::to_string(port_); }
        json graph;
        std::mutex mutex_;

    private:
        httplib::Server server_;
        std::thread thread_;
        int port_ = 0;
        std::vector<std::uint8_t> flac_;
    };
}

TEST_CASE("Music runs ACE-Step, effects Stable Audio 3, with the right sampler settings (S4)", "[sound]")
{
    AudioRequest music;
    music.prompt = "calm village theme";
    music.seconds = 45.0F;
    music.seed = 7;
    const json ace = AudioWorkflow(music);
    CHECK(ace["3"]["class_type"] == "TextEncodeAceStepAudio1.5");
    CHECK(ace["3"]["inputs"]["tags"] == "calm village theme");
    CHECK(ace["3"]["inputs"]["lyrics"] == "[Instrumental]");
    CHECK(ace["5"]["inputs"]["seconds"] == 45.0);
    CHECK(ace["6"]["inputs"]["seed"] == 7);
    CHECK(ace["8"]["class_type"] == "PreviewAudio"); // nothing piles up in ComfyUI's output folder

    AudioRequest effect = music;
    effect.model = "stable_audio_3_medium_base.safetensors";
    const json sa3 = AudioWorkflow(effect);
    CHECK(sa3["2"]["inputs"]["type"] == "stable_audio");
    CHECK(sa3["6"]["inputs"]["steps"] == 50);
    effect.model = "stable_audio_3_medium.safetensors"; // distilled
    CHECK(AudioWorkflow(effect)["6"]["inputs"]["steps"] == 8);

    const std::vector<std::string> installed{"sd_xl_base_1.0.safetensors", "stable_audio_3_medium_base.safetensors", "ace_step_1.5_turbo_aio.safetensors",
                                             "acestep_v1.5_turbo.safetensors"};
    CHECK(PreferredSoundModel(installed, SoundKind::Music) == "ace_step_1.5_turbo_aio.safetensors");
    CHECK(PreferredSoundModel(installed, SoundKind::Effect) == "stable_audio_3_medium_base.safetensors");
    CHECK(PreferredSoundModel({"stable_audio_3_medium.safetensors"}, SoundKind::Music) == "stable_audio_3_medium.safetensors");
    CHECK(PreferredSoundModel({"sd_xl_base_1.0.safetensors", "acestep_v1.5_turbo.safetensors"}, SoundKind::Music).empty()); // split files only
}

TEST_CASE("A generated sound is downloaded, trimmed, made mono and loud, and saved with its source", "[sound]")
{
    FakeAudioComfy comfy;
    const SoundStyle style;
    const auto requests = MakeSoundRequests("sword swing", SoundKind::Effect, style, "stable_audio_3_medium_base.safetensors", comfy.Url(), 2.0F, {11});
    CHECK(requests[0].prompt == "sword swing, " + style.effect);
    CancelToken cancel;
    const AudioResult result = GenerateAudio(requests[0], cancel);
    REQUIRE(result.ok);
    Pcm pcm;
    std::string error;
    REQUIRE(DecodeFlac(result.file, pcm, error));
    CHECK(pcm.rate == 44100);
    CHECK(pcm.channels == 2);
    CHECK(pcm.Frames() == Approx(0.8 * 44100).margin(2000)); // 0.2 s silence, 0.3 s tone, 0.3 s silence

    const SoundCandidate sound = FinishSound(pcm, SoundKind::Effect, result.request);
    CHECK(sound.clip.channels == 1);
    CHECK(sound.seconds == Approx(0.365F).margin(0.03F)); // the tone plus 5 ms before and 60 ms after
    float peak = 0.0F;
    for (float s : sound.clip.samples)
        peak = std::max(peak, std::abs(s));
    CHECK(peak == Approx(0.89F).margin(0.01F));
    CHECK_FALSE(sound.silent);

    // Saved as a 48 kHz WAV the game reads, next to its source; the second save gets a new name.
    const auto root = std::filesystem::temp_directory_path() / "umbra-sound-save";
    std::filesystem::remove_all(root);
    std::vector<core::Diagnostic> diagnostics;
    CHECK(SaveSound(root, "sword_swing", SoundKind::Effect, sound, diagnostics) == "audio/sfx/sword_swing.wav");
    CHECK(SaveSound(root, "sword_swing", SoundKind::Effect, sound, diagnostics) == "audio/sfx/sword_swing_2.wav");
    std::string wavError;
    const auto clip = audio::LoadWav(root / "audio/sfx/sword_swing.wav", wavError);
    REQUIRE(clip);
    CHECK(clip->Frames() == sound.clip.Frames());
    std::ifstream sourceFile(root / "audio/sfx/sword_swing.source.json");
    const json source = json::parse(sourceFile);
    CHECK(source["seed"] == 11);
    CHECK(source["models"][0]["license"].get<std::string>().find("Community") != std::string::npos);
    CHECK(source["models"][0]["commercial"] == true);
}

TEST_CASE("Music is resampled to 48 kHz and loops without a jump; the .ogg plays in the game", "[sound]")
{
    // 3 s of a 220 Hz tone at 44.1 kHz, getting louder: the end differs from the start.
    Pcm pcm;
    pcm.rate = 44100;
    pcm.channels = 2;
    for (int i = 0; i < 3 * 44100; ++i)
    {
        const float v = 0.2F * (1.0F + static_cast<float>(i) / 44100.0F) * std::sin(2.0F * 3.14159265F * 220.0F * static_cast<float>(i) / 44100.0F);
        pcm.samples.insert(pcm.samples.end(), {v, v});
    }
    const SoundCandidate music = FinishSound(pcm, SoundKind::Music, {});
    CHECK(music.clip.channels == 2);
    const std::size_t frames = music.clip.Frames();
    CHECK(frames == Approx(3 * 48000 - 3 * 48000 / 4).margin(4)); // a quarter (at most 2 s) became the crossfade

    // The loop point: the jump from the last frame back to the first is no bigger than a normal step.
    float biggestStep = 0.0F;
    for (std::size_t i = 1; i < frames; ++i)
        biggestStep = std::max(biggestStep, std::abs(music.clip.samples[i * 2] - music.clip.samples[(i - 1) * 2]));
    CHECK(std::abs(music.clip.samples[0] - music.clip.samples[(frames - 1) * 2]) <= biggestStep * 1.5F);

    std::string ogg, error;
    REQUIRE(EncodeOgg(music.clip, 0.4F, ogg, error));
    const auto track = audio::ParseMusic(std::vector<unsigned char>(ogg.begin(), ogg.end()), error);
    REQUIRE(track);
    CHECK(track->channels == 2);
    CHECK(track->seconds == Approx(static_cast<float>(frames) / 48000.0F).margin(0.05F));
    CHECK(ogg.size() < music.clip.samples.size()); // much smaller than 16-bit samples would be
}
