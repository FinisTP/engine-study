#pragma once

#include <Wav.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct stb_vorbis; // engine/audio/third_party/stb_vorbis.c

namespace umbra::audio
{
    // Ogg Vorbis (stb_vorbis), 48000 Hz like every other sound, 1 or 2 channels (Audio.md).
    // A short sound decodes into an AudioClip at once.
    [[nodiscard]] std::optional<AudioClip> ParseOgg(std::span<const std::byte> bytes, std::string &error);

    // Music stays compressed in memory and is decoded while it plays (Mixer::PlayMusic), so a
    // 3-minute loop costs ~3 MB instead of ~70 MB of samples.
    struct MusicTrack final
    {
        std::vector<unsigned char> ogg; // the whole file
        int channels = 2;
        float seconds = 0.0F;
    };
    [[nodiscard]] std::optional<MusicTrack> LoadMusic(const std::filesystem::path &path, std::string &error);
    [[nodiscard]] std::optional<MusicTrack> ParseMusic(std::vector<unsigned char> ogg, std::string &error);

    // Decodes a track while it plays; loops back to the start at the end.
    class MusicDecoder final
    {
    public:
        explicit MusicDecoder(const MusicTrack &track);
        ~MusicDecoder();
        MusicDecoder(const MusicDecoder &) = delete;
        MusicDecoder &operator=(const MusicDecoder &) = delete;
        [[nodiscard]] bool Ok() const noexcept { return vorbis_ != nullptr; }
        // Adds `frames` stereo frames into `out` (L R L R ...), the gain going from `from` to `to`.
        void AddTo(float *out, std::size_t frames, float from, float to);

    private:
        ::stb_vorbis *vorbis_ = nullptr;
        int channels_ = 2;
        std::vector<float> buffer_;
    };
}
