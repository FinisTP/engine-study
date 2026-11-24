#include "Keys.hpp"
#include <algorithm>

#include <utility>

namespace umbra::night
{
    namespace
    {
        using platform::InputKey;
        // Key names as written in the file and shown in prompts.
        const std::vector<std::pair<std::string, InputKey>> &Names()
        {
            static const std::vector<std::pair<std::string, InputKey>> names{
                {"Esc", InputKey::Escape}, {"Space", InputKey::Space}, {"Enter", InputKey::Enter}, {"Tab", InputKey::Tab}, {"Backspace", InputKey::Backspace},
                {"W", InputKey::W}, {"A", InputKey::A}, {"S", InputKey::S}, {"D", InputKey::D}, {"E", InputKey::E}, {"R", InputKey::R},
                {"Q", InputKey::Q}, {"F", InputKey::F}, {"I", InputKey::I}, {"K", InputKey::K}, {"J", InputKey::J}, {"H", InputKey::H},
                {"L", InputKey::L}, {"Up", InputKey::Up}, {"Down", InputKey::Down}, {"Left", InputKey::Left}, {"Right", InputKey::Right},
                {"1", InputKey::Num1}, {"2", InputKey::Num2}, {"3", InputKey::Num3}, {"4", InputKey::Num4}, {"5", InputKey::Num5}, {"6", InputKey::Num6},
                {"F1", InputKey::F1}, {"F5", InputKey::F5}, {"F9", InputKey::F9}};
            return names;
        }
        const std::vector<std::pair<std::string, InputKey>> &PadNames()
        {
            static const std::vector<std::pair<std::string, InputKey>> names{
                {"PadA", InputKey::PadA},       {"PadB", InputKey::PadB},       {"PadX", InputKey::PadX},         {"PadY", InputKey::PadY},
                {"PadUp", InputKey::PadUp},     {"PadDown", InputKey::PadDown}, {"PadLeft", InputKey::PadLeft},   {"PadRight", InputKey::PadRight},
                {"PadStart", InputKey::PadStart}, {"PadBack", InputKey::PadBack}};
            return names;
        }
    }

    Keys::Keys()
    {
        keys_ = {{"back", InputKey::Escape}, {"confirm", InputKey::Enter}, {"next", InputKey::Space}, {"hurry", InputKey::Space},
                 {"endTurn", InputKey::F}, {"skip", InputKey::F}, {"log", InputKey::L}, {"pin", InputKey::I}, {"notebook", InputKey::J},
                 {"menu1", InputKey::Num1}, {"menu2", InputKey::Num2}, {"menu3", InputKey::Num3}, {"menu4", InputKey::Num4},
                 {"menu5", InputKey::Num5}, {"menu6", InputKey::Num6}};
        pads_ = {{"back", InputKey::PadB},     {"confirm", InputKey::PadA}, {"next", InputKey::PadA},         {"hurry", InputKey::PadA},
                 {"endTurn", InputKey::PadY},  {"skip", InputKey::PadX},    {"notebook", InputKey::PadBack}, {"pause", InputKey::PadStart}};
    }

    bool Keys::Load(const nlohmann::json &json, const std::string &file, std::vector<core::Diagnostic> &d)
    {
        const std::size_t before = d.size();
        if (!json.is_object() || json.value("format", "") != "umbra.lc-keys" || json.value("version", 0) != 1)
            return d.push_back({core::Severity::Error, "keys", file, "format", "expected format umbra.lc-keys, version 1"}), false;
        const nlohmann::json rows = json.value("keys", nlohmann::json::object()); // kept: items() must not outlive it
        for (const auto &[meaning, name] : rows.items())
        {
            const auto known = keys_.find(meaning);
            const auto key = std::ranges::find_if(Names(), [&](const auto &n) { return name.is_string() && n.first == name.get<std::string>(); });
            if (known == keys_.end())
                d.push_back({core::Severity::Error, "keys", file, "keys." + meaning, "not a meaning the game knows (back, confirm, next, hurry, endTurn, skip, log, pin, notebook, menu1-menu6)"});
            else if (key == Names().end())
                d.push_back({core::Severity::Error, "keys", file, "keys." + meaning, "not a key name (Esc, Space, Enter, letters, 1-6, arrows, F1, F5, F9)"});
            else
                known->second = key->second;
        }
        // The pad buttons ("pad": meaning -> PadA..PadBack), checked the same way.
        const nlohmann::json pads = json.value("pad", nlohmann::json::object());
        for (const auto &[meaning, name] : pads.items())
        {
            const auto key = std::ranges::find_if(PadNames(), [&](const auto &n) { return name.is_string() && n.first == name.get<std::string>(); });
            if (!keys_.contains(meaning) && meaning != "pause")
                d.push_back({core::Severity::Error, "keys", file, "pad." + meaning, "not a meaning the game knows"});
            else if (key == PadNames().end())
                d.push_back({core::Severity::Error, "keys", file, "pad." + meaning, "not a pad button (PadA, PadB, PadX, PadY, PadUp, PadDown, PadLeft, PadRight, PadStart, PadBack)"});
            else
                pads_[meaning] = key->second;
        }
        return d.size() == before;
    }

    platform::InputKey Keys::PadOf(const std::string &meaning) const
    {
        const auto it = pads_.find(meaning);
        return it == pads_.end() ? InputKey::Unknown : it->second;
    }

    std::string Keys::PadName(const std::string &meaning) const
    {
        const InputKey key = PadOf(meaning);
        const auto it = std::ranges::find_if(PadNames(), [&](const auto &n) { return n.second == key; });
        return it == PadNames().end() ? std::string() : it->first.substr(3);
    }

    platform::InputKey Keys::Of(const std::string &meaning) const
    {
        const auto it = keys_.find(meaning);
        return it == keys_.end() ? InputKey::Unknown : it->second;
    }

    std::string Keys::Name(const std::string &meaning) const
    {
        const InputKey key = Of(meaning);
        const auto it = std::ranges::find_if(Names(), [&](const auto &n) { return n.second == key; });
        return it == Names().end() ? std::string() : it->first;
    }
}
