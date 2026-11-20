#include <Career.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

// The career file (Cafe.md "Career").
namespace umbra::cafe
{
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CharacterProgress, id, level, xp, maxHp, attack, defence, speed, move, abilities, boost, cut)

    namespace
    {
        constexpr int version = 1;
    }

    CharacterProgress *Career::Find(const std::string &id)
    {
        for (CharacterProgress &c : characters)
            if (c.id == id)
                return &c;
        return nullptr;
    }

    const CharacterProgress *Career::Find(const std::string &id) const { return const_cast<Career *>(this)->Find(id); }

    bool SaveCareer(const std::filesystem::path &file, const Career &career, std::string &error)
    {
        // 1. Written whole to a temp file, then moved over the old one: a crash never leaves half a career.
        const nlohmann::json j{{"version", version}, {"money", career.money}, {"day", career.day}, {"characters", career.characters}, {"heard", career.heard}};
        std::filesystem::path temp = file;
        temp += ".tmp";
        {
            std::ofstream out(temp, std::ios::trunc);
            if (!(out << j.dump(2)))
                return error = "cannot write " + temp.string(), false;
        }
        std::error_code ec;
        std::filesystem::rename(temp, file, ec);
        return ec ? (error = ec.message(), false) : true;
    }

    bool LoadCareer(const std::filesystem::path &file, Career &career, std::string &error)
    {
        try
        {
            std::ifstream in(file);
            if (!in)
                return error = "cannot read " + file.string(), false;
            const nlohmann::json j = nlohmann::json::parse(in);
            if (j.value("version", 0) != version)
                return error = "the career's version " + std::to_string(j.value("version", 0)) + " is not " + std::to_string(version), false;
            Career c;
            c.money = j.at("money"), c.day = j.at("day");
            c.characters = j.at("characters").get<std::vector<CharacterProgress>>();
            c.heard = j.value("heard", std::vector<std::string>{});
            career = std::move(c);
            return true;
        }
        catch (const std::exception &e)
        {
            return error = e.what(), false;
        }
    }
}
