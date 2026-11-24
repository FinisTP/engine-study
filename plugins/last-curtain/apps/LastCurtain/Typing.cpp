#include "Typing.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

namespace
{
    // UTF-8 text split into letters (each one to four bytes).
    std::vector<std::string> Letters(const std::string &text)
    {
        std::vector<std::string> out;
        for (std::size_t i = 0; i < text.size();)
        {
            const auto c = static_cast<unsigned char>(text[i]);
            const std::size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            out.push_back(text.substr(i, n));
            i += n;
        }
        return out;
    }

    bool Lower(const std::string &letter, char c) { return letter.size() == 1 && (letter[0] | 0x20) == c; }
}

Typing Typing::Load(const std::filesystem::path &file)
{
    Typing t;
    std::ifstream in(file);
    const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (!j.is_object() || j.value("format", "") != "umbra.lc-typing")
        return t;
    for (const nlohmann::json &row : j.value("vowels", nlohmann::json::array()))
        if (std::vector<std::string> forms = Letters(row.get<std::string>()); forms.size() == 6)
            t.rows_.push_back(forms);
    t.hats_ = j.value("hats", "");
    const nlohmann::json upper = j.value("upper", nlohmann::json::object()); // kept: items() must not outlive it
    for (const auto &[small, big] : upper.items())
        t.upper_.emplace_back(small, big.get<std::string>());
    for (const nlohmann::json &row : j.value("keys", nlohmann::json::array()))
        t.keys_.push_back(row.get<std::vector<std::string>>());
    return t;
}

std::string Typing::Letter(int row, int tone) const
{
    return row >= 0 && row < static_cast<int>(rows_.size()) && tone >= 0 && tone < 6 ? rows_[static_cast<std::size_t>(row)][static_cast<std::size_t>(tone)] : "";
}

bool Typing::Find(const std::string &letter, int &row, int &tone) const
{
    for (std::size_t r = 0; r < rows_.size(); ++r)
        for (std::size_t k = 0; k < 6; ++k)
            if (rows_[r][k] == letter)
                return row = static_cast<int>(r), tone = static_cast<int>(k), true;
    return false;
}

std::string Typing::AddTone(const std::string &text, int tone) const
{
    // 1. The last word's letters and which of them are vowels.
    std::vector<std::string> letters = Letters(text);
    std::size_t start = letters.size();
    while (start > 0 && letters[start - 1] != " ")
        --start;
    std::vector<std::size_t> vowels;
    for (std::size_t i = start; i < letters.size(); ++i)
    {
        int r = 0, k = 0;
        // qu and gi: that u or i belongs to the consonant when a vowel follows.
        const bool consonant = i + 1 < letters.size() && i > start &&
                               ((Lower(letters[i], 'u') && Lower(letters[i - 1], 'q')) || (Lower(letters[i], 'i') && Lower(letters[i - 1], 'g')));
        if (!consonant && Find(letters[i], r, k))
            vowels.push_back(i);
    }
    if (vowels.empty())
        return text;
    // 2. Which vowel: the last with a hat or horn; else the last when a consonant closes the word; else
    //    the next to last of two or more.
    std::size_t at = vowels.back();
    bool hat = false;
    for (const std::size_t v : vowels)
    {
        int r = 0, k = 0;
        (void)Find(letters[v], r, k);
        if (hats_.find(rows_[static_cast<std::size_t>(r)][0]) != std::string::npos)
            at = v, hat = true;
    }
    if (!hat && vowels.size() >= 2 && vowels.back() + 1 == letters.size())
        at = vowels[vowels.size() - 2];
    // 3. Every old tone off the word, the new one on.
    for (const std::size_t v : vowels)
    {
        int r = 0, k = 0;
        (void)Find(letters[v], r, k);
        letters[v] = Letter(r, v == at ? tone : 0);
    }
    std::string out;
    for (const std::string &l : letters)
        out += l;
    return out;
}

std::string Typing::Upper(const std::string &letter) const
{
    int r = 0, k = 0;
    if (Find(letter, r, k) && r < static_cast<int>(rows_.size()) / 2)
        return Letter(r + static_cast<int>(rows_.size()) / 2, k);
    for (const auto &[small, big] : upper_)
        if (small == letter)
            return big;
    return letter.size() == 1 && letter[0] >= 'a' && letter[0] <= 'z' ? std::string(1, static_cast<char>(letter[0] - 32)) : letter;
}

std::string Typing::Backspace(const std::string &text)
{
    std::vector<std::string> letters = Letters(text);
    if (!letters.empty())
        letters.pop_back();
    std::string out;
    for (const std::string &l : letters)
        out += l;
    return out;
}
