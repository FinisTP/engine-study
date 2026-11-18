#pragma once

#include <Csv.hpp>
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
