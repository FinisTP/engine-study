#include "Sequences.hpp"

#include <algorithm>
#include <set>

bool Sequence::Has(const std::string &what) const
{
    return std::ranges::any_of(cues, [&](const Cue &c) { return c.what == what; });
}

const Sequence *Sequences::For(const std::string &kind, const std::string &ability) const
{
    for (const auto *names : {&byAbility, &byKind})
        if (const auto it = names->find(names == &byAbility ? ability : kind); it != names->end() && !it->first.empty())
            if (const auto s = named.find(it->second); s != named.end())
                return &s->second;
    return nullptr;
}

std::vector<std::string> Sequences::Effects() const
{
    std::set<std::string> out;
    for (const auto &[name, s] : named)
        for (const Cue &c : s.cues)
            if (!c.effect.empty())
                out.insert(c.effect);
    return {out.begin(), out.end()};
}

bool LoadSequences(const nlohmann::json &j, Sequences &out, std::vector<std::string> &errors)
{
    const std::size_t before = errors.size();
    if (!j.is_object() || j.value("format", "") != "lastcurtain.sequences" || j.value("version", 0) != 1)
        return errors.push_back("expected \"format\": \"lastcurtain.sequences\", \"version\": 1"), false;
    // 1. Each sequence: a length and its cues, each a known kind within the length.
    static const std::set<std::string> kinds{"camera", "effect", "projectile", "hit", "shake", "sound", "bubble"};
    const nlohmann::json sequences = j.value("sequences", nlohmann::json::object()); // kept: items() of a temporary dangles
    for (const auto &[name, s] : sequences.items())
    {
        Sequence seq;
        seq.length = s.value("length", 0.0F);
        if (seq.length <= 0.0F)
            errors.push_back(name + ".length: seconds the line is held, more than 0");
        for (const nlohmann::json &c : s.value("cues", nlohmann::json::array()))
        {
            Cue cue{c.value("at", 0.0F), c.value("do", ""), c.value("on", "user"), c.value("to", ""), c.value("effect", ""), c.value("sound", ""),
                    c.value("seconds", 0.0F), c.value("amount", 0.0F)};
            const std::string at = name + "." + cue.what;
            if (!kinds.contains(cue.what))
                errors.push_back(at + ": \"do\" is one of camera, effect, projectile, hit, shake, sound, bubble");
            if (cue.at < 0.0F || cue.at + cue.seconds > seq.length)
                errors.push_back(at + ": \"at\" (plus a projectile's seconds) must be within the length");
            if (cue.on != "user" && cue.on != "target" && !(cue.on == "all" && cue.what == "camera"))
                errors.push_back(at + ": \"on\" is user or target (a camera also all)");
            if ((cue.what == "effect" || cue.what == "projectile") && cue.effect.empty())
                errors.push_back(at + ": names its \"effect\"");
            seq.cues.push_back(cue);
        }
        std::ranges::stable_sort(seq.cues, {}, &Cue::at);
        out.named[name] = seq;
    }
    // 2. Who plays which: every name must be a sequence.
    out.byKind = j.value("byKind", std::map<std::string, std::string>{});
    out.byAbility = j.value("byAbility", std::map<std::string, std::string>{});
    for (const auto *names : {&out.byKind, &out.byAbility})
        for (const auto &[key, name] : *names)
            if (!out.named.contains(name))
                errors.push_back(key + ": no sequence \"" + name + "\"");
    return errors.size() == before;
}
