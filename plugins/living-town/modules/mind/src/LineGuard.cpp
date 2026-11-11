#include "LineGuard.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace umbra::town
{
    namespace
    {
        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        // 1. Plain printable text: no control characters, markup or other alphabets (English for now).
        std::string CleanText(const LineContext &, const LineRules &, const std::string &line)
        {
            for (const char c : line)
                if (c < 32 || c > 126 || c == '<' || c == '>' || c == '{' || c == '}' || c == '[' || c == ']' || c == '*' || c == '#' || c == '\\')
                    return "not plain text";
            return {};
        }

        // 2. One short sentence or two.
        std::string Length(const LineContext &, const LineRules &rules, const std::string &line)
        {
            return line.size() < 8 ? "too short" : static_cast<int>(line.size()) > rules.maxLength ? "too long" : "";
        }

        // 3. The developer's word list.
        std::string Banned(const LineContext &, const LineRules &rules, const std::string &line)
        {
            const std::string lower = Lower(line);
            for (const std::string &word : rules.banned)
                if (!word.empty() && lower.find(Lower(word)) != std::string::npos)
                    return "banned: " + word;
            return {};
        }

        // Is this resident named in the line? The last word of the name is how people say it
        // ("Old Bren" -> "bren"), and it must stand as a word ("pipes" does not name Pip).
        bool Names(const TownDef &def, int resident, const std::string &lower)
        {
            const std::string &name = def.residents[static_cast<std::size_t>(resident)].name;
            const std::string called = Lower(name.substr(name.rfind(' ') == std::string::npos ? 0 : name.rfind(' ') + 1));
            for (std::size_t at = lower.find(called); at != std::string::npos; at = lower.find(called, at + 1))
            {
                const bool startsWord = at == 0 || !std::isalpha(static_cast<unsigned char>(lower[at - 1]));
                const bool endsWord = at + called.size() >= lower.size() || !std::isalpha(static_cast<unsigned char>(lower[at + called.size()]));
                if (startsWord && endsWord)
                    return true;
            }
            return false;
        }

        // 4. Stay on the fact: no other resident may be named than the people it is about and
        //    the speaker. (A model that drags someone else in is inventing.)
        std::string OnlyTheirNames(const LineContext &context, const LineRules &, const std::string &line)
        {
            const std::string lower = Lower(line);
            for (int resident = 0; resident < static_cast<int>(context.def.residents.size()); ++resident)
                if (resident != context.fact.who && resident != context.fact.other && resident != context.speaker && Names(context.def, resident, lower))
                    return "names " + context.def.residents[static_cast<std::size_t>(resident)].name + ", who is not part of this";
            return {};
        }

        // 5. And it must still be about them: everyone the fact names (other than the speaker)
        //    is named in the line. A line that lost its people has lost its fact.
        std::string KeepsTheirNames(const LineContext &context, const LineRules &, const std::string &line)
        {
            const std::string lower = Lower(line);
            for (const int resident : {static_cast<int>(context.fact.who), static_cast<int>(context.fact.other)})
                if (resident >= 0 && resident != context.speaker && !Names(context.def, resident, lower))
                    return "does not name " + context.def.residents[static_cast<std::size_t>(resident)].name;
            return {};
        }
    }

    namespace
    {
        // 6. Spoken, not shouted: a line that is mostly capitals is a model gone wrong.
        std::string NotShouting(const LineContext &, const LineRules &, const std::string &line)
        {
            int letters = 0, capitals = 0;
            for (const char c : line)
                letters += std::isalpha(static_cast<unsigned char>(c)) ? 1 : 0, capitals += std::isupper(static_cast<unsigned char>(c)) ? 1 : 0;
            return capitals * 2 > letters ? "shouting" : "";
        }
    }

    namespace
    {
        // 7. The same yes or no: a line that says "no longer", "not", "never" where the template
        //    does not (or the other way round) has most likely turned the fact upside down.
        bool Denies(const std::string &line)
        {
            const std::string lower = " " + Lower(line) + " ";
            for (const char *word : {" not ", "n't ", " no longer ", " never ", " no more ", " nobody ", " neither ", " former "})
                if (lower.find(word) != std::string::npos)
                    return true;
            return false;
        }
        std::string SameYesOrNo(const LineContext &context, const LineRules &, const std::string &line)
        {
            return !context.templated.empty() && Denies(context.templated) != Denies(line) ? "turns the meaning around" : "";
        }
    }

    LineGuard::LineGuard() : filters{CleanText, Length, Banned, OnlyTheirNames, KeepsTheirNames, NotShouting, SameYesOrNo} {}

    std::string LineGuard::Objection(const LineContext &context, const std::string &line) const
    {
        for (const LineFilter filter : filters)
            if (std::string reason = filter(context, rules, line); !reason.empty())
                return reason;
        return {};
    }

    std::string Tidy(std::string line)
    {
        const auto drop = [](char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '"'; };
        while (!line.empty() && drop(line.back()))
            line.pop_back();
        std::size_t start = 0;
        while (start < line.size() && drop(line[start]))
            ++start;
        return line.substr(start);
    }

    bool LoadLineRules(const std::filesystem::path &file, LineRules &rules, std::vector<core::Diagnostic> &diagnostics)
    {
        std::ifstream in(file, std::ios::binary);
        if (!in)
            return true; // no file: the defaults
        const nlohmann::json root = nlohmann::json::parse(in, nullptr, false);
        if (!root.is_object() || (root.contains("banned") && !root["banned"].is_array()) || (root.contains("maxLength") && !root["maxLength"].is_number_integer()))
        {
            diagnostics.push_back({core::Severity::Error, "town.lines", file.generic_string(), "", "expected {\"maxLength\": 160, \"banned\": [\"word\", ...]}"});
            return false;
        }
        rules.maxLength = std::clamp(root.value("maxLength", 160), 40, 400);
        rules.banned.clear();
        for (const nlohmann::json &word : root.value("banned", nlohmann::json::array()))
            if (word.is_string())
                rules.banned.push_back(word.get<std::string>());
        return true;
    }
}
