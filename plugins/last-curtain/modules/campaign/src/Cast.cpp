#include <Campaign.hpp>
#include <Csv.hpp>

#include <array>

namespace umbra::campaign
{
    const CastMember *Cast::Find(const std::string &id) const
    {
        for (const CastMember &m : members)
            if (m.id == id)
                return &m;
        return nullptr;
    }

    bool LoadCast(const std::filesystem::path &file, Cast &cast, std::vector<core::Diagnostic> &d)
    {
        const auto error = [&](const std::string &field, const std::string &message) {
            d.push_back({core::Severity::Error, "campaign.cast", file.string(), field, message});
        };
        // 1. The table and its columns.
        night::CsvTable table;
        std::string read;
        if (!night::ReadCsv(file, table, read))
        {
            error("", read);
            return false;
        }
        const std::array<std::string, 6> names{"id", "name", "sprite", "bound", "form", "voice"};
        std::array<int, 6> col{};
        for (std::size_t i = 0; i < names.size(); ++i)
            if ((col[i] = table.Column(names[i])) < 0)
                error("header", "a column \"" + names[i] + "\" is missing");
        if (!d.empty())
            return false;
        // 2. One member a row; the form is one of six words.
        const std::array<std::string, 6> forms{"player", "ally", "strung", "ghost", "host", "zodiac"};
        Cast out;
        const std::size_t before = d.size();
        for (std::size_t r = 0; r < table.rows.size(); ++r)
        {
            const auto &row = table.rows[r];
            const auto cell = [&](int i) { return static_cast<std::size_t>(col[i]) < row.size() ? row[col[i]] : std::string(); };
            const std::string where = "rows[" + std::to_string(r) + "].";
            CastMember m{cell(0), cell(1), cell(2), cell(3), Form::Ghost, cell(5)};
            if (m.id.empty() || out.Find(m.id))
                error(where + "id", "each row needs its own id");
            std::size_t f = 0;
            while (f < forms.size() && forms[f] != cell(4))
                ++f;
            if (f == forms.size())
                error(where + "form", "expected player, ally, strung, ghost, host or zodiac");
            else
                m.form = static_cast<Form>(f);
            out.members.push_back(m);
        }
        if (d.size() != before)
            return false;
        cast = std::move(out);
        return true;
    }
}
