#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace umbra::night
{
    // A CSV file: the header row and the data rows, every cell as text.
    struct CsvTable
    {
        std::vector<std::string> header;
        std::vector<std::vector<std::string>> rows;

        // The index of a column, or -1.
        int Column(std::string_view name) const;
    };

    // Reads quoted fields ("a, b" and "" inside quotes), CRLF and a trailing blank line.
    bool ReadCsv(const std::filesystem::path &file, CsvTable &out, std::string &error);
}
