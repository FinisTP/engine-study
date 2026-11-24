#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Every short text the game shows, in one table (assets/localization.csv: key, vi, en; the user,
// 2026-11-24 19:40). Localization.md says how keys are named and how a screen gets its words.
namespace umbra::night
{
    struct Localization
    {
        std::map<std::string, std::pair<std::string, std::string>> text; // key -> Vietnamese, English

        bool Load(const std::filesystem::path &file, std::string &error);
        // The text in a language; no English yet: the Vietnamese; no row: the key itself (easy to spot).
        [[nodiscard]] std::string Get(const std::string &key, bool english) const;
        // Every row under "prefix." as nested JSON (a screen's words); keys of numbers become lists.
        [[nodiscard]] nlohmann::json Words(const std::string &prefix, bool english) const;
        [[nodiscard]] std::vector<std::string> Missing() const; // "key (vi)" or "key (en)" for each empty cell
    };

    // The words under `prefix` from `csv`, in the language the settings chose (Vietnamese unless English);
    // an empty object if the file can't be read.
    [[nodiscard]] nlohmann::json LoadWords(const std::filesystem::path &csv, const std::string &prefix, bool english);
    [[nodiscard]] bool EnglishChosen(); // the settings' language switch (screen.json)
}
