#include <Campaign.hpp>
#include <Csv.hpp>

#include <cmath>
#include <set>

namespace umbra::campaign
{
    bool LoadGrowth(const std::filesystem::path &file, Growth &growth, std::vector<core::Diagnostic> &d)
    {
        const std::size_t before = d.size();
        const auto error = [&](const std::string &field, const std::string &message) {
            d.push_back({core::Severity::Error, "campaign.growth", file.string(), field, message});
        };
        night::CsvTable table;
        std::string read;
        if (!night::ReadCsv(file, table, read))
            return error("", read), false;
        const char *names[] = {"role", "level", "hp", "attack", "defence", "speed", "move"};
        int col[7];
        for (int i = 0; i < 7; ++i)
            if ((col[i] = table.Column(names[i])) < 0)
                error("header", std::string("a column \"") + names[i] + "\" is missing");
        if (d.size() != before)
            return false;
        // 1. One row per role and level; the five classes only, numbers above zero.
        static const std::set<std::string> roles{"attacker", "defender", "supporter", "speedster", "allrounder"};
        Growth out;
        for (std::size_t r = 0; r < table.rows.size(); ++r)
        {
            const auto &row = table.rows[r];
            const auto cell = [&](int i) { return static_cast<std::size_t>(col[i]) < row.size() ? row[static_cast<std::size_t>(col[i])] : std::string(); };
            const std::string where = "rows[" + std::to_string(r) + "].";
            if (!roles.contains(cell(0)))
                error(where + "role", "expected attacker, defender, supporter, speedster or allrounder");
            std::array<int, 5> v{};
            for (int i = 0; i < 5; ++i)
                if ((v[static_cast<std::size_t>(i)] = std::atoi(cell(i + 2).c_str())) <= 0)
                    error(where + names[i + 2], "expected a whole number above 0");
            out.lines[{cell(0), std::atoi(cell(1).c_str())}] = v;
        }
        if (d.size() != before)
            return false;
        growth = std::move(out);
        return true;
    }

    match::Unit MakeUnit(const match::MatchDef &def, const Cast &cast, const Growth &growth, const std::string &id, int level, int faction, bool enemy)
    {
        const CastMember *m = cast.Find(id);
        if (!m)
            return {};
        // 1. A match character of the same class: its row names the class for the match's screens and abilities.
        const auto role = std::ranges::find_if(def.characters, [&](const match::CharacterDef &c) {
            static const char *names[] = {"attacker", "defender", "supporter", "speedster", "allrounder"};
            return names[static_cast<int>(c.role)] == m->role;
        });
        match::Unit u = match::FromCharacter(def, static_cast<int>(role - def.characters.begin()), faction, -1);
        // 2. The class line at this level, then the character's own changes.
        const auto line = growth.lines.find({m->role, level});
        if (line != growth.lines.end())
        {
            const auto &[hp, attack, defence, speed, move] = line->second;
            u.maxHp = static_cast<std::int16_t>(hp), u.attack = static_cast<std::int16_t>(attack), u.defence = static_cast<std::int16_t>(defence);
            u.speed = static_cast<std::int16_t>(speed), u.move = static_cast<std::int16_t>(move);
        }
        u.maxHp = static_cast<std::int16_t>(u.maxHp + m->hpPlus);
        u.attack = static_cast<std::int16_t>(u.attack + m->attackPlus);
        u.move = static_cast<std::int16_t>(u.move + m->movePlus);
        // 3. Enemies scale by form: zodiacs stronger, ordinary ghosts weaker (Campaign.md "Units").
        const double scale = !enemy ? 1.0 : m->form == Form::Zodiac ? 1.33 : m->form == Form::Ghost ? 0.6 : 1.0;
        u.maxHp = static_cast<std::int16_t>(std::lround(u.maxHp * scale));
        u.attack = static_cast<std::int16_t>(std::lround(u.attack * scale));
        u.hp = u.maxHp;
        u.level = static_cast<std::int8_t>(level);
        // 4. Its abilities: the basic attack first (every character has one), then the kit, else the class's
        //    up to this level (jobs.csv).
        u.abilities.fill(-1);
        std::size_t slot = 0;
        if (const auto basic = std::ranges::find(def.abilities, std::string("basic"), &match::AbilityDef::id); basic != def.abilities.end())
            u.abilities[slot++] = static_cast<std::int8_t>(basic - def.abilities.begin());
        for (const std::string &a : m->kit)
            for (std::size_t i = 0; i < def.abilities.size(); ++i)
                if (def.abilities[i].id == a && slot < u.abilities.size())
                    u.abilities[slot++] = static_cast<std::int8_t>(i);
        if (m->kit.empty())
            for (const match::JobLevel &j : def.jobs)
                if (j.role == role->role && j.level <= level && slot < u.abilities.size())
                    u.abilities[slot++] = static_cast<std::int8_t>(j.ability);
        return u;
    }
}
