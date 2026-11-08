#include <SoundAssistant.hpp>

#include <Material.hpp> // ModelLicense

#include <vorbis/vorbisenc.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>

// dr_flac (public domain / MIT-0) reads what ComfyUI saves; its warnings are not ours.
#if defined(_MSC_VER)
#pragma warning(push, 0)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Weverything"
#else
#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
#endif
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#include "../third_party/dr_flac.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

namespace umbra::localai
{
    namespace
    {
        constexpr float targetPeak = 0.89F; // -1 dBFS: loud, with room so nothing clips
        constexpr float pi = 3.14159265F;

        bool WriteFileAtomic(const std::filesystem::path &file, const std::string &bytes)
        {
            std::error_code error;
            std::filesystem::create_directories(file.parent_path(), error);
            const std::filesystem::path temporary = file.string() + ".tmp";
            {
                std::ofstream out(temporary, std::ios::binary);
                if (!out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
                    return false;
            }
            std::filesystem::rename(temporary, file, error);
            return !error;
        }

        std::string Today()
        {
            const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char text[16];
            std::strftime(text, sizeof(text), "%Y-%m-%d", std::localtime(&now));
            return text;
        }

        bool Has(const std::string &name, const char *part) { return name.find(part) != std::string::npos; }

        float Peak(const std::vector<float> &samples)
        {
            float peak = 0.0F;
            for (const float s : samples)
                peak = std::max(peak, std::abs(s));
            return peak;
        }

        void Scale(std::vector<float> &samples, float gain)
        {
            for (float &s : samples)
                s *= gain;
        }

        void PutU32(std::string &out, std::uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
                out += static_cast<char>((v >> (8 * i)) & 0xFF);
        }
    }

    SoundStyle LoadSoundStyle(const assets::AssetFolders &assetRoot)
    {
        SoundStyle style;
        std::ifstream in(assetRoot / "audio/style.json");
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_object())
        {
            style.music = j.value("music", style.music);
            style.effect = j.value("effect", style.effect);
        }
        return style;
    }

    std::string PreferredSoundModel(const std::vector<std::string> &installed, SoundKind kind, const std::string &wanted)
    {
        // ACE-Step needs its all-in-one checkpoint here (CheckpointLoaderSimple loads model, text and VAE).
        const auto first = [&](auto fits) {
            for (const std::string &name : installed)
                if (fits(name))
                    return name;
            return std::string();
        };
        if (!wanted.empty() && std::find(installed.begin(), installed.end(), wanted) != installed.end())
            return wanted;
        const auto ace = [](const std::string &n) { return (Has(n, "ace_step") || Has(n, "acestep")) && Has(n, "aio"); };
        const auto sfx = [](const std::string &n) { return Has(n, "stable_audio_3") && Has(n, "sfx"); };
        const auto sa3 = [](const std::string &n) { return Has(n, "stable_audio_3"); };
        const auto anyAudio = [&](const std::string &n) { return ace(n) || sa3(n); };
        std::string pick = kind == SoundKind::Music ? first(ace) : first(sfx);
        if (pick.empty())
            pick = kind == SoundKind::Music ? first(sa3) : first(sa3);
        return pick.empty() ? first(anyAudio) : pick;
    }

    std::vector<AudioRequest> MakeSoundRequests(const std::string &idea, SoundKind kind, const SoundStyle &style, const std::string &model,
                                                const std::string &host, float seconds, const std::vector<long long> &seeds)
    {
        std::vector<AudioRequest> requests;
        for (const long long seed : seeds)
        {
            AudioRequest r;
            r.host = host;
            r.model = model;
            r.prompt = idea + ", " + (kind == SoundKind::Music ? style.music : style.effect);
            r.seconds = seconds;
            r.seed = seed;
            requests.push_back(r);
        }
        return requests;
    }

