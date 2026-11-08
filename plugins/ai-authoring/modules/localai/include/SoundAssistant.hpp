#pragma once

#include <ComfyUi.hpp>

#include <AssetFolders.hpp>
#include <Wav.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace umbra::localai
{
    // Glue between ComfyUI's audio models and game sounds, shared by the editor and UmbraTool
    // (roadmap S4, SoundGen.md). Music becomes a looping .ogg, an effect a short mono .wav.
    enum class SoundKind
    {
        Music,
        Effect
    };

    // The project's sound style, added to every prompt: assets/audio/style.json {"music", "effect"}.
    struct SoundStyle final
    {
        std::string music = "instrumental video game soundtrack, fantasy RPG, seamless loop, no vocals";
        std::string effect = "single isolated game sound effect, clean, no music, no background noise";
    };
    [[nodiscard]] SoundStyle LoadSoundStyle(const assets::AssetFolders &assetRoot);

    // The installed model to use: ACE-Step for music, Stable Audio 3 for effects (user decision
    // 2026-11-08); `wanted` if it is installed; "" if ComfyUI has neither.
    [[nodiscard]] std::string PreferredSoundModel(const std::vector<std::string> &installed, SoundKind kind, const std::string &wanted = "");

    // One request per seed.
    [[nodiscard]] std::vector<AudioRequest> MakeSoundRequests(const std::string &idea, SoundKind kind, const SoundStyle &style, const std::string &model,
                                                              const std::string &host, float seconds, const std::vector<long long> &seeds);

    // Decoded PCM at any rate (what ComfyUI returns), before it is made into a game sound.
    struct Pcm final
    {
        int rate = 44100;
        int channels = 2;
        std::vector<float> samples; // interleaved, -1..1
        [[nodiscard]] std::size_t Frames() const noexcept { return channels > 0 ? samples.size() / static_cast<std::size_t>(channels) : 0; }
    };
    bool DecodeFlac(const std::vector<std::uint8_t> &bytes, Pcm &pcm, std::string &error);
    // Cubic interpolation to `rate` (44100 -> 48000 for the mixer).
    [[nodiscard]] Pcm Resample(const Pcm &pcm, int rate);

    // A generated sound made ready for the game, with what was done to it.
    struct SoundCandidate final
    {
        audio::AudioClip clip; // 48000 Hz; music stereo, effects mono
        float peak = 0.0F;     // before normalizing
        float seconds = 0.0F;
        bool silent = false;   // almost nothing in it: warn
        AudioRequest request;
    };
    // Music: normalized, and its last 2 seconds crossfaded into its start so it loops without a
    // click. Effect: silence trimmed, short fades, mono, normalized.
    [[nodiscard]] SoundCandidate FinishSound(const Pcm &pcm, SoundKind kind, const AudioRequest &request);

    // File bytes: 16-bit PCM WAV, and Ogg Vorbis (quality 0..1, ~0.4 = 128 kbit/s).
    [[nodiscard]] std::string EncodeWav(const audio::AudioClip &clip);
    bool EncodeOgg(const audio::AudioClip &clip, float quality, std::string &out, std::string &error);

    // Writes audio/music/<name>.ogg or audio/sfx/<name>.wav plus <name>.source.json (how it was made,
    // the model license); new files ("_2", "_3"... if taken). Returns the sound's asset path or "".
    [[nodiscard]] std::string SaveSound(const std::filesystem::path &assetRoot, const std::string &name, SoundKind kind, const SoundCandidate &candidate,
                                        std::vector<core::Diagnostic> &diagnostics);
}
