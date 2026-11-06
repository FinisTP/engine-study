#include <SoundBank.hpp>

#include <ModelCache.hpp>

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
        else if (std::optional<AudioClip> loaded = LoadWav(assetRoot_ / path, error))
            clip = std::make_shared<const AudioClip>(std::move(*loaded));
        if (!clip)
            diagnostics_.push_back({core::Severity::Warning, "sound.load", path, "", error + " (sound is skipped)"});
        clips_[path] = clip;
        return clip;
    }

    std::vector<core::Diagnostic> SoundBank::TakeDiagnostics()
    {
        return std::exchange(diagnostics_, {});
    }
}
