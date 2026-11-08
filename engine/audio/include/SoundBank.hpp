#pragma once

#include <AssetFolders.hpp>

#include <Diagnostic.hpp>
#include <Wav.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace umbra::audio
{
    // Loads sound files from the asset folder once and shares them.
    // A missing or unsupported file returns nullptr and is reported once, so the game
    // keeps running silently instead of failing.
    class SoundBank final
    {
    public:
        explicit SoundBank(assets::AssetFolders assetRoot) : assetRoot_(std::move(assetRoot)) {}

        [[nodiscard]] std::shared_ptr<const AudioClip> Get(const std::string &path);
        // Problems found since the last call.
        [[nodiscard]] std::vector<core::Diagnostic> TakeDiagnostics();
        // Forgets every clip. Clips still playing stay alive until their voice ends.
        void Clear() { clips_.clear(); }
        [[nodiscard]] std::size_t Count() const noexcept { return clips_.size(); }

    private:
        assets::AssetFolders assetRoot_; // project, then plugins
        std::map<std::string, std::shared_ptr<const AudioClip>> clips_; // nullptr = failed, don't retry
        std::vector<core::Diagnostic> diagnostics_;
    };
}
