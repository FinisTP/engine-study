#include <SoundBank.hpp>

#include <ModelCache.hpp>

#include <fstream>
#include <iterator>
#include <span>
#include <utility>

namespace umbra::audio
{
    std::shared_ptr<const AudioClip> SoundBank::Get(const std::string &path)
    {
        if (const auto it = clips_.find(path); it != clips_.end())
            return it->second;

        // First request: load it, or remember the failure so it is reported only once.
        std::shared_ptr<const AudioClip> clip;
        std::string error;
        if (!assets::IsSafeAssetPath(path))
            error = "path must be relative to the asset folder and must not contain '..'";
        else if (path.ends_with(".ogg"))
        {
            std::ifstream file(assetRoot_ / path, std::ios::binary);
            const std::vector<char> bytes{std::istreambuf_iterator<char>(file), {}};
            if (!file && bytes.empty())
                error = "file not found";
            else if (std::optional<AudioClip> loaded = ParseOgg(std::as_bytes(std::span(bytes)), error))
                clip = std::make_shared<const AudioClip>(std::move(*loaded));
        }
        else if (std::optional<AudioClip> loaded = LoadWav(assetRoot_ / path, error))
            clip = std::make_shared<const AudioClip>(std::move(*loaded));
        if (!clip)
            diagnostics_.push_back({core::Severity::Warning, "sound.load", path, "", error + " (sound is skipped)"});
        clips_[path] = clip;
        return clip;
    }

    std::shared_ptr<const MusicTrack> SoundBank::GetMusic(const std::string &path)
    {
        if (const auto it = music_.find(path); it != music_.end())
            return it->second;
        std::shared_ptr<const MusicTrack> track;
        std::string error;
        if (!assets::IsSafeAssetPath(path))
            error = "path must be relative to the asset folder and must not contain '..'";
        else if (!path.ends_with(".ogg"))
            error = "music must be an .ogg file";
        else if (std::optional<MusicTrack> loaded = LoadMusic(assetRoot_ / path, error))
            track = std::make_shared<const MusicTrack>(std::move(*loaded));
        if (!track)
            diagnostics_.push_back({core::Severity::Warning, "music.load", path, "", error + " (music is skipped)"});
        music_[path] = track;
        return track;
    }

    std::vector<core::Diagnostic> SoundBank::TakeDiagnostics()
    {
        return std::exchange(diagnostics_, {});
    }
}
