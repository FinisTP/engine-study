#include "TalkMind.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace umbra::talk
{
    namespace
    {
        using nlohmann::json;
        namespace fs = std::filesystem;

        struct Reader
        {
            std::vector<core::Diagnostic> &diagnostics;
            bool ok = true;

            void Fail(const fs::path &file, const std::string &field, const std::string &message)
            {
                diagnostics.push_back({core::Severity::Error, "talk.data", file.generic_string(), field, message});
                ok = false;
            }

            // A versioned JSON file of the given format, or null after a diagnostic.
            json File(const fs::path &file, const char *format)
            {
                std::ifstream in(file);
                json value = json::parse(in, nullptr, false);
                if (!in || !value.is_object())
                    Fail(file, "", "missing or not a JSON object");
                else if (value.value("format", "") != format)
                    Fail(file, "format", std::string("must be \"") + format + "\"");
                else if (value.value("version", 0) != 1)
                    Fail(file, "version", "must be 1 (the only version this build reads)");
                else
                    return value;
                return nullptr;
            }

            std::string Text(const json &object, const fs::path &file, const std::string &at, const char *key)
            {
                if (!object.contains(key) || !object[key].is_string() || object[key].get<std::string>().empty())
                {
                    Fail(file, at + key, "a non-empty text is needed");
                    return "";
                }
                return object[key].get<std::string>();
            }

            std::vector<std::string> Texts(const json &object, const fs::path &file, const std::string &at, const char *key)
            {
                std::vector<std::string> out;
                if (!object.contains(key))
                    return out;
                if (!object[key].is_array())
                    Fail(file, at + key, "a list of texts is needed");
                else
                    for (const json &item : object[key])
                        if (item.is_string())
                            out.push_back(item.get<std::string>());
                        else
                            Fail(file, at + key, "every entry must be a text");
                return out;
            }
        };
    }

    const Voice *TestSet::Find(const std::string &id) const
    {
        const auto it = std::find_if(voices.begin(), voices.end(), [&](const Voice &v) { return v.id == id; });
        return it == voices.end() ? nullptr : &*it;
    }

    bool LoadTestSet(const fs::path &campaign, TestSet &set, std::vector<core::Diagnostic> &diagnostics)
    {
        Reader r{diagnostics};
        set = {};
        // 1. The rules: numbers, word lists, dialect markers.
        const fs::path rulesFile = campaign / "model-test/rules.json";
        if (const json rules = r.File(rulesFile, "umbra.lc-model-rules"); !rules.is_null())
        {
            TalkRules &t = set.rules;
            t.minLength = rules.value("minLength", t.minLength);
            t.maxLength = rules.value("maxLength", t.maxLength);
            t.markedShare = rules.value("markedShare", t.markedShare);
            t.passShare = rules.value("passShare", t.passShare);
            t.maxMs = rules.value("maxMs", t.maxMs);
            t.maxVideoMb = rules.value("maxVideoMb", t.maxVideoMb);
            t.seeds = rules.value("seeds", t.seeds);
            t.maxTokens = rules.value("maxTokens", t.maxTokens);
            t.never = r.Texts(rules, rulesFile, "", "never");
            t.english = r.Texts(rules, rulesFile, "", "english");
            t.narration = r.Texts(rules, rulesFile, "", "narration");
            if (rules.contains("dialects") && rules["dialects"].is_object())
                for (const auto &[name, words] : rules["dialects"].items())
                    t.dialects[name] = r.Texts(rules["dialects"], rulesFile, "dialects.", name.c_str());
            if (t.dialects.empty())
                r.Fail(rulesFile, "dialects", "list each dialect's marker words, e.g. \"central\": [...]");
        }
        // 2. The voices, one file each, in name order.
        std::vector<fs::path> files;
        if (fs::is_directory(campaign / "voices"))
            for (const auto &entry : fs::directory_iterator(campaign / "voices"))
                if (entry.path().extension() == ".json")
                    files.push_back(entry.path());
        std::sort(files.begin(), files.end());
        if (files.empty())
            r.Fail(campaign / "voices", "", "no voice sheets (voices/<id>.json)");
        for (const fs::path &file : files)
        {
            const json v = r.File(file, "umbra.lc-voice");
            if (v.is_null())
                continue;
            Voice voice{r.Text(v, file, "", "id"), r.Text(v, file, "", "name"), r.Text(v, file, "", "dialect"), r.Text(v, file, "", "sheet"),
                        r.Texts(v, file, "", "markers"), r.Texts(v, file, "", "never"), r.Texts(v, file, "", "tts"), {}};
            if (!voice.dialect.empty() && !set.rules.dialects.empty() && set.rules.dialects.count(voice.dialect) == 0)
                r.Fail(file, "dialect", "\"" + voice.dialect + "\" is not a dialect in model-test/rules.json \"dialects\"");
            for (const json &f : v.value("fragments", json::array()))
                voice.fragments.push_back({r.Text(f, file, "fragments.", "id"), r.Text(f, file, "fragments.", "text")});
            set.voices.push_back(std::move(voice));
        }
        // 3. The situations.
        const fs::path situationsFile = campaign / "model-test/situations.json";
        if (const json s = r.File(situationsFile, "umbra.lc-model-test"); !s.is_null())
        {
            int i = 0;
            for (const json &item : s.value("situations", json::array()))
            {
                const std::string at = "situations[" + std::to_string(i++) + "].";
                Situation situation{r.Text(item, situationsFile, at, "id"), r.Text(item, situationsFile, at, "speaker"),
                                    r.Text(item, situationsFile, at, "task"), r.Text(item, situationsFile, at, "authored"),
                                    r.Texts(item, situationsFile, at, "facts"), r.Texts(item, situationsFile, at, "must"),
                                    r.Texts(item, situationsFile, at, "forbid")};
                if (!situation.speaker.empty() && set.Find(situation.speaker) == nullptr)
                {
                    std::string known;
                    for (const Voice &v : set.voices)
                        known += (known.empty() ? "" : ", ") + v.id;
                    r.Fail(situationsFile, at + "speaker", "\"" + situation.speaker + "\" has no voice sheet; known: " + known);
                }
                set.situations.push_back(std::move(situation));
            }
            if (set.situations.empty())
                r.Fail(situationsFile, "situations", "the test set needs at least one situation");
        }
        // 4. The prompt template.
        const fs::path promptFile = campaign / "model-test/prompt.txt";
        std::ifstream in(promptFile);
        std::stringstream text;
        text << in.rdbuf();
        set.prompt = text.str();
        if (set.prompt.find("{authored}") == std::string::npos)
            r.Fail(promptFile, "", "the template must contain {authored} (the line to reword)");
        return r.ok;
    }
}
