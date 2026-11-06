#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace umbra::audio
{
    // Every clip and the output device run at this rate, so the mixer never resamples.
    inline constexpr int mixRate = 48000;

    // A decoded sound: samples in -1..1, interleaved (L R L R ...) when stereo.
    struct AudioClip final
    {
        int channels = 1;
        std::vector<float> samples;

        [[nodiscard]] std::size_t Frames() const noexcept { return samples.size() / static_cast<std::size_t>(channels); }
    };

    // Accepts only RIFF/WAVE with 16-bit PCM, 1 or 2 channels, 48000 Hz (see Audio.md).
    // On failure returns nothing and says why in `error`.
    [[nodiscard]] std::optional<AudioClip> ParseWav(std::span<const std::byte> bytes, std::string &error);
    [[nodiscard]] std::optional<AudioClip> LoadWav(const std::filesystem::path &path, std::string &error);
}
