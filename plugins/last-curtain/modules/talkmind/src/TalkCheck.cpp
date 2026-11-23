#include "TalkMind.hpp"

#include <algorithm>

namespace umbra::talk
{
    namespace
    {
        constexpr char32_t bad = 0xFFFFFFFF;

        // Next code point of UTF-8 text, or `bad` for a broken sequence; advances `at`.
        char32_t Next(const std::string &text, std::size_t &at)
        {
            const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
            const unsigned char first = byte(at);
            const int length = first < 0x80 ? 1 : (first >> 5) == 6 ? 2 : (first >> 4) == 14 ? 3 : (first >> 3) == 30 ? 4 : 0;
            if (length == 0 || at + static_cast<std::size_t>(length) > text.size())
            {
                ++at;
                return bad;
            }
            char32_t c = length == 1 ? first : first & (0x7F >> length);
            for (int i = 1; i < length; ++i)
            {
                if ((byte(at + static_cast<std::size_t>(i)) & 0xC0) != 0x80)
                {
                    ++at;
                    return bad;
                }
                c = (c << 6) | (byte(at + static_cast<std::size_t>(i)) & 0x3F);
            }
            at += static_cast<std::size_t>(length);
            return c;
        }

        void Append(std::string &out, char32_t c)
        {
            if (c < 0x80)
                out += static_cast<char>(c);
            else if (c < 0x800)
            {
                out += static_cast<char>(0xC0 | (c >> 6));
                out += static_cast<char>(0x80 | (c & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xE0 | (c >> 12));
                out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (c & 0x3F));
            }
        }

