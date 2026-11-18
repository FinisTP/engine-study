#include <Csv.hpp>

#include <fstream>
#include <sstream>

namespace umbra::night
{
    int CsvTable::Column(std::string_view name) const
    {
        for (std::size_t i = 0; i < header.size(); ++i)
            if (header[i] == name)
                return static_cast<int>(i);
        return -1;
    }

    bool ReadCsv(const std::filesystem::path &file, CsvTable &out, std::string &error)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
        {
            error = "cannot open the file";
            return false;
        }
        std::stringstream buffer;
        buffer << stream.rdbuf();
        const std::string text = buffer.str();

        // 1. Split into rows of cells, honouring quotes.
        std::vector<std::vector<std::string>> rows(1);
        std::string cell;
        bool quoted = false;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char c = text[i];
            if (quoted)
            {
                if (c == '"' && i + 1 < text.size() && text[i + 1] == '"')
                    cell += text[++i];
                else if (c == '"')
                    quoted = false;
                else
                    cell += c;
            }
            else if (c == '"')
                quoted = true;
            else if (c == ',')
                rows.back().push_back(std::move(cell)), cell.clear();
            else if (c == '\n')
                rows.back().push_back(std::move(cell)), cell.clear(), rows.emplace_back();
            else if (c != '\r')
                cell += c;
        }
        if (quoted)
        {
            error = "a quote is never closed";
            return false;
        }
        rows.back().push_back(std::move(cell));

        // 2. Drop blank lines; the first row is the header.
        out = {};
        for (auto &row : rows)
        {
            if (row.size() == 1 && row[0].empty())
                continue;
            if (out.header.empty())
                out.header = std::move(row);
            else
                out.rows.push_back(std::move(row));
        }
        if (out.header.empty())
        {
            error = "the file has no header row";
            return false;
        }
        return true;
    }
}
