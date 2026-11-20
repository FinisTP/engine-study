#include <Cafe.hpp>
#include <Csv.hpp>

#include <charconv>
#include <fstream>

// Reads assets/cafe/ (Cafe.md "Data").
namespace umbra::cafe
{
    namespace
    {
        using Diagnostics = std::vector<core::Diagnostic>;

        void Fail(Diagnostics &out, const std::string &file, const std::string &field, const std::string &message)
        {
            out.push_back({core::Severity::Error, "cafe." + file.substr(0, file.find('.')), file, field, message});
        }

        // One CSV: its rows by column name, each problem as "<file>:<row>:<column>".
        struct Sheet
        {
            Diagnostics &out;
            std::string file;
            night::CsvTable table;

            Sheet(Diagnostics &d, const std::filesystem::path &folder, const char *name, std::initializer_list<const char *> columns) : out(d), file(name)
            {
                std::string error;
                if (!night::ReadCsv(folder / name, table, error))
                {
                    Fail(out, file, file, error);
                    return;
                }
                for (const char *c : columns)
                    if (table.Column(c) < 0)
                    {
                        Fail(out, file, file + ":" + c, "the column is missing");
                        table.rows.clear();
                        return;
                    }
            }
            int Rows() const { return static_cast<int>(table.rows.size()); }
            std::string Where(int row, const char *column) const { return file + ":" + std::to_string(row + 1) + ":" + column; }
            std::string Text(int row, const char *column)
            {
                const auto &cells = table.rows[static_cast<std::size_t>(row)];
                const auto at = static_cast<std::size_t>(table.Column(column));
                if (at >= cells.size() || cells[at].empty())
                    return Fail(out, file, Where(row, column), "the cell is blank"), std::string{};
                return cells[at];
            }
            float Number(int row, const char *column, float min, float max)
            {
                const std::string cell = Text(row, column);
                float v = 0.0F;
                const auto [end, error] = std::from_chars(cell.data(), cell.data() + cell.size(), v);
                if (!cell.empty() && (error != std::errc{} || end != cell.data() + cell.size() || v < min || v > max))
                    Fail(out, file, Where(row, column), "'" + cell + "' is not a number from " + std::to_string(int(min)) + " to " + std::to_string(int(max)));
                return v;
            }
            // "a;b;c" -> the index of each id in `ids`; an unknown id is a problem.
            template <class T> std::vector<int> Ids(int row, const char *column, const std::vector<T> &ids)
            {
                std::vector<int> found;
                const std::string cell = Text(row, column);
                for (std::size_t start = 0; !cell.empty() && start <= cell.size();)
                {
                    const std::size_t end = std::min(cell.find(';', start), cell.size());
                    const std::string id = cell.substr(start, end - start);
                    int i = 0;
                    while (i < static_cast<int>(ids.size()) && ids[static_cast<std::size_t>(i)].id != id)
                        ++i;
                    if (i == static_cast<int>(ids.size()))
                        Fail(out, file, Where(row, column), "unknown id '" + id + "'");
                    else
                        found.push_back(i);
                    start = end + 1;
                }
                return found;
            }
        };
    }

    bool LoadCafe(const std::filesystem::path &folder, CafeDef &def, std::vector<core::Diagnostic> &diagnostics)
    {
        const std::size_t before = diagnostics.size();
        CafeDef d;
        // 1. Stations, then recipes made of them, then the ghosts that order them.
        Sheet stations(diagnostics, folder, "stations.csv", {"id", "name"});
        for (int r = 0; r < stations.Rows(); ++r)
            d.stations.push_back({stations.Text(r, "id"), stations.Text(r, "name")});
        Sheet recipes(diagnostics, folder, "recipes.csv", {"id", "name", "steps", "price"});
        for (int r = 0; r < recipes.Rows(); ++r)
            d.recipes.push_back({recipes.Text(r, "id"), recipes.Text(r, "name"), recipes.Ids(r, "steps", d.stations), static_cast<int>(recipes.Number(r, "price", 1, 99))});
        Sheet guests(diagnostics, folder, "guests.csv", {"id", "name", "sprite", "likes", "patience", "weight"});
        for (int r = 0; r < guests.Rows(); ++r)
            d.guests.push_back({guests.Text(r, "id"), guests.Text(r, "name"), guests.Text(r, "sprite"), guests.Ids(r, "likes", d.recipes),
                                guests.Number(r, "patience", 5, 300), static_cast<int>(guests.Number(r, "weight", 1, 20))});
        // 2. The rules and the talks.
        try
        {
            std::ifstream rules(folder / "cafe.json");
            const nlohmann::json j = nlohmann::json::parse(rules);
            Rules &u = d.rules;
            const nlohmann::json &shift = j.at("shift");
            u.shiftSeconds = shift.at("seconds"), u.firstArrival = shift.at("firstArrival"), u.arrivalMin = shift.at("arrivalMin"), u.arrivalMax = shift.at("arrivalMax");
            u.seats = j.at("seats"), u.tipMax = j.at("tipMax"), u.maxSteps = j.at("maxSteps"), u.moneyPerEssence = j.at("moneyPerEssence");
            std::ifstream talk(folder / "talk.json");
            d.talk = nlohmann::json::parse(talk);
        }
        catch (const std::exception &e)
        {
            Fail(diagnostics, "cafe.json", "cafe.json", e.what());
        }
        for (const Recipe &r : d.recipes)
            if (static_cast<int>(r.steps.size()) > d.rules.maxSteps)
                Fail(diagnostics, "recipes.csv", "recipes.csv:" + r.id + ":steps", "more steps than maxSteps");
        if (diagnostics.size() != before)
            return false;
        def = std::move(d);
        return true;
    }
}
