#pragma once

#include <Csv.hpp>
#include <Match.hpp>
#include <MatchDef.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

// The shipped match data, loaded once per call.
inline umbra::match::MatchDef Data()
{
    std::vector<umbra::core::Diagnostic> diagnostics;
    umbra::match::MatchDef def;
    REQUIRE(umbra::match::LoadMatch(UMBRA_MATCH_DATA, def, diagnostics));
    return def;
}

// A copy of the shipped data in a temp folder, for tests that break one file.
inline std::filesystem::path DataCopy(const char *name)
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / name;
    fs::remove_all(dir);
    fs::copy(UMBRA_MATCH_DATA, dir, fs::copy_options::recursive);
    return dir;
}

// Rewrites one CSV cell (row 0 = the first data row), quoting every cell.
inline void SetCell(const std::filesystem::path &file, int row, const char *column, const std::string &value)
{
    umbra::night::CsvTable t;
    std::string error;
    REQUIRE(umbra::night::ReadCsv(file, t, error));
    t.rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(t.Column(column))] = value;
    std::ofstream out(file, std::ios::trunc);
    auto line = [&](const std::vector<std::string> &cells) {
        for (std::size_t i = 0; i < cells.size(); ++i)
            out << (i ? "," : "") << '"' << cells[i] << '"';
        out << "\n";
    };
    line(t.header);
    for (const auto &r : t.rows)
        line(r);
}

// Drops one CSV row (row 0 = the first data row).
inline void DropRow(const std::filesystem::path &file, int row)
{
    umbra::night::CsvTable t;
    std::string error;
    REQUIRE(umbra::night::ReadCsv(file, t, error));
    t.rows.erase(t.rows.begin() + row);
    std::ofstream out(file, std::ios::trunc);
    auto line = [&](const std::vector<std::string> &cells) {
        for (std::size_t i = 0; i < cells.size(); ++i)
            out << (i ? "," : "") << '"' << cells[i] << '"';
        out << "\n";
    };
    line(t.header);
    for (const auto &r : t.rows)
        line(r);
}

// A small campaign fight on the faction board (campaign plan 2): the player (All-Rounder, side 0,
// flag::Player), an ally (Supporter, side 0), a boss (Attacker, side 1, flag::Boss) and a strung
// recruit (Speedster, side 1); units 0-3 in that order, on hall squares near the middle, then begun.
inline umbra::match::MatchState Fight(const umbra::match::MatchDef &def, std::uint64_t seed = 5)
{
    using namespace umbra::match;
    MatchState s;
    s.board = BuildBoard(def, seed);
    s.mode = def.modes.at("campaign");
    s.stage = static_cast<std::int16_t>((s.board.height / 2) * s.board.width + s.board.width / 2);
    const auto row = [&](Role r) { return static_cast<int>(std::ranges::find(def.characters, r, &CharacterDef::role) - def.characters.begin()); };
    const std::pair<Role, int> who[] = {{Role::AllRounder, 0}, {Role::Supporter, 0}, {Role::Attacker, 1}, {Role::Speedster, 1}};
    const std::uint8_t flags[] = {flag::Player, 0, flag::Boss, static_cast<std::uint8_t>(flag::Recruit | flag::Strung)};
    std::vector<int> halls;
    for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
        if (s.board.cells[static_cast<std::size_t>(c)].kind == CellKind::Hall && c != s.stage)
            halls.push_back(c);
    std::ranges::sort(halls, {}, [&](int c) { return Steps(s.board, s.stage, c); });
    for (int i = 0; i < 4; ++i)
    {
        Unit u = FromCharacter(def, row(who[i].first), who[i].second, -1);
        u.cell = static_cast<std::int16_t>(halls[static_cast<std::size_t>(i) * 3]);
        u.flags = flags[i];
        s.units.push_back(u);
    }
    BeginMatch(def, s, seed);
    return s;
}

// Puts a unit on a square (tests arrange fights by hand).
inline void Put(umbra::match::MatchState &s, int unit, int cell) { s.units[static_cast<std::size_t>(unit)].cell = static_cast<std::int16_t>(cell); }
