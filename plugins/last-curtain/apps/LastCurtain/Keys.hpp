#pragma once

#include <Diagnostic.hpp>
#include <Input.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace umbra::night
{
    // assets/keys-keyboard.json: what each key means (campaign plan 5, P1). Meanings the file leaves out
    // keep their defaults; an unknown meaning or key name is refused with its row.
    class Keys final
    {
    public:
        Keys(); // the defaults
        bool Load(const nlohmann::json &json, const std::string &file, std::vector<core::Diagnostic> &d);
        [[nodiscard]] platform::InputKey Of(const std::string &meaning) const;
        [[nodiscard]] std::string Name(const std::string &meaning) const; // as shown in prompts ("F", "Esc")
        [[nodiscard]] bool Pressed(const platform::Input &input, const std::string &meaning) const { return input.IsKeyPressed(Of(meaning)); }
        [[nodiscard]] bool Held(const platform::Input &input, const std::string &meaning) const { return input.IsKeyHeld(Of(meaning)); }

    private:
        std::map<std::string, platform::InputKey> keys_;
    };
}