    bool DecodeFlac(const std::vector<std::uint8_t> &bytes, Pcm &pcm, std::string &error)
    {
        unsigned int channels = 0, rate = 0;
        drflac_uint64 frames = 0;
        float *data = drflac_open_memory_and_read_pcm_frames_f32(bytes.data(), bytes.size(), &channels, &rate, &frames, nullptr);
        if (data == nullptr || channels < 1 || channels > 2 || rate < 8000 || frames == 0)
        {
            drflac_free(data, nullptr);
            error = "not a readable mono/stereo FLAC file";
            return false;
        }
        pcm.channels = static_cast<int>(channels);
        pcm.rate = static_cast<int>(rate);
        pcm.samples.assign(data, data + frames * channels);
        drflac_free(data, nullptr);
        return true;
    }

    Pcm Resample(const Pcm &pcm, int rate)
    {
        if (pcm.rate == rate || pcm.Frames() < 2)
        {
            Pcm same = pcm;
            same.rate = rate;
            return same;
        }
        // Catmull-Rom: each new sample from the 4 old ones around its position (SoundGen.md).
        Pcm out;
        out.rate = rate;
        out.channels = pcm.channels;
        const std::size_t inFrames = pcm.Frames();
        const auto outFrames = static_cast<std::size_t>(static_cast<double>(inFrames) * rate / pcm.rate);
        out.samples.resize(outFrames * static_cast<std::size_t>(pcm.channels));
        const auto at = [&](std::ptrdiff_t frame, int c) {
            frame = std::clamp<std::ptrdiff_t>(frame, 0, static_cast<std::ptrdiff_t>(inFrames) - 1);
            return pcm.samples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(pcm.channels) + static_cast<std::size_t>(c)];
        };
        for (std::size_t j = 0; j < outFrames; ++j)
        {
            const double t = static_cast<double>(j) * pcm.rate / rate;
            const auto i = static_cast<std::ptrdiff_t>(t);
            const auto f = static_cast<float>(t - static_cast<double>(i));
            for (int c = 0; c < pcm.channels; ++c)
            {
                const float p0 = at(i - 1, c), p1 = at(i, c), p2 = at(i + 1, c), p3 = at(i + 2, c);
                out.samples[j * static_cast<std::size_t>(pcm.channels) + static_cast<std::size_t>(c)] =
                    p1 + 0.5F * f * (p2 - p0 + f * (2.0F * p0 - 5.0F * p1 + 4.0F * p2 - p3 + f * (3.0F * (p1 - p2) + p3 - p0)));
            }
        }
        return out;
    }

    SoundCandidate FinishSound(const Pcm &input, SoundKind kind, const AudioRequest &request)
    {
        SoundCandidate result;
        result.request = request;
        Pcm pcm = Resample(input, audio::mixRate);
        audio::AudioClip &clip = result.clip;
        if (kind == SoundKind::Music)
        {
            // 1. Stereo; the loop: the last `fade` frames are blended into the first ones and cut off,
            //    so the end runs straight into the start (equal-power crossfade, SoundGen.md).
            clip.channels = 2;
            const std::size_t frames = pcm.Frames();
            const std::size_t fade = std::min<std::size_t>(2 * audio::mixRate, frames / 4);
            const std::size_t length = frames - fade;
            clip.samples.resize(length * 2);
            const auto sample = [&](std::size_t frame, int c) {
                return pcm.samples[frame * static_cast<std::size_t>(pcm.channels) + (pcm.channels == 2 ? static_cast<std::size_t>(c) : 0)];
            };
            for (std::size_t i = 0; i < length; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    float v = sample(i, c);
                    if (i < fade)
                    {
                        const float w = static_cast<float>(i) / static_cast<float>(fade) * pi * 0.5F;
                        v = v * std::sin(w) + sample(length + i, c) * std::cos(w);
                    }
                    clip.samples[i * 2 + static_cast<std::size_t>(c)] = v;
                }
        }
        else
        {
            // 1. Mono; trim the quiet start and end (keep 5 ms before the sound, 60 ms after).
            clip.channels = 1;
            std::vector<float> mono(pcm.Frames());
            for (std::size_t i = 0; i < mono.size(); ++i)
                mono[i] = pcm.channels == 2 ? 0.5F * (pcm.samples[i * 2] + pcm.samples[i * 2 + 1]) : pcm.samples[i];
            const float threshold = std::max(0.002F, Peak(mono) * 0.03F);
            const auto loud = [&](float s) { return std::abs(s) > threshold; };
            const auto first = std::find_if(mono.begin(), mono.end(), loud);
            const auto last = std::find_if(mono.rbegin(), mono.rend(), loud);
            if (first != mono.end())
            {
                const std::size_t start = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, (first - mono.begin()) - audio::mixRate / 200));
                const std::size_t end = std::min(mono.size(), static_cast<std::size_t>(mono.rend() - last) + audio::mixRate * 6 / 100);
                mono = std::vector<float>(mono.begin() + static_cast<std::ptrdiff_t>(start), mono.begin() + static_cast<std::ptrdiff_t>(end));
            }
            // 2. Fades: 2 ms in (no click, keeps the attack), 30 ms out.
            const std::size_t in = std::min<std::size_t>(mono.size(), audio::mixRate / 500), out = std::min<std::size_t>(mono.size(), audio::mixRate * 3 / 100);
            for (std::size_t i = 0; i < in; ++i)
                mono[i] *= static_cast<float>(i) / static_cast<float>(in);
            for (std::size_t i = 0; i < out; ++i)
                mono[mono.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(out);
            clip.samples = std::move(mono);
        }
        // 3. Loudness: the peak to -1 dBFS. Almost nothing there = warn instead of blowing up noise.
        result.peak = Peak(clip.samples);
        result.silent = result.peak < 0.01F;
        if (!result.silent)
            Scale(clip.samples, targetPeak / result.peak);
        result.seconds = static_cast<float>(clip.Frames()) / static_cast<float>(audio::mixRate);
        return result;
    }

    std::string EncodeWav(const audio::AudioClip &clip)
    {
        // RIFF header (Audio.md), then 16-bit little-endian samples.
        const auto dataBytes = static_cast<std::uint32_t>(clip.samples.size() * 2);
        const auto channels = static_cast<std::uint32_t>(clip.channels);
        std::string out = "RIFF";
        PutU32(out, 36 + dataBytes);
        out += "WAVEfmt ";
        PutU32(out, 16);
        PutU32(out, 1 | (channels << 16));                       // PCM, channels
        PutU32(out, static_cast<std::uint32_t>(audio::mixRate)); // sample rate
        PutU32(out, static_cast<std::uint32_t>(audio::mixRate) * channels * 2);
        PutU32(out, (channels * 2) | (16U << 16));               // block align, bits
        out += "data";
        PutU32(out, dataBytes);
        for (const float s : clip.samples)
        {
            const auto v = static_cast<std::int16_t>(std::lround(std::clamp(s, -1.0F, 1.0F) * 32767.0F));
            out += static_cast<char>(v & 0xFF);
            out += static_cast<char>((v >> 8) & 0xFF);
        }
        return out;
    }

    bool EncodeOgg(const audio::AudioClip &clip, float quality, std::string &out, std::string &error)
    {
        // libvorbis: set up, write the 3 header packets, then feed blocks of samples and collect the
        // pages it produces (the usual libvorbisenc loop).
        vorbis_info info;
        vorbis_info_init(&info);
        if (vorbis_encode_init_vbr(&info, clip.channels, audio::mixRate, std::clamp(quality, -0.1F, 1.0F)) != 0)
        {
            vorbis_info_clear(&info);
            error = "the Vorbis encoder refused these settings";
            return false;
        }
        vorbis_comment comment;
        vorbis_comment_init(&comment);
        vorbis_comment_add_tag(&comment, "ENCODER", "Umbra");
        vorbis_dsp_state dsp;
        vorbis_block block;
        vorbis_analysis_init(&dsp, &info);
        vorbis_block_init(&dsp, &block);
        ogg_stream_state stream;
        ogg_stream_init(&stream, 0x55AB);
        ogg_page page;
        ogg_packet packet;
        const auto write = [&] { out.append(reinterpret_cast<const char *>(page.header), static_cast<std::size_t>(page.header_len));
                                 out.append(reinterpret_cast<const char *>(page.body), static_cast<std::size_t>(page.body_len)); };
        {
            ogg_packet header, commentPacket, codebook;
            vorbis_analysis_headerout(&dsp, &comment, &header, &commentPacket, &codebook);
            ogg_stream_packetin(&stream, &header);
            ogg_stream_packetin(&stream, &commentPacket);
            ogg_stream_packetin(&stream, &codebook);
            while (ogg_stream_flush(&stream, &page) != 0)
                write();
        }
        const std::size_t frames = clip.Frames();
        bool done = false;
        for (std::size_t at = 0; !done;)
        {
            const std::size_t count = std::min<std::size_t>(1024, frames - at);
            if (count == 0)
                vorbis_analysis_wrote(&dsp, 0); // end of stream
            else
            {
                float **buffer = vorbis_analysis_buffer(&dsp, static_cast<int>(count));
                for (std::size_t i = 0; i < count; ++i)
                    for (int c = 0; c < clip.channels; ++c)
                        buffer[c][i] = clip.samples[(at + i) * static_cast<std::size_t>(clip.channels) + static_cast<std::size_t>(c)];
                vorbis_analysis_wrote(&dsp, static_cast<int>(count));
                at += count;
            }
            while (vorbis_analysis_blockout(&dsp, &block) == 1)
            {
                vorbis_analysis(&block, nullptr);
                vorbis_bitrate_addblock(&block);
                while (vorbis_bitrate_flushpacket(&dsp, &packet) != 0)
                {
                    ogg_stream_packetin(&stream, &packet);
                    while (ogg_stream_pageout(&stream, &page) != 0)
                    {
                        write();
                        done = done || ogg_page_eos(&page) != 0;
                    }
                }
            }
            done = done || (count == 0 && ogg_stream_flush(&stream, &page) == 0);
        }
        while (ogg_stream_flush(&stream, &page) != 0)
            write();
        ogg_stream_clear(&stream);
        vorbis_block_clear(&block);
        vorbis_dsp_clear(&dsp);
        vorbis_comment_clear(&comment);
        vorbis_info_clear(&info);
        return true;
    }

    std::string SaveSound(const std::filesystem::path &assetRoot, const std::string &name, SoundKind kind, const SoundCandidate &candidate,
                          std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. A free name.
        const bool music = kind == SoundKind::Music;
        const std::string folder = music ? "audio/music/" : "audio/sfx/", extension = music ? ".ogg" : ".wav";
        std::string base = name.empty() ? (music ? "music" : "sound") : name;
        std::string free = base;
        for (int n = 2; std::filesystem::exists(assetRoot / (folder + free + extension)); ++n)
            free = base + "_" + std::to_string(n);
        const std::string path = folder + free + extension;

        // 2. The sound file, then how it was made (prompt, seed, model and its license).
        std::string bytes, error;
        if (music ? !EncodeOgg(candidate.clip, 0.4F, bytes, error) : (bytes = EncodeWav(candidate.clip)).empty())
        {
            diagnostics.push_back({core::Severity::Error, "save.encode", path, "", "couldn't encode the sound: " + error});
            return "";
        }
        const AudioRequest &r = candidate.request;
        // "models" has the same shape as a generated model's provenance, so package-check warns alike.
        const assets::ModelLicenseInfo license = assets::ModelLicense(r.model);
        const nlohmann::json source{{"format", "umbra.sound-source"}, {"version", 1}, {"generator", "comfyui"},
                                    {"models", {{{"file", r.model}, {"license", license.known ? license.name : "unknown"}, {"commercial", license.known && license.commercial}}}},
                                    {"prompt", r.prompt}, {"seconds", r.seconds}, {"seed", r.seed}, {"kind", music ? "music" : "effect"},
                                    {"looped", music}, {"date", Today()}};
        if (!WriteFileAtomic(assetRoot / path, bytes) || !WriteFileAtomic(assetRoot / (folder + free + ".source.json"), source.dump(2) + "\n"))
        {
            diagnostics.push_back({core::Severity::Error, "save.write", (assetRoot / path).string(), "", "couldn't write the sound file"});
            return "";
        }
        return path;
    }
}
