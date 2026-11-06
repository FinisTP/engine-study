#include <Wav.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>

namespace umbra::audio
{
    namespace
    {
        constexpr std::uintmax_t maxFileBytes = 16ull * 1024 * 1024;

        // Little-endian reads; the caller has already checked the bounds.
        std::uint32_t Read32(const std::byte *p)
        {
            return std::to_integer<std::uint32_t>(p[0]) | std::to_integer<std::uint32_t>(p[1]) << 8 |
                   std::to_integer<std::uint32_t>(p[2]) << 16 | std::to_integer<std::uint32_t>(p[3]) << 24;
        }

        std::uint16_t Read16(const std::byte *p)
        {
            return static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(p[0]) | std::to_integer<std::uint16_t>(p[1]) << 8);
        }

        bool Tag(const std::byte *p, const char *tag) { return std::memcmp(p, tag, 4) == 0; }
    }

    std::optional<AudioClip> ParseWav(std::span<const std::byte> bytes, std::string &error)
    {
        // 1. Header: "RIFF" <size> "WAVE".
        if (bytes.size() < 12 || !Tag(bytes.data(), "RIFF") || !Tag(bytes.data() + 8, "WAVE"))
        {
            error = "not a RIFF/WAVE file";
            return std::nullopt;
        }

        // 2. Walk the chunks: 4-byte id, 4-byte size, data, padded to an even length.
        std::span<const std::byte> format, data;
        std::size_t offset = 12;
        while (offset + 8 <= bytes.size())
        {
            const std::byte *chunk = bytes.data() + offset;
            const std::size_t size = Read32(chunk + 4);
            if (size > bytes.size() - offset - 8)
            {
                error = "chunk runs past the end of the file";
                return std::nullopt;
            }
            if (Tag(chunk, "fmt "))
                format = {chunk + 8, size};
            else if (Tag(chunk, "data"))
                data = {chunk + 8, size};
            offset += 8 + size + (size & 1);
        }
        if (format.size() < 16 || data.empty())
        {
            error = "missing 'fmt ' or 'data' chunk";
            return std::nullopt;
        }

        // 3. Only the one controlled format.
        const std::uint16_t encoding = Read16(format.data());
        const std::uint16_t channels = Read16(format.data() + 2);
        const std::uint32_t rate = Read32(format.data() + 4);
        const std::uint16_t bits = Read16(format.data() + 14);
        if (encoding != 1 || bits != 16)
        {
            error = "only 16-bit PCM is supported";
            return std::nullopt;
        }
        if (channels != 1 && channels != 2)
        {
            error = "only mono or stereo is supported";
            return std::nullopt;
        }
        if (rate != static_cast<std::uint32_t>(mixRate))
        {
            error = "sample rate is " + std::to_string(rate) + " Hz; convert it to 48000 Hz";
            return std::nullopt;
        }

        // 4. Samples: signed 16-bit -> float in -1..1 (a trailing partial frame is dropped).
        AudioClip clip;
        clip.channels = channels;
        const std::size_t frames = data.size() / (2u * channels);
        clip.samples.resize(frames * channels);
        for (std::size_t i = 0; i < clip.samples.size(); ++i)
            clip.samples[i] = static_cast<float>(static_cast<std::int16_t>(Read16(data.data() + i * 2))) / 32768.0F;
        if (clip.samples.empty())
        {
            error = "no samples";
            return std::nullopt;
        }
        return clip;
    }

    std::optional<AudioClip> LoadWav(const std::filesystem::path &path, std::string &error)
    {
        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(path, ec);
        if (ec)
        {
            error = "cannot read file: " + ec.message();
            return std::nullopt;
        }
        if (size > maxFileBytes)
        {
            error = "file is larger than 16 MiB";
            return std::nullopt;
        }
        std::ifstream file(path, std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>(file), {}};
        return ParseWav(std::as_bytes(std::span{text.data(), text.size()}), error);
    }
}
