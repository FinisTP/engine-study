#include <Campaign.hpp>

#include <algorithm>
#include <utility>

namespace umbra::campaign
{
    namespace
    {
        // "chi" with its dot below, in UTF-8 (sources stay ASCII).
        const std::string chi = "ch\xe1\xbb\x8b", Chi = "Ch\xe1\xbb\x8b";

        void Replace(std::string &text, const std::string &from, const std::string &to)
        {
            for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
                text.replace(at, from.size(), to);
        }

        // The byte where the n-th letter (code point) starts.
        std::size_t ByteOf(const std::string &text, std::size_t n)
        {
            std::size_t i = 0;
            for (std::size_t letters = 0; i < text.size(); ++i)
                if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80 && letters++ == n)
                    return i;
            return text.size();
        }
    }

    std::string FillTokens(const std::string &text, const Speaker &player, const Cast &cast)
    {
        // 1. The name, or the cast's default.
        std::string name = player.name;
        if (name.empty())
            if (const CastMember *nameless = cast.Find("nameless"))
                name = nameless->name;
        // 2. The form of address by gender.
        const bool male = player.gender == "male";
        std::string out = text;
        Replace(out, "{player}", name);
        Replace(out, "{anh/" + chi + "}", male ? "anh" : chi);
        Replace(out, "{Anh/" + Chi + "}", male ? "Anh" : Chi);
        return out;
    }

    ScenePlayer::ScenePlayer(const Scene &scene, const Cast &cast, Speaker player, Facts &facts)
        : scene_(scene), cast_(cast), player_(std::move(player)), facts_(facts)
    {
        Enter(0);
    }

    void ScenePlayer::Enter(std::size_t index)
    {
        at_ = index;
        // 0. A line for another answer is passed over (its "when" does not hold).
        while (!Done() && !scene_.lines[at_].when.empty())
        {
            const std::string &when = scene_.lines[at_].when;
            const std::size_t eq = when.find('=');
            const auto it = facts_.find(when.substr(0, eq));
            if (it != facts_.end() && (eq == std::string::npos || it->second == when.substr(eq + 1)))
                break;
            ++at_;
        }
        if (Done())
            return;
        const Line &line = scene_.lines[at_];
        // 1. Who leaves, who comes in; one in a taken place sends the other out.
        for (const std::string &who : line.leave)
            stage_.erase(who);
        for (const Stage &s : line.enter)
        {
            std::erase_if(stage_, [&](const auto &p) { return p.second.side == s.side && p.second.slot == s.slot && p.first != s.who; });
            stage_[s.who] = s;
        }
        // 2. The speaker's face; the words with tokens filled.
        if (auto it = stage_.find(line.speaker); it != stage_.end() && !line.face.empty())
            it->second.face = line.face;
        const CastMember *speaker = cast_.Find(line.speaker);
        name_ = speaker ? FillTokens(speaker->name, player_, cast_) : std::string();
        text_ = FillTokens(line.text, player_, cast_);
        letters_ = 0;
        for (const char ch : text_)
            letters_ += (static_cast<unsigned char>(ch) & 0xC0) != 0x80;
        shown_ = 0;
        progress_ = 0.0F;
        sound_ = line.sound;
        log_.emplace_back(name_, text_);
    }

    void ScenePlayer::Next(const std::string &jump)
    {
        std::size_t next = at_ + 1;
        if (!jump.empty())
            for (std::size_t i = 0; i < scene_.lines.size(); ++i)
                if (scene_.lines[i].id == jump)
                    next = i;
        Enter(next);
    }

    void ScenePlayer::Update(float seconds, float lettersPerSecond)
    {
        if (Done() || Complete())
            return;
        progress_ += seconds * lettersPerSecond;
        shown_ = std::min(letters_, static_cast<std::size_t>(progress_));
    }

    void ScenePlayer::Advance()
    {
        if (Done())
            return;
        if (!Complete())
            shown_ = letters_;
        else if (scene_.lines[at_].choices.empty())
            Next(scene_.lines[at_].jump);
    }

    void ScenePlayer::Skip()
    {
        while (!Done())
        {
            shown_ = letters_;
            if (!scene_.lines[at_].choices.empty())
                return;
            Next(scene_.lines[at_].jump);
        }
    }

    bool ScenePlayer::Choose(int index)
    {
        const std::vector<std::string> shown = Choices();
        if (index < 0 || static_cast<std::size_t>(index) >= shown.size())
            return false;
        const Choice &c = scene_.lines[at_].choices[static_cast<std::size_t>(index)];
        if (!c.fact.empty())
            facts_[c.fact] = c.value;
        Next(c.jump);
        return true;
    }

    std::string ScenePlayer::Shown() const { return Done() ? std::string() : text_.substr(0, ByteOf(text_, shown_)); }

    Side ScenePlayer::SpeakerSide() const
    {
        if (Done())
            return Side::Left;
        const auto it = stage_.find(scene_.lines[at_].speaker);
        return it == stage_.end() ? Side::Left : it->second.side;
    }

    std::vector<std::string> ScenePlayer::Choices() const
    {
        std::vector<std::string> out;
        if (!Done() && Complete())
            for (const Choice &c : scene_.lines[at_].choices)
                out.push_back(FillTokens(c.text, player_, cast_));
        return out;
    }

    std::array<Portrait, 2> ScenePlayer::Row(Side side) const
    {
        std::array<Portrait, 2> row;
        const std::string speaker = Done() ? std::string() : scene_.lines[at_].speaker;
        for (const auto &[who, s] : stage_)
            if (s.side == side)
                row[static_cast<std::size_t>(s.slot)] = {who, s.face, who == speaker};
        return row;
    }

    std::string ScenePlayer::TakeSound() { return std::exchange(sound_, {}); }
}
