#include <Mixer.hpp>

#include <algorithm>

namespace umbra::audio
{
    bool Mixer::Play(std::shared_ptr<const AudioClip> clip, float volume, Bus bus)
    {
        if (!clip || clip->Frames() == 0)
            return false;
        const std::scoped_lock lock(mutex_);
        if (voices_.size() >= maxVoices)
            return false;
        voices_.push_back({std::move(clip), 0, std::clamp(volume, 0.0F, 1.0F), bus});
        return true;
    }

    void Mixer::Mix(float *out, std::size_t frames)
    {
        std::fill(out, out + frames * 2, 0.0F);
        const std::scoped_lock lock(mutex_);

        // 1. Add each voice; mono plays the same sample on both sides.
        for (Voice &voice : voices_)
        {
            if (voice.bus == Bus::Game && gamePaused_)
                continue;
            const AudioClip &clip = *voice.clip;
            const std::size_t count = std::min(frames, clip.Frames() - voice.frame);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::size_t at = (voice.frame + i) * static_cast<std::size_t>(clip.channels);
                const float left = clip.samples[at];
                const float right = clip.channels == 2 ? clip.samples[at + 1] : left;
                out[i * 2] += left * voice.volume;
                out[i * 2 + 1] += right * voice.volume;
            }
            voice.frame += count;
        }

        // 2. Drop finished voices (this releases their clips).
        std::erase_if(voices_, [](const Voice &voice) { return voice.frame >= voice.clip->Frames(); });

        // 3. Several loud sounds can add past 1; clamp instead of wrapping around.
        for (std::size_t i = 0; i < frames * 2; ++i)
            out[i] = std::clamp(out[i], -1.0F, 1.0F);
    }

    void Mixer::SetGamePaused(bool paused)
    {
        const std::scoped_lock lock(mutex_);
        gamePaused_ = paused;
    }

    void Mixer::StopAll()
    {
        const std::scoped_lock lock(mutex_);
        voices_.clear();
    }

    std::size_t Mixer::VoiceCount() const
    {
        const std::scoped_lock lock(mutex_);
        return voices_.size();
    }
}
