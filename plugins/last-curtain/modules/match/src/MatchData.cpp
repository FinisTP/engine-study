#include <Csv.hpp>
#include <MatchDef.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <initializer_list>

// Reads assets/match/ (Match.md "Data"). The Sheet helper is the night loader's, trimmed; it moves
// to one shared place when the night module retires (Plan 3).
namespace umbra::match
{
    namespace
    {
        using Diagnostics = std::vector<core::Diagnostic>;

        void Fail(Diagnostics &out, const std::string &file, const std::string &field, const std::string &message)
        {
            out.push_back({core::Severity::Error, "match." + file.substr(0, file.find('.')), file, field, message});
        }

        bool ParseInt(const std::string &text, int &value)
        {
            const char *end = text.data() + text.size();
            return !text.empty() && std::from_chars(text.data(), end, value).ptr == end;
        }

        // One CSV file, read with diagnostics that name "<file>:<row>:<column>".
        struct Sheet
        {
            Diagnostics &out;
            std::string file;
            night::CsvTable table;
            bool ok = true;

            Sheet(Diagnostics &d, const std::filesystem::path &folder, const char *name, std::initializer_list<const char *> columns)
                : out(d), file(name)
            {
                std::string error;
                if (!night::ReadCsv(folder / name, table, error))
                    { Bad(file, error); return; }
                for (const char *c : columns)
                    if (table.Column(c) < 0)
                        { Bad(file + ":" + c, "the column is missing"); return; }
                for (std::size_t r = 0; r < table.rows.size(); ++r)
                    if (table.rows[r].size() != table.header.size())
                        { Bad(file + ":" + std::to_string(r + 1), "the row has the wrong number of cells"); return; }
            }

            void Bad(const std::string &field, const std::string &message) { Fail(out, file, field, message); ok = false; }
            int Rows() const { return ok ? static_cast<int>(table.rows.size()) : 0; }
            std::string Where(int row, const char *column) const { return file + ":" + std::to_string(row + 1) + ":" + column; }

            std::string Text(int row, const char *column)
            {
                const std::string &cell = table.rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(table.Column(column))];
                if (cell.empty())
                    Bad(Where(row, column), "the cell is blank");
                return cell;
            }

            int Int(int row, const char *column, int min, int max)
            {
                const std::string cell = Text(row, column);
                int v = 0;
                if (!cell.empty() && (!ParseInt(cell, v) || v < min || v > max))
                    Bad(Where(row, column), "'" + cell + "' is not a whole number from " + std::to_string(min) + " to " + std::to_string(max));
                return v;
            }

