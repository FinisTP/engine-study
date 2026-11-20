#include "Options.hpp"

#include <charconv>
#include <fstream>
#include <map>
#include <string>

namespace umbra::night
{
    bool LoadOptions(const std::filesystem::path &file, GameOptions &options, std::vector<core::Diagnostic> &diagnostics)
    {
        std::ifstream in(file);
        if (!in)
        {
            diagnostics.push_back({core::Severity::Error, "night.options", file.string(), "", "cannot be read"});
            return false;
        }
        // 1. Each key's place in the struct; a flag reads 0 or 1.
        const std::map<std::string, float GameOptions::*> numbers{
            {"slideSeconds", &GameOptions::slideSeconds},           {"thinkSeconds", &GameOptions::thinkSeconds},
            {"lineSeconds", &GameOptions::lineSeconds},             {"beatSeconds", &GameOptions::beatSeconds},
            {"fastBeatSeconds", &GameOptions::fastBeatSeconds},     {"unseenBeatSeconds", &GameOptions::unseenBeatSeconds}, {"diceSeconds", &GameOptions::diceSeconds},
            {"cameraDistance", &GameOptions::cameraDistance},       {"cameraMinDistance", &GameOptions::cameraMinDistance},
            {"cameraMaxDistance", &GameOptions::cameraMaxDistance}, {"cameraTilt", &GameOptions::cameraTilt},
            {"cameraEaseSeconds", &GameOptions::cameraEaseSeconds}, {"tapGap", &GameOptions::tapGap},
            {"turnBannerSeconds", &GameOptions::turnBannerSeconds}, {"plainBeatSeconds", &GameOptions::plainBeatSeconds},
            {"matchCameraDistance", &GameOptions::matchCameraDistance}, {"matchCameraTilt", &GameOptions::matchCameraTilt},
            {"blowSeconds", &GameOptions::blowSeconds},             {"orderLength", &GameOptions::orderLength}};
        const std::map<std::string, bool GameOptions::*> flags{{"tapToContinue", &GameOptions::tapToContinue}, {"moments", &GameOptions::moments}, {"light", &GameOptions::light}};
        // 2. Rows: key,value,note (the note may hold commas); the header is skipped.
        std::string line;
        std::getline(in, line);
        const std::size_t before = diagnostics.size();
        for (int row = 1; std::getline(in, line); ++row)
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;
            const std::size_t a = line.find(','), b = line.find(',', a == std::string::npos ? a : a + 1);
            const std::string key = line.substr(0, a), text = a == std::string::npos ? "" : line.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
            const std::string where = "options.csv:" + std::to_string(row) + ":";
            float value = 0.0F;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (!numbers.count(key) && !flags.count(key))
                diagnostics.push_back({core::Severity::Error, "night.options", file.string(), where + "key", "no option '" + key + "' (Options.hpp lists them)"});
            else if (error != std::errc{} || end != text.data() + text.size() || value < 0.0F)
                diagnostics.push_back({core::Severity::Error, "night.options", file.string(), where + "value", "'" + text + "' is not a number of 0 or more"});
            else if (numbers.count(key))
                options.*numbers.at(key) = value;
            else
                options.*flags.at(key) = value != 0.0F;
        }
        return diagnostics.size() == before;
    }
}
