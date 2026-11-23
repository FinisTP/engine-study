#include "DialogView.hpp"

#include <cmath>
#include <cstdio>

namespace
{
    using umbra::campaign::Form;

    // 1. The form's colour (Campaign.md); lit portraits keep it, the others are dimmed.
    std::array<int, 4> FormColour(Form form)
    {
        switch (form)
        {
        case Form::Strung: return {0xff, 0x9a, 0x9a, 0xff};
        case Form::Ghost: return {0xd8, 0xe4, 0xff, 0xcc};
        case Form::Ally: return {0xa8, 0xc8, 0xff, 0xff};
        case Form::Host: return {0xff, 0xe8, 0xd0, 0xff};
        default: return {0xff, 0xff, 0xff, 0xff};
        }
    }

    std::string Tint(Form form, bool lit)
    {
        const std::array<int, 4> c = FormColour(form);
        const int dim = lit ? 0xff : 0x8a;
        const auto mul = [](int a, int b) { return static_cast<int>(std::lround(a * b / 255.0)); };
        char text[10];
        std::snprintf(text, sizeof(text), "#%02x%02x%02x%02x", mul(c[0], dim), mul(c[1], dim), mul(c[2], dim), c[3]);
        return text;
    }

    // 2. The face's picture: the one asked for, else calm, else none.
    std::string Face(const std::string &sprite, const std::string &face, const std::function<bool(const std::string &)> &exists)
    {
        for (const std::string &f : {face.empty() ? std::string("calm") : face, std::string("calm")})
            if (const std::string path = "art/sprites/" + sprite + "/face_" + f + ".png"; !sprite.empty() && exists(path))
                return path;
        return {};
    }
}

nlohmann::json DialogData(const umbra::campaign::ScenePlayer &p, const umbra::campaign::Cast &cast, const umbra::campaign::Speaker &player,
                          const std::function<bool(const std::string &)> &exists)
{
    using umbra::campaign::Side;
    nlohmann::json slots;
    // 1. Four places: l0 and r0 near the middle; the right side mirrored to face it.
    for (const Side side : {Side::Left, Side::Right})
    {
        const auto row = side == Side::Left ? p.Left() : p.Right();
        for (std::size_t slot = 0; slot < row.size(); ++slot)
        {
            const umbra::campaign::Portrait &who = row[slot];
            const umbra::campaign::CastMember *m = cast.Find(who.who);
            std::string sprite;
            if (m)
                sprite = m->id == "player" ? (player.gender == "male" ? "player_male" : "player_female")
                                           : (m->form == Form::Strung && !m->bound.empty() ? m->bound : m->sprite);
            const std::string image = m ? Face(sprite, who.face, exists) : std::string();
            slots[std::string(side == Side::Left ? "l" : "r") + std::to_string(slot)] = {
                {"image", image}, {"show", !image.empty()}, {"flip", side == Side::Right}, {"tint", Tint(m ? m->form : Form::Ghost, who.lit)}};
        }
    }
    // 2. The name tag on the speaker's side, the text, the arrow or the choices.
    const std::vector<std::string> choices = p.Choices();
    nlohmann::json list = nlohmann::json::array();
    for (std::size_t i = 0; i < choices.size(); ++i)
        list.push_back({{"text", choices[i]}, {"index", static_cast<int>(i)}, {"key", std::to_string(i + 1)}});
    nlohmann::json log = nlohmann::json::array();
    for (const auto &[name, text] : p.Log())
        log.push_back({{"name", name}, {"text", text}});
    const bool named = !p.SpeakerName().empty();
    return {{"dialog",
             {{"slots", slots},
              {"name", p.SpeakerName()},
              {"narration", !named},
              {"nameLeft", named && p.SpeakerSide() == Side::Left},
              {"nameRight", named && p.SpeakerSide() == Side::Right},
              {"text", p.Shown()},
              {"more", p.Complete() && choices.empty()},
              {"choices", list},
              {"hasChoices", !choices.empty()},
              {"log", log}}}};
}