            std::uint32_t Color(int row, const char *column)
            {
                const std::string cell = Text(row, column);
                if (cell.size() == 6 && std::all_of(cell.begin(), cell.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
                    return static_cast<std::uint32_t>(std::stoul(cell, nullptr, 16));
                Bad(Where(row, column), "'" + cell + "' is not a colour (RRGGBB)");
                return 0;
            }

            int Pick(int row, const char *column, std::initializer_list<const char *> words)
            {
                const std::string cell = Text(row, column);
                int i = 0;
                for (const char *w : words)
                {
                    if (cell == w)
                        return i;
                    ++i;
                }
                Bad(Where(row, column), "'" + cell + "' is not one of the allowed words");
                return 0;
            }

            template <class T> int Ref(int row, const char *column, const std::vector<T> &rows, bool allowNone = false)
            {
                return Id(row, column, Text(row, column), rows, allowNone);
            }

            template <class T> int Id(int row, const char *column, const std::string &id, const std::vector<T> &rows, bool allowNone = false)
            {
                if (allowNone && id == "none")
                    return -1;
                const int found = MatchDef::Find(rows, id);
                if (found < 0)
                    Bad(Where(row, column), "unknown id '" + id + "'");
                return found;
            }
        };

        constexpr std::initializer_list<const char *> roles = {"attacker", "defender", "supporter", "speedster", "allrounder"};

        void LoadOptions(const std::filesystem::path &folder, Options &o, Diagnostics &out)
        {
            Sheet s(out, folder, "options.csv", {"id", "value"});
            for (int r = 0; r < s.Rows(); ++r)
            {
                const std::string id = s.Text(r, "id");
                if (id == "rounds")
                    o.rounds = s.Int(r, "value", 1, 60);
                else if (id == "side")
                    o.side = s.Int(r, "value", 1, 5);
                else if (id == "autoEndTurn")
                    o.autoEndTurn = s.Int(r, "value", 0, 1) == 1;
                else if (id == "bots")
                    o.bots = static_cast<BotLevel>(s.Pick(r, "value", {"easy", "medium", "hard"}));
                else
                    s.Bad(s.Where(r, "id"), "unknown option '" + id + "'");
            }
        }

        // rules.json: every number is required.
        void LoadRules(const std::filesystem::path &folder, Rules &r, Diagnostics &out)
        {
            std::ifstream in(folder / "rules.json");
            const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_discarded())
                return Fail(out, "rules.json", "rules.json", "not valid JSON");
            auto num = [&](const char *group, const char *key, int &v) {
                const auto *g = group[0] ? (j.contains(group) ? &j[group] : nullptr) : &j;
                if (!g || !g->contains(key) || !(*g)[key].is_number_integer())
                    return Fail(out, "rules.json", std::string("rules.json:") + group + "." + key, "a whole number is required");
                v = (*g)[key].get<int>();
            };
            auto &b = r.board;
            for (auto [k, v] : {std::pair{"width", &b.width}, {"height", &b.height}, {"lanes", &b.lanes}, {"squareCapacity", &b.squareCapacity},
                                {"weakMin", &b.weakMin}, {"weakMax", &b.weakMax}, {"strongMin", &b.strongMin}, {"strongMax", &b.strongMax},
                                {"weakPerHalf", &b.weakPerHalf}, {"strongPerHalf", &b.strongPerHalf}, {"eventsPerHalf", &b.eventsPerHalf},
                                {"eventGap", &b.eventGap}, {"itemsPerHalf", &b.itemsPerHalf}})
                num("board", k, *v);
            num("respawn", "turns", r.respawn.turns);
            num("battle", "rounds", r.battle.rounds);
            num("battle", "minDamage", r.battle.minDamage);
            num("levels", "hp", r.levels.hp);
            num("levels", "killXp", r.levels.killXp);
            num("levels", "cards", r.levels.cards);
            num("levels", "upgradePower", r.levels.upgradePower);
            num("levels", "upgradeCost", r.levels.upgradeCost);
            num("spawn", "warn", r.spawn.warn);
            num("spawn", "centre", r.spawn.centre);
            num("treasure", "min", r.treasure.min);
            num("treasure", "max", r.treasure.max);
            num("treasure", "fullEssence", r.treasure.fullEssence);
            num("rooms", "respawn", r.rooms.respawn);
            if (r.treasure.min < 1 || r.treasure.max < r.treasure.min)
                Fail(out, "rules.json", "rules.json:treasure", "1 <= min <= max is required");
            num("", "carry", r.carry);
            auto &bal = r.balance;
            for (auto [k, v] : {std::pair{"sideMin", &bal.sideMin}, {"sideMax", &bal.sideMax}, {"firstMoverMax", &bal.firstMoverMax},
                                {"sourceMax", &bal.sourceMax}, {"comebackMin", &bal.comebackMin}})
                num("balance", k, *v);
            if (j.contains("levels") && j["levels"].contains("xp") && j["levels"]["xp"].is_array())
                r.levels.xp = j["levels"]["xp"].get<std::vector<int>>();
            if (r.levels.xp.empty() || !std::is_sorted(r.levels.xp.begin(), r.levels.xp.end()))
                Fail(out, "rules.json", "rules.json:levels.xp", "a rising list of XP steps is required");
            if (j.contains("bots") && j["bots"].contains("noise") && j["bots"]["noise"].size() == 3)
                for (std::size_t i = 0; i < 3; ++i)
                    r.bots.noise[i] = j["bots"]["noise"][i].get<int>();
            else
                Fail(out, "rules.json", "rules.json:bots.noise", "three numbers are required (easy, medium, hard)");
            if (r.carry < 0 || r.carry > maxItems)
                Fail(out, "rules.json", "rules.json:carry", "carry must be 0 to " + std::to_string(maxItems));
            if (b.width < 5 || b.height < 7 || b.width * b.height > 32000)
                Fail(out, "rules.json", "rules.json:board", "the board is too small or too large");
        }

        // "2d6" or "none".
        Dice ParseDice(Sheet &s, int row)
        {
            const std::string t = s.Text(row, "dice");
            if (t == "none")
                return {};
            Dice d;
            const auto at = t.find('d');
            if (at == std::string::npos || !ParseInt(t.substr(0, at), d.count) || !ParseInt(t.substr(at + 1), d.sides) || d.count < 1 || d.sides < 2)
                s.Bad(s.Where(row, "dice"), "'" + t + "' is not dice like 2d6, or none");
            return d;
        }

