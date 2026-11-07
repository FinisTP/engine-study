#pragma once

// Internal to Umbra::Rpg: typed, bounds-checked reads from the content JSON with diagnostics.

#include <Diagnostic.hpp>
#include <RpgContent.hpp>

#include <nlohmann/json.hpp>

#include <initializer_list>
#include <set>
#include <string>
#include <vector>

namespace umbra::rpg::detail
{
    using nlohmann::json;

    constexpr int maxValue = 9999; // stats, costs and powers stay small and readable
    constexpr const char *statNames[] = {"maxHealth", "maxEnergy", "attack", "defense", "speed"};

    // Collects diagnostics for one file and reads typed fields with bounds.
    struct Reader final
    {
        std::string file;
        std::vector<core::Diagnostic> &out;

        void Error(const std::string &code, const std::string &field, const std::string &message)
        {
            out.push_back({core::Severity::Error, code, file, field, message});
        }
        void Warning(const std::string &code, const std::string &field, const std::string &message)
        {
            out.push_back({core::Severity::Warning, code, file, field, message});
        }

        // Warns about keys this format doesn't know (usually typos).
        void Known(const json &object, const std::string &where, std::initializer_list<const char *> keys)
        {
            const std::set<std::string> known(keys.begin(), keys.end());
            for (const auto &[key, unused] : object.items())
            {
                if (!known.contains(key))
                    Warning("field.unknown", where + "." + key, "unknown field is ignored (typo?)");
            }
        }

        int Int(const json &object, const char *key, int fallback, int min, int max, const std::string &where)
        {
            const auto it = object.find(key);
            if (it == object.end())
                return fallback;
            if (!it->is_number_integer() || it->get<long long>() < min || it->get<long long>() > max)
            {
                Error("field.range", where + "." + key, "expected a whole number from " + std::to_string(min) + " to " + std::to_string(max));
                return fallback;
            }
            return it->get<int>();
        }

        float Float(const json &object, const char *key, float fallback, float min, float max, const std::string &where)
        {
            const auto it = object.find(key);
            if (it == object.end())
                return fallback;
            if (!it->is_number() || it->get<double>() < min || it->get<double>() > max)
            {
                Error("field.range", where + "." + key, "expected a number from " + std::to_string(min) + " to " + std::to_string(max));
                return fallback;
            }
            return it->get<float>();
        }

        std::string String(const json &object, const char *key, const std::string &where)
        {
            const auto it = object.find(key);
            if (it == object.end())
                return {};
            if (!it->is_string())
            {
                Error("field.type", where + "." + key, "expected a string");
                return {};
            }
            return it->get<std::string>();
        }

        bool Bool(const json &object, const char *key, const std::string &where)
        {
            const auto it = object.find(key);
            if (it == object.end())
                return false;
            if (!it->is_boolean())
            {
                Error("field.type", where + "." + key, "expected true or false");
                return false;
            }
            return it->get<bool>();
        }

        std::vector<std::string> Strings(const json &object, const char *key, const std::string &where)
        {
            std::vector<std::string> values;
            const auto it = object.find(key);
            if (it == object.end())
                return values;
            if (!it->is_array())
            {
                Error("field.type", where + "." + key, "expected a list of strings");
                return values;
            }
            for (std::size_t i = 0; i < it->size(); ++i)
            {
                if ((*it)[i].is_string())
                    values.push_back((*it)[i].get<std::string>());
                else
                    Error("field.type", where + "." + key + "[" + std::to_string(i) + "]", "expected a string");
            }
            return values;
        }

        // Calls `each(object, where)` for every object in the list `key`.
        template <typename Each>
        void Objects(const json &object, const char *key, const std::string &where, Each each)
        {
            const auto it = object.find(key);
            if (it == object.end())
                return;
            if (!it->is_array())
            {
                Error("field.type", where + key, "expected a list");
                return;
            }
            for (std::size_t i = 0; i < it->size(); ++i)
            {
                const std::string at = where + key + "[" + std::to_string(i) + "]";
                if ((*it)[i].is_object())
                    each((*it)[i], at);
                else
                    Error("field.type", at, "expected an object");
            }
        }
    };


    // Story sections (StoryContent.cpp).
    void ReadStorySections(Reader &r, const json &root, Content &content);
    void CheckStoryReferences(Reader &r, const Content &content);
}
