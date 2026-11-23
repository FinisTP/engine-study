#include "Analog.hpp"

#include <Platform.hpp>

#include <algorithm>
#include <fstream>

namespace umbra::night
{
    void AnalogScreen::Apply(core::PostSettings &post) const
    {
        post.crt = crtOn ? crt : 0.0F;
        post.vhs = vhsOn ? vhs : 0.0F;
    }

    bool AnalogScreen::Toggle(const std::string &action, const std::filesystem::path &file)
    {
        if (action == "crt")
            crtOn = !crtOn;
        else if (action == "vhs")
            vhsOn = !vhsOn;
        else
            return false;
        // Saved whole at once: a temp file, then moved over the old one.
        if (!file.empty())
        {
            std::error_code error;
            std::filesystem::create_directories(file.parent_path(), error);
            const std::filesystem::path temp = file.string() + ".tmp";
            std::ofstream(temp) << nlohmann::json{{"format", "umbra.lc-screen"}, {"version", 1}, {"crt", crtOn}, {"vhs", vhsOn}}.dump(2) << "\n";
            std::filesystem::rename(temp, file, error);
        }
        return true;
    }

    nlohmann::json AnalogScreen::Data() const { return {{"crt", crtOn}, {"crtOff", !crtOn}, {"vhs", vhsOn}, {"vhsOff", !vhsOn}}; }

    std::filesystem::path AnalogPath()
    {
        const std::filesystem::path folder = platform::Platform::PreferencesFolder();
        return folder.empty() ? std::filesystem::path{} : folder / "last-curtain" / "screen.json";
    }

    AnalogScreen LoadAnalog(const std::filesystem::path &file, const GameOptions &options)
    {
        AnalogScreen analog;
        analog.crt = std::clamp(options.crt, 0.0F, 1.0F);
        analog.vhs = std::clamp(options.vhs, 0.0F, 1.0F);
        std::ifstream in(file);
        const nlohmann::json saved = nlohmann::json::parse(in, nullptr, false);
        if (saved.is_object() && saved.value("format", "") == "umbra.lc-screen" && saved.value("version", 0) == 1)
        {
            analog.crtOn = saved.value("crt", true);
            analog.vhsOn = saved.value("vhs", true);
        }
        return analog;
    }
}