        // "none", "hp:-3", "essence:2", "xp:2", "item:<id>".
        Outcome ParseOutcome(Sheet &s, int row, const char *column, const MatchDef &def)
        {
            const std::string t = s.Text(row, column);
            if (t == "none")
                return {};
            const auto at = t.find(':');
            const std::string kind = t.substr(0, at), value = at == std::string::npos ? "" : t.substr(at + 1);
            Outcome o;
            if (kind == "item")
                return {Effect::Item, s.Id(row, column, value, def.items)};
            if (kind == "hp")
                o.effect = Effect::Hp;
            else if (kind == "essence")
                o.effect = Effect::Essence;
            else if (kind == "xp")
                o.effect = Effect::Xp;
            if (o.effect == Effect::None || !ParseInt(value, o.amount))
                s.Bad(s.Where(row, column), "'" + t + "' is not none, hp:n, essence:n, xp:n or item:<id>");
            return o;
        }

        void LoadAbilities(const std::filesystem::path &folder, MatchDef &def, Diagnostics &out)
        {
            Sheet s(out, folder, "abilities.csv", {"id", "name", "use", "class", "target", "hpCost", "power", "dice", "range", "animation"});
            for (int r = 0; r < s.Rows(); ++r)
            {
                AbilityDef a;
                a.id = s.Text(r, "id");
                a.name = s.Text(r, "name");
                a.animation = s.Text(r, "animation");
                a.use = static_cast<Use>(s.Pick(r, "use", {"battle", "board", "passive"}));
                a.kind = static_cast<AbilityClass>(s.Pick(r, "class", {"strike", "shield", "heal", "hit", "firststrike"}));
                a.target = static_cast<Target>(s.Pick(r, "target", {"enemy", "ally", "self", "monster", "room", "any"}));
                a.hpCost = s.Int(r, "hpCost", 0, 99);
                a.power = s.Int(r, "power", 0, 99);
                a.range = s.Int(r, "range", 0, 20);
                a.dice = ParseDice(s, r);
                // 1. Each class has its place: strike and shield in battle, hit on the board, first strike passive.
                const bool fits = a.kind == AbilityClass::Heal ? a.use != Use::Passive
                                : a.kind == AbilityClass::Hit  ? a.use == Use::Board
                                : a.kind == AbilityClass::FirstStrike ? a.use == Use::Passive
                                                                      : a.use == Use::Battle;
                if (!fits)
                    s.Bad(s.Where(r, "use"), "this class cannot be used there");
                // 2. And its targets: strikes an enemy, shields yourself, board heals your side, hits anyone not on it.
                const Target t = a.target;
                const bool aims = a.kind == AbilityClass::Hit ? t == Target::Enemy || t == Target::Monster || t == Target::Any
                                : a.kind == AbilityClass::Heal && a.use == Use::Board ? t == Target::Ally || t == Target::Self
                                : a.kind == AbilityClass::Strike ? t == Target::Enemy
                                                                 : t == Target::Self;
                if (s.ok && !aims)
                    s.Bad(s.Where(r, "target"), "this class cannot aim there");
                def.abilities.push_back(a);
            }
        }

        void LoadJobs(const std::filesystem::path &folder, MatchDef &def, Diagnostics &out)
        {
            Sheet s(out, folder, "jobs.csv", {"role", "level", "ability", "choice"});
            for (int r = 0; r < s.Rows(); ++r)
                def.jobs.push_back({static_cast<Role>(s.Pick(r, "role", roles)), s.Int(r, "level", 1, 20),
                                    s.Ref(r, "ability", def.abilities), s.Ref(r, "choice", def.abilities, true)});
            // 1. Every role starts with a battle ability that costs nothing, so a fighter at 1 HP can still act.
            for (int role = 0; role < static_cast<int>(roles.size()); ++role)
            {
                const bool free = std::any_of(def.jobs.begin(), def.jobs.end(), [&](const JobLevel &j) {
                    if (static_cast<int>(j.role) != role || j.level != 1 || j.ability < 0)
                        return false;
                    const AbilityDef &a = def.abilities[static_cast<std::size_t>(j.ability)];
                    return a.use == Use::Battle && a.kind == AbilityClass::Strike && a.hpCost == 0;
                });
                if (s.ok && !free)
                    s.Bad("jobs.csv", std::string("the role ") + roles.begin()[role] + " has no free level-1 Strike");
            }
        }

