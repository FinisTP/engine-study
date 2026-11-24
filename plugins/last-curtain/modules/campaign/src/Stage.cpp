#include <Campaign.hpp>

#include <fstream>
#include <set>

namespace umbra::campaign
{
    using json = nlohmann::json;

    namespace
    {
        struct Reader
        {
            const std::filesystem::path &file;
            const Cast &cast;
            const match::MatchDef &def;
            const StageDef &stage;
            std::vector<core::Diagnostic> &d;

            void Error(const std::string &field, const std::string &message) { d.push_back({core::Severity::Error, "campaign.stage", file.string(), field, message}); }
            bool Item(const std::string &id) const { return std::ranges::any_of(def.items, [&](const match::ItemDef &i) { return i.id == id; }); }

            // A square [x, y] on the map that is not a wall.
            bool Square(const json &j, const std::string &field, int &x, int &y)
            {
                if (!j.is_array() || j.size() != 2 || !j[0].is_number_integer() || !j[1].is_number_integer())
                    return Error(field, "expected [x, y]"), false;
                x = j[0].get<int>(), y = j[1].get<int>();
                if (stage.At(x, y) == '#')
                    return Error(field, "[" + std::to_string(x) + ", " + std::to_string(y) + "] is a wall or outside the map"), false;
                return true;
            }

            StageUnit Unit(const json &j, const std::string &at)
            {
                StageUnit u;
                u.who = j.value("who", "");
                if (!cast.Find(u.who))
                    Error(at + ".who", "\"" + u.who + "\" is not in cast.csv");
                Square(j.value("at", json()), at + ".at", u.x, u.y);
                u.side = j.value("side", 1), u.level = j.value("level", 1), u.height = j.value("height", 1.0F);
                // 1. Flags.
                for (const json &f : j.value("flags", json::array()))
                {
                    const std::string flag = f.is_string() ? f.get<std::string>() : "";
                    if (flag == "boss")
                        u.boss = true;
                    else if (flag == "recruit")
                        u.recruit = true;
                    else if (flag == "strung")
                        u.strung = true;
                    else
                        Error(at + ".flags", "expected boss, recruit or strung");
                }
                // 2. Behaviour and its number.
                if (j.contains("behaviour"))
                {
                    const json &b = j["behaviour"];
                    const std::string kind = b.value("kind", "");
                    u.behaviour = kind == "aggressive" ? Behaviour::Aggressive : kind == "passive" ? Behaviour::Passive
                                : kind == "reinforcement" ? Behaviour::Reinforcement : Behaviour::None;
                    if (kind != "aggressive" && kind != "passive" && kind != "reinforcement" && kind != "none")
                        Error(at + ".behaviour.kind", "expected none, aggressive (with after), passive or reinforcement (with round)");
                    u.after = b.value("after", 0), u.round = b.value("round", 0);
                }
                // 3. Drops: items.csv ids and a percent.
                const json drops = j.value("drops", json::array());
                for (std::size_t i = 0; i < drops.size(); ++i)
                {
                    const std::string item = drops[i].value("item", "");
                    if (!Item(item))
                        Error(at + ".drops[" + std::to_string(i) + "].item", "\"" + item + "\" is not in items.csv");
                    u.drops.emplace_back(item, std::clamp(drops[i].value("percent", 0), 0, 100));
                }
                return u;
            }

            std::vector<StageUnit> Units(const json &list, const std::string &at)
            {
                std::vector<StageUnit> out;
                for (std::size_t i = 0; i < list.size(); ++i)
                    out.push_back(Unit(list[i], at + "[" + std::to_string(i) + "]"));
                return out;
            }
        };

        // A moment a scene may start at (Campaign.md "Stages").
        bool Moment(const std::string &m, const Cast &cast)
        {
            if (m == "start" || m == "end" || m == "lost")
                return true;
            const std::size_t colon = m.find(':');
            if (colon == std::string::npos)
                return false;
            const std::string what = m.substr(0, colon), arg = m.substr(colon + 1);
            if (what == "round")
                return !arg.empty() && arg.find_first_not_of("0123456789") == std::string::npos;
            return (what == "firstAttacked" || what == "falls" || what == "freed") && cast.Find(arg);
        }
    }

