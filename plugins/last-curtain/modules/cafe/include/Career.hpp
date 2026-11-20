#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

// The saved career between matches (Cafe.md "Career"): money, the day, and each character as it
// left its last match. One file, versioned, written whole.
namespace umbra::cafe
{
    struct CharacterProgress
    {
        std::string id;                     // characters.csv id
        int level = 1, xp = 0;
        int maxHp = 0, attack = 0, defence = 0, speed = 0, move = 0; // 0: as the data has it
        std::vector<std::string> abilities; // ability ids, in slot order
        std::array<int, 8> boost{}, cut{};  // upgrades by slot (power, cost)
    };

    struct Career
    {
        int money = 0, day = 1;
        std::vector<CharacterProgress> characters;
        std::vector<std::string> heard; // talk lines already told ("ghost:character:n")

        [[nodiscard]] CharacterProgress *Find(const std::string &id);
        [[nodiscard]] const CharacterProgress *Find(const std::string &id) const;
    };

    bool SaveCareer(const std::filesystem::path &file, const Career &career, std::string &error); // written to a temp file, then moved
    bool LoadCareer(const std::filesystem::path &file, Career &career, std::string &error);       // untouched on failure
}