        void LoadCharacters(const std::filesystem::path &folder, MatchDef &def, Diagnostics &out)
        {
            Sheet s(out, folder, "characters.csv", {"id", "name", "role", "hp", "attack", "defence", "speed", "move", "sprite", "tint"});
            for (int r = 0; r < s.Rows(); ++r)
            {
                CharacterDef c{s.Text(r, "id"), s.Text(r, "name"), s.Text(r, "sprite")};
                c.role = static_cast<Role>(s.Pick(r, "role", roles));
                c.hp = s.Int(r, "hp", 1, 999);
                c.attack = s.Int(r, "attack", 0, 99);
                c.defence = s.Int(r, "defence", 0, 99);
                c.speed = s.Int(r, "speed", 0, 99);
                c.move = s.Int(r, "move", 1, 20);
                c.tint = s.Color(r, "tint");
                def.characters.push_back(c);
            }
        }

        void LoadMonsters(const std::filesystem::path &folder, MatchDef &def, Diagnostics &out)
        {
            Sheet s(out, folder, "monsters.csv", {"id", "name", "tier", "behaviour", "hp", "attack", "defence", "speed", "essence", "xp", "level", "size", "sight", "abilities", "sprite", "tint"});
            for (int r = 0; r < s.Rows(); ++r)
            {
                MonsterDef m;
                m.id = s.Text(r, "id");
                m.name = s.Text(r, "name");
                m.sprite = s.Text(r, "sprite");
                m.tier = static_cast<Tier>(s.Pick(r, "tier", {"weak", "strong", "great", "guardian"}));
                m.behaviour = static_cast<Behaviour>(s.Pick(r, "behaviour", {"stay", "flee", "stage", "guard", "hunt"}));
                m.hp = s.Int(r, "hp", 1, 999);
                m.attack = s.Int(r, "attack", 0, 99);
                m.defence = s.Int(r, "defence", 0, 99);
                m.speed = s.Int(r, "speed", 0, 99);
                m.essence = s.Int(r, "essence", 0, 999);
                m.xp = s.Int(r, "xp", 0, 999);
                m.level = s.Int(r, "level", 1, 20);
                m.size = s.Int(r, "size", 1, 3);
                m.sight = s.Int(r, "sight", 0, 20);
                if (s.ok && m.behaviour == Behaviour::Hunt && m.sight < 1)
                    s.Bad(s.Where(r, "sight"), "a hunter needs a sight of 1 or more");
                m.tint = s.Color(r, "tint");
                // 1. Its battle abilities, separated by ';'; the first is what it strikes with.
                std::string list = s.Text(r, "abilities");
                for (std::size_t at = 0; at <= list.size();)
                {
                    const std::size_t end = std::min(list.find(';', at), list.size());
                    m.abilities.push_back(s.Id(r, "abilities", list.substr(at, end - at), def.abilities));
                    at = end + 1;
                }
                def.monsters.push_back(m);
            }
        }

        void LoadSpawns(const std::filesystem::path &folder, MatchDef &def, Diagnostics &out)
        {
            Sheet s(out, folder, "spawns.csv", {"zone", "from", "monster", "every"});
            for (int r = 0; r < s.Rows(); ++r)
            {
                SpawnRow w{s.Pick(r, "zone", {"weak", "strong", "centre"}), s.Int(r, "from", 1, 999), s.Ref(r, "monster", def.monsters),
                           s.Int(r, "every", 0, 999)}; // 0: once, never again
                // 1. Guardians belong to a hub and never spawn.
                if (w.monster >= 0 && def.monsters[static_cast<std::size_t>(w.monster)].tier == Tier::Guardian)
                    s.Bad(s.Where(r, "monster"), "a guardian cannot spawn");
                def.spawns.push_back(w);
            }
            std::ranges::stable_sort(def.spawns, {}, [](const SpawnRow &w) { return std::pair{w.zone, w.from}; });
        }

        void LoadItems(const std::filesystem::path &folder, MatchDef &def, Diagnostics &out)
        {
            Sheet s(out, folder, "items.csv", {"id", "name", "text", "sprite", "class", "amount"});
            for (int r = 0; r < s.Rows(); ++r)
            {
                ItemDef i{s.Text(r, "id"), s.Text(r, "name"), s.Text(r, "text"), s.Text(r, "sprite")};
                i.kind = static_cast<ItemClass>(s.Pick(r, "class", {"heal", "xp"}));
                i.amount = s.Int(r, "amount", 0, 999);
                def.items.push_back(i);
            }
        }

