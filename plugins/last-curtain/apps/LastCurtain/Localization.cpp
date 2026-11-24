#include "Localization.hpp"

#include "Analog.hpp"

#include <Csv.hpp>

#include <algorithm>
#include <cctype>

namespace umbra::night
{
    bool Localization::Load(const std::filesystem::path &file, std::string &error)
    {
        CsvTable t;
        if (!ReadCsv(file, t, error))
            return false;
        const int key = t.Column("key"), vi = t.Column("vi"), en = t.Column("en");
        if (key < 0 || vi < 0 || en < 0)
            return error = "expected the columns key, vi, en", false;
        for (const auto &row : t.rows)
            if (static_cast<int>(row.size()) > std::max({key, vi, en}) && !row[static_cast<std::size_t>(key)].empty())
                text[row[static_cast<std::size_t>(key)]] = {row[static_cast<std::size_t>(vi)], row[static_cast<std::size_t>(en)]};
        return true;
    }

    std::string Localization::Get(const std::string &key, bool english) const
    {
        const auto it = text.find(key);
        if (it == text.end())
            return key;
        return english && !it->second.second.empty() ? it->second.second : it->second.first;
    }

    nlohmann::json Localization::Words(const std::string &prefix, bool english) const
    {
        // 1. Each row under the prefix goes to its place in the tree (a.b.c -> {"a": {"b": {"c": ...}}}).
        nlohmann::json out = nlohmann::json::object();
        const std::string start = prefix + ".";
        for (auto it = text.lower_bound(start); it != text.end() && it->first.starts_with(start); ++it)
            out[nlohmann::json::json_pointer("/" + [&] {
                std::string path = it->first.substr(start.size());
                std::ranges::replace(path, '.', '/');
                return path;
            }())] = Get(it->first, english);
        // 2. An object whose keys are all numbers is a list, in number order.
        const auto lists = [](auto &self, nlohmann::json &j) -> void {
            if (!j.is_object())
                return;
            for (auto &[k, v] : j.items())
                self(self, v);
            const bool numbers = !j.empty() && std::ranges::all_of(j.items(), [](const auto &item) {
                return std::ranges::all_of(item.key(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
            });
            if (!numbers)
                return;
            nlohmann::json list = nlohmann::json::array();
            for (std::size_t i = 0; j.contains(std::to_string(i)); ++i)
                list.push_back(j[std::to_string(i)]);
            j = list;
        };
        lists(lists, out);
        return out;
    }

    std::vector<std::string> Localization::Missing() const
    {
        std::vector<std::string> out;
        for (const auto &[key, pair] : text)
        {
            if (pair.first.empty())
                out.push_back(key + " (vi)");
            if (pair.second.empty())
                out.push_back(key + " (en)");
        }
        return out;
    }

    nlohmann::json LoadWords(const std::filesystem::path &csv, const std::string &prefix, bool english)
    {
        Localization loc;
        std::string error;
        return loc.Load(csv, error) ? loc.Words(prefix, english) : nlohmann::json::object();
    }

    bool EnglishChosen() { return LoadAnalog(AnalogPath(), GameOptions{}).english; }
}