    bool LoadStage(const std::filesystem::path &file, const Cast &cast, const match::MatchDef &def, const std::filesystem::path &scenes,
                   StageDef &stage, std::vector<core::Diagnostic> &d)
    {
        const std::size_t before = d.size();
        StageDef out;
        Reader r{file, cast, def, out, d};
        // 1. The file and its header.
        std::ifstream in(file, std::ios::binary);
        const json j = json::parse(in, nullptr, false);
        if (!in || j.is_discarded() || !j.is_object())
            return r.Error("", "not readable JSON"), false;
        if (j.value("format", "") != "umbra.lc-stage" || j.value("version", 0) != 1)
            return r.Error("format", "expected \"format\": \"umbra.lc-stage\" and \"version\": 1"), false;
        out.id = j.value("id", ""), out.kind = j.value("kind", "story"), out.title = j.value("title", ""), out.titleEn = j.value("titleEn", "");
        out.backdrop = j.value("backdrop", ""), out.music = j.value("music", "");
        out.level = j.value("level", 1), out.partyCap = j.value("partyCap", 1), out.xp = j.value("xp", 0);
        if (out.kind != "story" && out.kind != "skirmish")
            r.Error("kind", "expected story or skirmish");
        // 2. The map: equal rows of known characters; enough party starts.
        out.map = j.value("map", std::vector<std::string>{});
        out.height = static_cast<int>(out.map.size());
        out.width = out.map.empty() ? 0 : static_cast<int>(out.map[0].size());
        if (out.map.empty())
            r.Error("map", "a stage needs a map: rows of # . ~ W R S D");
        int starts = 0;
        for (std::size_t y = 0; y < out.map.size(); ++y)
        {
            const std::string at = "map[" + std::to_string(y) + "]";
            if (static_cast<int>(out.map[y].size()) != out.width)
                r.Error(at, "every row needs " + std::to_string(out.width) + " characters");
            else if (out.map[y].find_first_not_of("#.~WRSD") != std::string::npos)
                r.Error(at, "a square must be one of # . ~ W R S D");
            starts += static_cast<int>(std::ranges::count(out.map[y], 'S'));
        }
        if (d.size() != before)
            return false;
        if (starts < out.partyCap)
            r.Error("partyCap", "the map has " + std::to_string(starts) + " party starts (S) for " + std::to_string(out.partyCap));
        // 3. Units, items, rules.
        out.units = r.Units(j.value("units", json::array()), "units");
        if (out.kind == "story" && std::ranges::none_of(out.units, &StageUnit::boss))
            r.Error("units", "a story stage needs a unit with the boss flag (it is won when the bosses fall)");
        const json items = j.value("items", json::array());
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const std::string at = "items[" + std::to_string(i) + "]";
            StageItem it{items[i].value("id", ""), items[i].value("item", "")};
            if (it.id.empty())
                r.Error(at + ".id", "an item on the map needs its own id (the save remembers it was taken)");
            if (!r.Item(it.item))
                r.Error(at + ".item", "\"" + it.item + "\" is not in items.csv");
            r.Square(items[i].value("at", json()), at + ".at", it.x, it.y);
            out.items.push_back(it);
        }
        out.mustFree = j.value("mustFree", false);
        out.noRevive = j.value("noRevive", std::vector<int>{});
        out.startHp = j.value("startHp", std::map<std::string, int>{});
        out.gimmicks = j.value("gimmicks", json::object());
        out.aimStart = j.value("aims", json::object()).value("start", "");
        out.aimRecap = j.value("aims", json::object()).value("recap", "");
        // 4. Scenes: a known moment and a scene file.
        const json sceneMap = j.value("scenes", json::object()); // kept: items() of a temporary would dangle
        for (const auto &[moment, scene] : sceneMap.items())
        {
            if (!Moment(moment, cast))
                r.Error("scenes." + moment, "expected start, end, lost, round:<n>, or firstAttacked:/falls:/freed: and a cast id");
            else if (!scene.is_string() || !std::filesystem::exists(scenes / (scene.get<std::string>() + ".json")))
                r.Error("scenes." + moment, "no scene file " + (scene.is_string() ? scene.get<std::string>() : std::string("?")) + ".json in " + scenes.string());
            else
                out.scenes[moment] = scene.get<std::string>();
        }
        // 5. Gimmicks, the stage's and each twist's (Campaign.md "Gimmicks").
        const auto gimmicks = [&](const json &g, const std::string &at) {
            if (!g.is_object())
                return r.Error(at, "expected an object of windows, hazards, breath, dark");
            int windows = 0;
            for (const std::string &row : out.map)
                windows += static_cast<int>(std::ranges::count(row, 'W'));
            for (const auto &[key, v] : g.items())
            {
                const std::string where = at + "." + key;
                if (key == "windows")
                {
                    if (windows == 0)
                        r.Error(where, "windows need W squares on the map");
                    for (const json &round : v.value("pattern", json::array()))
                        for (const json &w : round)
                            if (!w.is_number_integer() || w.get<int>() < 0 || w.get<int>() >= windows)
                                r.Error(where + ".pattern", "each entry is a window's number, 0 to " + std::to_string(windows - 1) + " (W squares in reading order)");
                    if (!cast.Find(v.value("spawn", "")))
                        r.Error(where + ".spawn", "\"" + v.value("spawn", "") + "\" is not in cast.csv");
                    for (const json &drop : v.value("drops", json::array()))
                        if (!r.Item(drop.value("item", "")))
                            r.Error(where + ".drops", "\"" + drop.value("item", "") + "\" is not in items.csv");
                }
                else if (key == "breath")
                {
                    const std::string who = v.value("unit", "");
                    if (std::ranges::none_of(out.units, [&](const StageUnit &u) { return u.who == who; }))
                        r.Error(where + ".unit", "\"" + who + "\" is not one of this stage's units");
                }
                else if (key != "hazards" && key != "dark")
                    r.Error(where, "expected windows, hazards, breath or dark");
            }
        };
        gimmicks(out.gimmicks, "gimmicks");
        // 6. Twists: each refill's units and gimmicks.
        const json twists = j.value("twists", json::array());
        for (std::size_t i = 0; i < twists.size(); ++i)
            out.twists.push_back({twists[i].value("name", ""), r.Units(twists[i].value("units", json::array()), "twists[" + std::to_string(i) + "].units"),
                                  twists[i].value("gimmicks", json::object())}),
                gimmicks(out.twists.back().gimmicks, "twists[" + std::to_string(i) + "].gimmicks");
        if (d.size() != before)
            return false;
        stage = std::move(out);
        return true;
    }
}
