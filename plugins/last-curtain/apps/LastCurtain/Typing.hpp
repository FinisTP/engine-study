#pragma once

#include <filesystem>
#include <string>
#include <vector>

// The on-screen keyboard's letters (P17, Typing.md): Vietnamese vowels with their tone marks from
// assets/typing.json, and where a tone mark goes in the word being typed. Text is UTF-8.
class Typing final
{
public:
    static Typing Load(const std::filesystem::path &file);
    [[nodiscard]] bool Ok() const { return !rows_.empty(); }
    // Vowel row r (as typing.json lists them) with tone 0 none, 1 sac, 2 huyen, 3 hoi, 4 nga, 5 nang.
    [[nodiscard]] std::string Letter(int row, int tone) const;
    // The last word with `tone` on the vowel that carries it (any old tone replaced); unchanged without a vowel.
    [[nodiscard]] std::string AddTone(const std::string &text, int tone) const;
    // The capital of a letter: a vowel's row below (the file lists capitals after the small ones), "upper", or ASCII.
    [[nodiscard]] std::string Upper(const std::string &letter) const;
    [[nodiscard]] static std::string Backspace(const std::string &text); // the last letter, not the last byte
    [[nodiscard]] const std::vector<std::vector<std::string>> &Keys() const { return keys_; }

private:
    std::vector<std::vector<std::string>> rows_, keys_; // rows_: per vowel its 6 forms
    std::vector<std::pair<std::string, std::string>> upper_;
    std::string hats_;                                  // the vowels whose mark wins (UTF-8, back to back)
    [[nodiscard]] bool Find(const std::string &letter, int &row, int &tone) const;
};