        void LoadTreasure(const std::filesystem::path &folder, MatchDef &def, Diagnostics &out)
        {
            Sheet s(out, folder, "treasure.csv", {"id", "kind", "item", "amount", "weight"});
            for (int r = 0; r < s.Rows(); ++r)
            {
                TreasureDef t{s.Text(r, "id")};
                t.kind = static_cast<TreasureKind>(s.Pick(r, "kind", {"item", "essence", "xp", "heal"}));
                t.item = s.Ref(r, "item", def.items, true);
                t.amount = s.Int(r, "amount", 0, 999);
                t.weight = s.Int(r, "weight", 1, 999);
                if (s.ok && t.kind == TreasureKind::Item && t.item < 0)
                    s.Bad(s.Where(r, "item"), "an item card names an item");
                def.treasure.push_back(t);
            }
            if (s.ok && def.treasure.empty())
                s.Bad("treasure.csv", "at least one card is needed");
        }

        void LoadLevelCards(const std::filesystem::path &folder, MatchDef &def, Diagnostics &out)
        {
            Sheet s(out, folder, "levelcards.csv", {"id", "kind", "what", "amount", "weight"});
            for (int r = 0; r < s.Rows(); ++r)
            {
                LevelCardDef c{s.Text(r, "id")};
                c.kind = static_cast<CardKind>(s.Pick(r, "kind", {"learn", "stat", "upgrade"}));
                c.what = static_cast<Stat>(s.Pick(r, "what", {"hp", "attack", "defence", "speed", "move", "power", "cost"}));
                c.amount = s.Int(r, "amount", 0, 99);
                c.weight = s.Int(r, "weight", 1, 999);
                // 1. Stat cards raise a stat; upgrades raise power or cut cost; learn names "power" by convention and ignores it.
                const bool upgrade = c.what == Stat::Power || c.what == Stat::Cost;
                if (s.ok && ((c.kind == CardKind::Stat && upgrade) || (c.kind == CardKind::Upgrade && !upgrade)))
                    s.Bad(s.Where(r, "what"), "a stat card raises hp, attack, defence, speed or move; an upgrade, power or cost");
                def.levelCards.push_back(c);
            }
            if (s.ok && def.levelCards.empty())
                s.Bad("levelcards.csv", "at least one card is needed");
        }

        void LoadEvents(const std::filesystem::path &folder, MatchDef &def, Diagnostics &out)
        {
            Sheet s(out, folder, "events.csv", {"id", "name", "text", "label1", "chance1", "success1", "failure1", "label2", "chance2", "success2", "failure2"});
            for (int r = 0; r < s.Rows(); ++r)
            {
                EventDef e;
                e.id = s.Text(r, "id");
                e.name = s.Text(r, "name");
                e.text = s.Text(r, "text");
                e.choices.push_back({s.Text(r, "label1"), s.Int(r, "chance1", 0, 100), ParseOutcome(s, r, "success1", def), ParseOutcome(s, r, "failure1", def)});
                e.choices.push_back({s.Text(r, "label2"), s.Int(r, "chance2", 0, 100), ParseOutcome(s, r, "success2", def), ParseOutcome(s, r, "failure2", def)});
                def.events.push_back(e);
            }
        }
    }

    bool LoadMatch(const std::filesystem::path &folder, MatchDef &out, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. Files in dependency order: abilities before jobs and monsters, items before events.
        const std::size_t before = diagnostics.size();
        MatchDef def;
        LoadOptions(folder, def.options, diagnostics);
        LoadRules(folder, def.rules, diagnostics);
        LoadAbilities(folder, def, diagnostics);
        LoadJobs(folder, def, diagnostics);
        LoadCharacters(folder, def, diagnostics);
        LoadMonsters(folder, def, diagnostics);
        LoadSpawns(folder, def, diagnostics);
        LoadItems(folder, def, diagnostics);
        LoadEvents(folder, def, diagnostics);
        LoadTreasure(folder, def, diagnostics);
        LoadLevelCards(folder, def, diagnostics);
        if (diagnostics.size() != before)
            return false;
        out = std::move(def);
        return true;
    }
}