        char32_t LowerLetter(char32_t c)
        {
            if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7))
                return c + 0x20;
            // Pairs where upper case is the even (or listed) code point and lower the next one.
            if (c == 0x102 || c == 0x110 || c == 0x128 || c == 0x168 || c == 0x1A0 || c == 0x1AF)
                return c + 1;
            if (c >= 0x1EA0 && c <= 0x1EF9 && c % 2 == 0)
                return c + 1;
            return c;
        }

        bool IsLetter(char32_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c != 0xD7 && c != 0xF7 && c != bad); }

        // The text without its {tokens}.
        std::string WithoutTokens(const std::string &text)
        {
            std::string out;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const std::size_t close = text[i] == '{' ? text.find('}', i) : std::string::npos;
                if (close != std::string::npos)
                {
                    out += ' ';
                    i = close;
                }
                else
                    out += text[i];
            }
            return out;
        }

        // True if `phrase` (one or more words) appears as whole words in `words`; {tokens} as raw text.
        bool Has(const std::vector<std::string> &words, const std::string &line, const std::string &phrase)
        {
            if (!phrase.empty() && phrase[0] == '{')
                return line.find(phrase) != std::string::npos;
            const std::vector<std::string> want = Words(phrase);
            if (want.empty())
                return false;
            return std::search(words.begin(), words.end(), want.begin(), want.end()) != words.end();
        }

        bool AnyOf(const std::vector<std::string> &words, const std::string &line, const std::vector<std::string> &phrases)
        {
            return std::any_of(phrases.begin(), phrases.end(), [&](const std::string &p) { return Has(words, line, p); });
        }

        // 1. Broken UTF-8, control characters (new lines, tabs), separate tone marks, long numbers or markup.
        bool BadText(const std::string &line)
        {
            int digits = 0;
            for (std::size_t at = 0; at < line.size();)
            {
                const char32_t c = Next(line, at);
                digits = (c >= '0' && c <= '9') ? digits + 1 : 0;
                if (c == bad || c < 0x20 || c == 0x7F || (c >= 0x300 && c <= 0x36F) || digits > 4 ||
                    c == '<' || c == '>' || c == '*' || c == '#' || c == '_')
                    return true;
            }
            return false;
        }

        int Length(const std::string &line)
        {
            int n = 0;
            for (std::size_t at = 0; at < line.size(); ++n)
                Next(line, at);
            return n;
        }

        bool Marked(const std::string &word)
        {
            return std::any_of(word.begin(), word.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; });
        }

        bool English(const TalkRules &rules, const std::vector<std::string> &words)
        {
            for (const std::string &w : words)
            {
                if (std::find(rules.english.begin(), rules.english.end(), w) != rules.english.end())
                    return true;
                // Vietnamese writes no f, j, w or z: an ASCII word with one is a foreign word.
                if (!Marked(w) && w.find_first_of("fjwz") != std::string::npos)
                    return true;
            }
            return false;
        }

        bool BadToken(const std::string &line)
        {
            for (std::size_t open = line.find('{'); open != std::string::npos; open = line.find('{', open + 1))
            {
                const std::size_t close = line.find('}', open);
                if (close == std::string::npos)
                    return true;
                const std::string token = line.substr(open, close - open + 1);
                if (token != "{player}" && token != "{anh/ch\xe1\xbb\x8b}" && token != "{Anh/Ch\xe1\xbb\x8b}") // {anh/chi}, {Anh/Chi} with the dot below
                    return true;
            }
            return line.find('}') != std::string::npos && line.find('{') == std::string::npos;
        }
    }

    std::string Lower(const std::string &utf8)
    {
        std::string out;
        for (std::size_t at = 0; at < utf8.size();)
        {
            const std::size_t from = at;
            const char32_t c = Next(utf8, at);
            if (c == bad)
                out += utf8.substr(from, at - from);
            else
                Append(out, LowerLetter(c));
        }
        return out;
    }

    std::vector<std::string> Words(const std::string &utf8)
    {
        const std::string text = Lower(WithoutTokens(utf8));
        std::vector<std::string> words;
        std::string word;
        for (std::size_t at = 0; at < text.size();)
        {
            const char32_t c = Next(text, at);
            if (IsLetter(c))
                Append(word, c);
            else if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        if (!word.empty())
            words.push_back(word);
        return words;
    }

    std::string Tidy(std::string line)
    {
        const std::string open = "\xe2\x80\x9c", close = "\xe2\x80\x9d"; // curly double quotes
        const auto trim = [&] {
            const std::size_t first = line.find_first_not_of(" \t\r\n");
            line = first == std::string::npos ? "" : line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1);
        };
        trim();
        for (const auto &[a, b] : {std::pair<std::string, std::string>{"\"", "\""}, {"'", "'"}, {open, close}})
            if (line.size() >= a.size() + b.size() && line.compare(0, a.size(), a) == 0 && line.compare(line.size() - b.size(), b.size(), b) == 0)
            {
                line = line.substr(a.size(), line.size() - a.size() - b.size());
                trim();
            }
        return line;
    }

    std::string Check(const TestSet &set, const Situation &situation, const std::string &line)
    {
        const TalkRules &rules = set.rules;
        const Voice *voice = set.Find(situation.speaker);
        if (line.empty())
            return "empty";
        if (BadText(line))
            return "bad-text";
        const int length = Length(line);
        if (length < rules.minLength || length > rules.maxLength)
            return "length";
        const std::vector<std::string> words = Words(line);
        const auto marked = static_cast<double>(std::count_if(words.begin(), words.end(), Marked));
        if (words.empty() || marked < rules.markedShare * static_cast<double>(words.size()))
            return "no-marks";
        if (English(rules, words))
            return "english";
        if (AnyOf(words, line, rules.never) || (voice != nullptr && AnyOf(words, line, voice->never)))
            return "never";
        // Another dialect's marker the speaker doesn't use itself.
        for (const auto &[dialect, markers] : rules.dialects)
            if (voice != nullptr && dialect != voice->dialect)
                for (const std::string &m : markers)
                    if (std::find(voice->markers.begin(), voice->markers.end(), m) == voice->markers.end() && Has(words, line, m))
                        return "dialect";
        if (BadToken(line))
            return "token";
        for (const std::string &m : situation.must)
            if (!Has(words, line, m))
                return "missing-fact";
        if (AnyOf(words, line, situation.forbid))
            return "added-memory";
        return "";
    }
}
