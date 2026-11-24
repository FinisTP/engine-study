#include <Ogg.hpp>

#include <fstream>
#include <algorithm>
#include <iterator>

// stb_vorbis is C written for many compilers; its warnings are not ours.
#if defined(_MSC_VER)
#pragma warning(push, 0)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Weverything"
#else
#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
#endif
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#include "../third_party/stb_vorbis.c"
#if defined(_MSC_VER)
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
// stb_vorbis defines these short macro names; they would clash with code below.
#undef L
#undef C
#undef R

namespace umbra::audio
{
    namespace
    {
        // Opens the file and checks it fits the mixer: 48000 Hz, 1 or 2 channels.
        stb_vorbis *Open(const unsigned char *data, std::size_t size, int &channels, std::string &error)
        {
            if (size < 4 || data[0] != 'O' || data[1] != 'g' || data[2] != 'g' || data[3] != 'S')
            {
                error = "not an Ogg file";
                return nullptr;
            }
            int code = 0;
            stb_vorbis *v = stb_vorbis_open_memory(data, static_cast<int>(size), &code, nullptr);
            if (v == nullptr)
            {
                error = "not a readable Ogg Vorbis file (error " + std::to_string(code) + ")";
                return nullptr;
            }
            const stb_vorbis_info info = stb_vorbis_get_info(v);
            if (static_cast<int>(info.sample_rate) != mixRate || info.channels < 1 || info.channels > 2)
            {
                error = "sample rate " + std::to_string(info.sample_rate) + " Hz, " + std::to_string(info.channels) +
                        " channel(s); needs 48000 Hz, mono or stereo";
                stb_vorbis_close(v);
                return nullptr;
            }
            channels = info.channels;
            return v;
        }
    }

    std::optional<AudioClip> ParseOgg(std::span<const std::byte> bytes, std::string &error)
    {
        AudioClip clip;
        stb_vorbis *v = Open(reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(), clip.channels, error);
        if (v == nullptr)
            return std::nullopt;
        // Decode in blocks until the end (a short sound: a few seconds at most).
        float block[4096];
        for (;;)
        {
            const int frames = stb_vorbis_get_samples_float_interleaved(v, clip.channels, block, 4096);
            if (frames <= 0)
                break;
            clip.samples.insert(clip.samples.end(), block, block + frames * clip.channels);
        }
        stb_vorbis_close(v);
        if (clip.samples.empty())
        {
            error = "no samples";
            return std::nullopt;
        }
        return clip;
    }

    std::optional<MusicTrack> ParseMusic(std::vector<unsigned char> ogg, std::string &error)
    {
        MusicTrack track;
        stb_vorbis *v = Open(ogg.data(), ogg.size(), track.channels, error);
        if (v == nullptr)
            return std::nullopt;
        track.seconds = stb_vorbis_stream_length_in_seconds(v);
        stb_vorbis_close(v);
        track.ogg = std::move(ogg);
        return track;
    }

    std::optional<MusicTrack> LoadMusic(const std::filesystem::path &path, std::string &error)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            error = "file not found";
            return std::nullopt;
        }
        return ParseMusic({std::istreambuf_iterator<char>(file), {}}, error);
    }

    MusicDecoder::MusicDecoder(const MusicTrack &track)
    {
        std::string error;
        vorbis_ = Open(track.ogg.data(), track.ogg.size(), channels_, error);
        length_ = vorbis_ != nullptr ? stb_vorbis_stream_length_in_samples(vorbis_) : 0;
    }

    MusicDecoder::~MusicDecoder()
    {
        if (vorbis_ != nullptr)
            stb_vorbis_close(vorbis_);
    }

    std::size_t MusicDecoder::Frame() const
    {
        return frame_;
    }

    void MusicDecoder::Seek(std::size_t frame)
    {
        if (vorbis_ == nullptr)
            return;
        frame_ = length_ > 0 ? frame % length_ : 0;
        (void)stb_vorbis_seek(vorbis_, static_cast<unsigned int>(frame_));
    }

    void MusicDecoder::AddTo(float *out, std::size_t frames, float from, float to)
    {
        // 1. Decode exactly `frames` frames; at the end of the file jump back to the start (a loop).
        buffer_.resize(frames * static_cast<std::size_t>(channels_));
        std::size_t done = 0;
        bool restarted = false; // nothing right after a restart = an empty file: stay silent
        while (vorbis_ != nullptr && done < frames)
        {
            const int got = stb_vorbis_get_samples_float_interleaved(vorbis_, channels_, buffer_.data() + done * static_cast<std::size_t>(channels_),
                                                                     static_cast<int>((frames - done) * static_cast<std::size_t>(channels_)));
            if (got <= 0)
            {
                if (restarted)
                    break;
                stb_vorbis_seek_start(vorbis_);
                frame_ = 0;
                restarted = true;
                continue;
            }
            restarted = false;
            done += static_cast<std::size_t>(got);
            frame_ += static_cast<std::size_t>(got);
        }
        // 2. Add to the output with a smooth gain ramp (fades without steps); mono goes to both sides.
        for (std::size_t i = 0; i < done; ++i)
        {
            const float gain = from + (to - from) * static_cast<float>(i) / static_cast<float>(frames);
            const float left = buffer_[i * static_cast<std::size_t>(channels_)];
            const float right = channels_ == 2 ? buffer_[i * 2 + 1] : left;
            out[i * 2] += left * gain;
            out[i * 2 + 1] += right * gain;
        }
    }
}
