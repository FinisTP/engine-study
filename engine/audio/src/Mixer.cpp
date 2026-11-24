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

        // 2. Music: each stream's gain moves toward its target by `step` per frame (fade in/out).
        for (MusicStream &m : music_)
        {
            const float frameCount = static_cast<float>(frames), from = m.gain;
            m.gain = m.target > m.gain ? std::min(m.target, m.gain + m.step * frameCount) : std::max(m.target, m.gain - m.step * frameCount);
            if (from > 0.0F || m.gain > 0.0F)
                m.decoder->AddTo(out, frames, from, m.gain);
        }
        std::erase_if(music_, [](const MusicStream &m) { return m.gain <= 0.0F && m.target <= 0.0F; });

        // 3. Drop finished voices (this releases their clips).
        std::erase_if(voices_, [](const Voice &voice) { return voice.frame >= voice.clip->Frames(); });

        // 4. Several loud sounds can add past 1; clamp instead of wrapping around.
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

    void Mixer::PlayMusic(std::shared_ptr<const MusicTrack> track, float volume, float fadeSeconds, bool keepTime)
    {
        const std::scoped_lock lock(mutex_);
        if (track && !music_.empty() && music_.back().track == track && music_.back().target > 0.0F)
        {
            music_.back().target = std::clamp(volume, 0.0F, 1.0F); // already playing: only the volume changes
            return;
        }
        // 1. Everything playing fades out (its moment kept first, for a variant that takes over there).
        const std::size_t at = keepTime && !music_.empty() ? music_.back().decoder->Frame() : 0;
        const float frames = std::max(fadeSeconds, 0.01F) * static_cast<float>(mixRate);
        for (MusicStream &m : music_)
        {
            m.target = 0.0F;
            m.step = std::max(m.gain, 0.01F) / frames;
        }
        // 2. The new track fades in from silence, from its start.
        if (track)
        {
            MusicStream stream{track, std::make_unique<MusicDecoder>(*track), 0.0F, std::clamp(volume, 0.0F, 1.0F), 0.0F};
            stream.step = stream.target / frames;
            if (at > 0)
                stream.decoder->Seek(at);
            if (stream.decoder->Ok())
                music_.push_back(std::move(stream));
        }
        if (music_.size() > 3)
            music_.erase(music_.begin()); // switching very fast: drop the oldest at once
    }

    std::size_t Mixer::MusicFrame() const
    {
        const std::scoped_lock lock(mutex_);
        return music_.empty() ? 0 : music_.back().decoder->Frame();
    }

    std::shared_ptr<const MusicTrack> Mixer::Music() const
    {
        const std::scoped_lock lock(mutex_);
        return !music_.empty() && music_.back().target > 0.0F ? music_.back().track : nullptr;
    }

    std::size_t Mixer::VoiceCount() const
    {
        const std::scoped_lock lock(mutex_);
        return voices_.size();
    }
}
