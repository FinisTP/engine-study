#include <Night.hpp>

#include "Csv.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <initializer_list>

namespace umbra::night
{
    namespace
    {
        using Diagnostics = std::vector<core::Diagnostic>;

        void Fail(Diagnostics &out, const std::string &file, const std::string &field, const std::string &message)
        {
            const std::string name = file.substr(0, file.find('.'));
            out.push_back({core::Severity::Error, "night." + name, file, field, message});
        }

        bool ParseInt(const std::string &text, int &value)
        {
            const char *end = text.data() + text.size();
            return !text.empty() && std::from_chars(text.data(), end, value).ptr == end;
        }

        // One CSV file with its name, for reading cells with diagnostics.
        struct Sheet
        {
            Diagnostics &out;
            std::string file;
            CsvTable table;
            bool ok = true;

            Sheet(Diagnostics &diagnostics, const std::filesystem::path &folder, const char *name,
                  std::initializer_list<const char *> columns)
                : out(diagnostics), file(name)
            {
                // 1. The file, its columns, and rows as long as the header.
                std::string error;
                if (!ReadCsv(folder / name, table, error))
                    { Bad(file, error); return; }
                for (const char *column : columns)
                    if (table.Column(column) < 0)
                        { Bad(file + ":" + column, "the column is missing"); return; }
                for (std::size_t r = 0; r < table.rows.size(); ++r)
                    if (table.rows[r].size() != table.header.size())
                    {
                        Bad(file + ":" + std::to_string(r + 1), "the row has " + std::to_string(table.rows[r].size()) +
                                                                     " cells; the header has " + std::to_string(table.header.size()));
                        return;
                    }
            }

            void Bad(const std::string &field, const std::string &message)
            {
                Fail(out, file, field, message);
                ok = false;
            }

            int Rows() const { return ok ? static_cast<int>(table.rows.size()) : 0; }

            std::string Text(int row, const char *column)
            {
                const std::string &cell = table.rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(table.Column(column))];
                if (cell.empty())
                    Bad(Where(row, column), "the cell is blank");
                return cell;
            }

            int Int(int row, const char *column)
            {
                const std::string cell = Text(row, column);
                int value = 0;
                if (!cell.empty() && !ParseInt(cell, value))
                    Bad(Where(row, column), "'" + cell + "' is not a whole number");
                return value;
            }

            // Six hex digits, RRGGBB.
            std::uint32_t Color(int row, const char *column)
            {
                const std::string cell = Text(row, column);
                if (cell.size() == 6 && std::all_of(cell.begin(), cell.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
                    return static_cast<std::uint32_t>(std::stoul(cell, nullptr, 16));
                if (!cell.empty())
                    Bad(Where(row, column), "'" + cell + "' is not a colour (six hex digits, RRGGBB)");
                return 0;
            }

            // The position of the cell's word in the list.
            int Pick(int row, const char *column, std::initializer_list<const char *> words)
            {
                const std::string cell = Text(row, column);
                int i = 0;
                for (const char *word : words)
                {
                    if (cell == word)
                        return i;
                    ++i;
                }
                if (!cell.empty())
                    Bad(Where(row, column), "'" + cell + "' is not one of the allowed words");
                return 0;
            }

            // The row of another file with this id; "none" gives -1 when allowed.
            template <class T> int Ref(int row, const char *column, const std::vector<T> &rows, bool allowNone = false)
            {
                const std::string cell = Text(row, column);
                if (allowNone && cell == "none")
                    return -1;
                const int found = NightDef::Find(rows, cell);
                if (found < 0 && !cell.empty())
                    Bad(Where(row, column), "unknown id '" + cell + "'");
                return found;
            }

            std::string Where(int row, const char *column) const { return file + ":" + std::to_string(row + 1) + ":" + column; }
        };

        StatKind ToStat(int i) { return static_cast<StatKind>(i); }

        // rules.json: every number is required.
        void LoadRules(const std::filesystem::path &folder, Rules &r, Diagnostics &out)
        {
            const std::string file = "rules.json";
            std::ifstream stream(folder / file);
            if (!stream)
                return Fail(out, file, file, "cannot open the file");
            const nlohmann::json json = nlohmann::json::parse(stream, nullptr, false);
            if (json.is_discarded())
                return Fail(out, file, file, "not valid JSON");

            auto get = [&](const char *group, const char *key, int &value) {
                const nlohmann::json *node = group ? (json.contains(group) ? &json[group] : nullptr) : &json;
                const std::string field = file + ":" + (group ? std::string(group) + "." : "") + key;
                if (!node || !node->contains(key) || !(*node)[key].is_number_integer())
                    return Fail(out, file, field, "a whole number is required");
                value = (*node)[key].get<int>();
            };
            auto list = [&](const char *group, const char *key, std::vector<int> &values) {
                const std::string field = file + ":" + group + "." + key;
                values.clear();
                if (!json.contains(group) || !json[group].contains(key) || !json[group][key].is_array() || json[group][key].empty())
                    return Fail(out, file, field, "a list of whole numbers is required");
                for (const auto &v : json[group][key])
                    values.push_back(v.is_number_integer() ? v.get<int>() : 0);
            };

            get("clock", "startHour", r.clock.startHour);
            get("clock", "rounds", r.clock.rounds);
            get("clock", "turnAfterRound", r.clock.turnAfterRound);
            get("board", "width", r.board.width);
            get("board", "height", r.board.height);
            get("board", "rooms", r.board.rooms);
            get("checks", "easy", r.checks.easy);
            get("checks", "normal", r.checks.normal);
            get("checks", "hard", r.checks.hard);
            get("light", "steps", r.light.steps);
            get("moves", "stepsPerSpeed", r.moves.stepsPerSpeed);
            get(nullptr, "carry", r.carry);
            get("battle", "rounds", r.battle.rounds);
            get("battle", "minDamage", r.battle.minDamage);
            get("battle", "base", r.battle.base);
            get("battle", "defenseDivisor", r.battle.defenseDivisor);
            get("battle", "guardDivisor", r.battle.guardDivisor);
            list("levels", "xp", r.levels.xp);
            get("levels", "hp", r.levels.hp);
            get("levels", "stat", r.levels.stat);
            get("monsters", "fromRound", r.monsters.fromRound);
            get("monsters", "chancePercent", r.monsters.chancePercent);
            get("monsters", "stayRounds", r.monsters.stayRounds);
            get("monsters", "max", r.monsters.max);
            get("goals", "failHp", r.goals.failHp);
            get("goals", "earlyRooms", r.goals.earlyRooms);
            get("goals", "roomGoalRooms", r.goals.roomGoalRooms);
            get("persuade", "approvals", r.persuade.approvals);
            get("persuade", "bond", r.persuade.bond);
            get("guests", "downedReturnHp", r.guests.downedReturnHp);
            get("guests", "attackBondLoss", r.guests.attackBondLoss);
            get("balance", "companyWinMin", r.balance.companyWinMin);
            get("balance", "companyWinMax", r.balance.companyWinMax);
            get("balance", "takenMin", r.balance.takenMin);
            get("balance", "takenMax", r.balance.takenMax);
            get("balance", "allFinishedMax", r.balance.allFinishedMax);
            get("balance", "roomsMin", r.balance.roomsMin);
            get("balance", "roomsMax", r.balance.roomsMax);
            get("balance", "botChatRoundsMin", r.balance.botChatRoundsMin);
            get("balance", "goalMin", r.balance.goalMin);
            get("balance", "goalMax", r.balance.goalMax);

            // Limits the code relies on.
            auto check = [&](bool good, const char *field, const char *message) {
                if (!good)
                    Fail(out, file, file + ":" + field, message);
            };
            check(r.carry >= 1 && r.carry <= maxCarry, "carry", "must be 1 to 6 (a guest has 6 item slots)");
            check(r.board.width >= 1 && r.board.height >= 1 && r.board.width * r.board.height <= maxCells, "board", "the grid holds at most 81 cells");
            check(r.board.rooms >= 2 && r.board.rooms <= r.board.width * r.board.height, "board.rooms", "must fit the grid and hold the foyer and the under-stage");
            check(r.clock.rounds >= 1 && r.clock.rounds <= 100, "clock.rounds", "must be 1 to 100");
            check(r.clock.turnAfterRound >= 1 && r.clock.turnAfterRound < r.clock.rounds, "clock.turnAfterRound", "must come before the last round");
            check(r.battle.rounds >= 1 && r.battle.minDamage >= 0, "battle", "rounds must be 1 or more, minDamage 0 or more");
            check(r.battle.defenseDivisor >= 1 && r.battle.guardDivisor >= 1, "battle", "divisors must be 1 or more");
            check(r.moves.stepsPerSpeed >= 1, "moves.stepsPerSpeed", "must be 1 or more");
            check(r.light.steps >= 0 && r.monsters.max >= 0 && r.monsters.max <= 8, "monsters.max", "must be 0 to 8");
            check(r.goals.failHp >= 1 && r.goals.earlyRooms >= 8 && r.goals.earlyRooms <= r.board.rooms && r.goals.roomGoalRooms >= 8 &&
                      r.goals.roomGoalRooms <= r.board.rooms,
                  "goals", "failHp 1 or more; earlyRooms and roomGoalRooms 8 to board.rooms");
            check(r.balance.companyWinMin <= r.balance.companyWinMax, "balance", "companyWinMin must not exceed companyWinMax");
            for (std::size_t i = 1; i < r.levels.xp.size(); ++i)
                check(r.levels.xp[i] > r.levels.xp[i - 1], "levels.xp", "the steps must rise");
        }

        // powers.json: an object of id -> { kind, damage, range, rounds, cooldown }; missing numbers are 0 (cooldown 1).
        void LoadPowers(const std::filesystem::path &folder, std::vector<PowerDef> &powers, Diagnostics &out)
        {
            const std::string file = "powers.json";
            std::ifstream stream(folder / file);
            if (!stream)
                return Fail(out, file, file, "cannot open the file");
            const nlohmann::json json = nlohmann::json::parse(stream, nullptr, false);
            if (json.is_discarded() || !json.is_object())
                return Fail(out, file, file, "not a valid JSON object");
            static const char *kinds[] = {"line", "area", "flood", "pull", "seal", "swap", "reflect"};
            for (const auto &[id, value] : json.items())
            {
                PowerDef p;
                p.id = id;
                const std::string kind = value.value("kind", std::string());
                int k = 0;
                while (k < 7 && kind != kinds[k])
                    ++k;
                if (k == 7)
                    Fail(out, file, file + ":" + id + ":kind", "'" + kind + "' is not a power kind");
                p.kind = static_cast<PowerKind>(k % 7);
                p.damage = value.value("damage", 0);
                p.range = value.value("range", 0);
                p.rounds = value.value("rounds", 0);
                p.cooldown = value.value("cooldown", 1);
                if (p.cooldown < 1 || p.damage < 0 || p.range < 0 || p.rounds < 0)
                    Fail(out, file, file + ":" + id, "numbers must be 0 or more, cooldown 1 or more");
                powers.push_back(p);
            }
        }
    }

    bool LoadNight(const std::filesystem::path &folder, NightDef &out, std::vector<core::Diagnostic> &diagnostics)
    {
        const std::size_t before = diagnostics.size();
        NightDef d;
        LoadRules(folder, d.rules, diagnostics);
        LoadPowers(folder, d.powers, diagnostics);

        const auto stats = {"might", "sanity", "speed", "maxhp", "light", "none"};
        const auto weapons = {"physical", "magical"};

        Sheet buffs(diagnostics, folder, "buffs.csv", {"id", "name", "stat", "amount", "rounds"});
        for (int r = 0; r < buffs.Rows(); ++r)
            d.buffs.push_back({buffs.Text(r, "id"), buffs.Text(r, "name"),
                               static_cast<BuffStat>(buffs.Pick(r, "stat", {"guard", "might", "sanity"})),
                               buffs.Int(r, "amount"), buffs.Int(r, "rounds")});

        Sheet styles(diagnostics, folder, "styles.csv", {"style", "explore", "fight", "loot", "bonus", "fleeBelowHpPercent", "avoidGuests",
                                                         "healBelowPercent", "huntHpPercent", "huntNearSteps", "crowd", "chatty", "greed"});
        for (int r = 0; r < styles.Rows(); ++r)
            d.styles.push_back({styles.Text(r, "style"), styles.Int(r, "explore"), styles.Int(r, "fight"), styles.Int(r, "loot"),
                                styles.Int(r, "bonus"), styles.Int(r, "fleeBelowHpPercent"), styles.Int(r, "avoidGuests"),
                                styles.Int(r, "healBelowPercent"), styles.Int(r, "huntHpPercent"), styles.Int(r, "huntNearSteps"), styles.Int(r, "crowd"),
                                styles.Int(r, "chatty"), styles.Int(r, "greed")});

        Sheet items(diagnostics, folder, "items.csv", {"id", "name", "kind", "effect", "amount", "buff", "weight", "text", "room"});
        for (int r = 0; r < items.Rows(); ++r)
        {
            ItemDef i;
            i.id = items.Text(r, "id");
            i.name = items.Text(r, "name");
            i.kind = static_cast<ItemKind>(items.Pick(r, "kind", {"heal", "boost", "worn", "relic", "keepsake"}));
            i.effect = static_cast<Effect>(items.Pick(r, "effect", {"none", "hp", "hproom", "maxhp", "might", "sanity", "speed", "item", "xp", "buff", "armor", "light", "revive"}));
            i.amount = items.Int(r, "amount");
            i.buff = items.Ref(r, "buff", d.buffs, true);
            i.weight = items.Int(r, "weight");
            i.text = items.Text(r, "text");
            if (i.effect == Effect::Buff && i.buff < 0)
                items.Bad(items.Where(r, "buff"), "a buff item names a buff");
            if (i.weight < (i.kind == ItemKind::Keepsake ? 0 : 1))
                items.Bad(items.Where(r, "weight"), "must be 1 or more (a keepsake's is 0: it is never drawn)");
            d.items.push_back(i);
        }

        if (d.items.size() > 64)
            items.Bad(items.file, "at most 64 items");

        Sheet monsters(diagnostics, folder, "monsters.csv", {"id", "name", "hp", "might", "sanity", "attack", "resist", "xp", "debuff", "hpPerHour", "statPerHour"});
        for (int r = 0; r < monsters.Rows(); ++r)
        {
            MonsterDef m;
            m.id = monsters.Text(r, "id");
            m.name = monsters.Text(r, "name");
            m.hp = monsters.Int(r, "hp");
            m.might = monsters.Int(r, "might");
            m.sanity = monsters.Int(r, "sanity");
            m.attack = static_cast<Weapon>(monsters.Pick(r, "attack", weapons));
            m.resist = monsters.Pick(r, "resist", {"physical", "magical", "none"});
            m.resist = m.resist == 2 ? -1 : m.resist;
            m.xp = monsters.Int(r, "xp");
            m.debuff = monsters.Ref(r, "debuff", d.buffs, true);
            m.hpPerHour = monsters.Int(r, "hpPerHour");
            m.statPerHour = monsters.Int(r, "statPerHour");
            if (m.hp < 1)
                monsters.Bad(monsters.Where(r, "hp"), "must be 1 or more");
            d.monsters.push_back(m);
        }

        Sheet rooms(diagnostics, folder, "rooms.csv", {"id", "name", "kind", "doors", "stat", "amount", "count"});
        for (int r = 0; r < rooms.Rows(); ++r)
        {
            RoomDef room;
            room.id = rooms.Text(r, "id");
            room.name = rooms.Text(r, "name");
            room.kind = static_cast<RoomKind>(rooms.Pick(r, "kind", {"foyer", "understage", "plain", "item", "event", "trap", "transport", "bonus"}));
            for (char c : rooms.Text(r, "doors"))
            {
                const std::size_t bit = std::string_view("NESW").find(c);
                if (bit == std::string_view::npos)
                    rooms.Bad(rooms.Where(r, "doors"), "doors are letters of NESW");
                else
                    room.doors = static_cast<std::uint8_t>(room.doors | (1u << bit));
            }
            room.stat = ToStat(rooms.Pick(r, "stat", stats));
            room.amount = rooms.Int(r, "amount");
            room.count = rooms.Int(r, "count");
            if (room.count < 0)
                rooms.Bad(rooms.Where(r, "count"), "must be 0 or more");
            d.rooms.push_back(room);
        }
        if (rooms.ok)
        {
            // The board needs one foyer, one under-stage, transport as one pair, and enough rooms.
            int count[9] = {}, total = 0;
            for (const RoomDef &room : d.rooms)
                count[static_cast<int>(room.kind)] += room.count, total += room.count;
            if (count[0] != 1 || count[1] != 1)
                rooms.Bad(rooms.file, "exactly one foyer and one under-stage are needed");
            else if (const int t = count[static_cast<int>(RoomKind::Transport)]; t != 0 && t != 2)
                rooms.Bad(rooms.file, "transport rooms come as one pair");
            else if (total < d.rules.board.rooms)
                rooms.Bad(rooms.file, "fewer rooms than rules.json board.rooms");
        }

        Sheet events(diagnostics, folder, "events.csv", {"id", "name", "kind", "stat", "target", "passEffect", "passAmount", "failEffect", "failAmount", "text", "persist", "clear"});
        const auto effects = {"none", "hp", "hproom", "maxhp", "might", "sanity", "speed", "item", "xp", "buff", "armor", "light", "revive", "back"};
        for (int r = 0; r < events.Rows(); ++r)
        {
            EventDef e;
            e.id = events.Text(r, "id");
            e.name = events.Text(r, "name");
            e.text = events.Text(r, "text");
            e.trap = events.Pick(r, "kind", {"event", "trap"}) == 1;
            e.stat = ToStat(events.Pick(r, "stat", {"might", "sanity", "speed"}));
            const int target = events.Pick(r, "target", {"easy", "normal", "hard"});
            e.target = target == 0 ? d.rules.checks.easy : target == 1 ? d.rules.checks.normal : d.rules.checks.hard;
            e.passEffect = static_cast<Effect>(events.Pick(r, "passEffect", effects));
            e.passAmount = events.Int(r, "passAmount");
            e.failEffect = static_cast<Effect>(events.Pick(r, "failEffect", effects));
            e.failAmount = events.Int(r, "failAmount");
            e.persist = events.Int(r, "persist") != 0;
            // "none", "might>=6" (sanity, speed too) or "carry:<item id>".
            const std::string clear = events.Text(r, "clear");
            const std::size_t at = clear.find(">=");
            if (clear.rfind("carry:", 0) == 0)
            {
                e.clear = ClearKind::Carry;
                e.clearItem = NightDef::Find(d.items, clear.substr(6));
                if (e.clearItem < 0)
                    events.Bad(events.Where(r, "clear"), "no item " + clear.substr(6) + " in items.csv");
            }
            else if (at != std::string::npos)
            {
                const std::string stat = clear.substr(0, at);
                e.clear = ClearKind::Stat;
                e.clearStat = stat == "might" ? StatKind::Might : stat == "sanity" ? StatKind::Sanity : StatKind::Speed;
                e.clearAmount = std::atoi(clear.c_str() + at + 2);
                if ((stat != "might" && stat != "sanity" && stat != "speed") || e.clearAmount <= 0)
                    events.Bad(events.Where(r, "clear"), "'" + clear + "' is not might>=N, sanity>=N or speed>=N");
            }
            else if (clear != "none")
                events.Bad(events.Where(r, "clear"), "'" + clear + "' is not none, <stat>>=N or carry:<item>");
            d.events.push_back(e);
        }

        if (d.events.size() > 64)
            events.Bad(events.file, "at most 64 events and traps");

        Sheet guests(diagnostics, folder, "guests.csv", {"id", "name", "pronoun", "weapon", "hp", "might", "sanity", "speed", "style",
                                                         "taken", "takenHp", "takenMight", "takenSanity", "takenSpeed", "color", "voice", "takenVoice", "power"});
        for (int r = 0; r < guests.Rows(); ++r)
        {
            GuestDef g;
            g.id = guests.Text(r, "id");
            g.name = guests.Text(r, "name");
            g.pronoun = guests.Text(r, "pronoun");
            g.weapon = static_cast<Weapon>(guests.Pick(r, "weapon", weapons));
            g.hp = guests.Int(r, "hp");
            g.might = guests.Int(r, "might");
            g.sanity = guests.Int(r, "sanity");
            g.speed = guests.Int(r, "speed");
            g.style = guests.Ref(r, "style", d.styles);
            g.taken = guests.Text(r, "taken");
            g.takenHp = guests.Int(r, "takenHp");
            g.takenMight = guests.Int(r, "takenMight");
            g.takenSanity = guests.Int(r, "takenSanity");
            g.takenSpeed = guests.Int(r, "takenSpeed");
            g.color = guests.Color(r, "color");
            g.voice = guests.Text(r, "voice");
            g.takenVoice = guests.Text(r, "takenVoice");
            g.power = guests.Ref(r, "power", d.powers);
            if (g.hp < 1 || g.takenHp < 1)
                guests.Bad(guests.Where(r, "hp"), "hp and takenHp must be 1 or more");
            d.guests.push_back(g);
        }
        if (guests.ok && (d.guests.size() < 2 || d.guests.size() > 7))
            guests.Bad(guests.file, "a night has 2 to 7 guests");

        // Minions: one kind per Taken form, after the ordinary monsters in the same list.
        Sheet minions(diagnostics, folder, "minions.csv", {"taken", "id", "name", "hp", "might", "sanity", "attack", "resist", "xp", "debuff", "hpShare", "count"});
        for (int r = 0; r < minions.Rows(); ++r)
        {
            MonsterDef m;
            m.taken = minions.Ref(r, "taken", d.guests);
            m.id = minions.Text(r, "id");
            m.name = minions.Text(r, "name");
            m.hp = minions.Int(r, "hp");
            m.might = minions.Int(r, "might");
            m.sanity = minions.Int(r, "sanity");
            m.attack = static_cast<Weapon>(minions.Pick(r, "attack", weapons));
            m.resist = minions.Pick(r, "resist", {"physical", "magical", "none"});
            m.resist = m.resist == 2 ? -1 : m.resist;
            m.xp = minions.Int(r, "xp");
            m.debuff = minions.Ref(r, "debuff", d.buffs, true);
            m.hpShare = minions.Int(r, "hpShare");
            m.count = minions.Int(r, "count");
            if (m.hp < 1 || m.hpShare < 0 || m.hpShare > 100 || m.count < 0)
                minions.Bad(minions.Where(r, "hp"), "hp must be 1 or more, hpShare 0 to 100, count 0 or more");
            d.monsters.push_back(m);
        }

        // Questions: five per guest, each answer's target a word of rules.checks.
        Sheet questions(diagnostics, folder, "questions.csv", {"guest", "id", "text", "physicalText", "physicalTarget", "magicalText", "magicalTarget"});
        auto targetOf = [&](int word) { return word == 0 ? d.rules.checks.easy : word == 1 ? d.rules.checks.normal : d.rules.checks.hard; };
        for (int r = 0; r < questions.Rows(); ++r)
            d.questions.push_back({questions.Ref(r, "guest", d.guests), questions.Text(r, "id"), questions.Text(r, "text"), questions.Text(r, "physicalText"),
                                   questions.Text(r, "magicalText"), targetOf(questions.Pick(r, "physicalTarget", {"easy", "normal", "hard"})),
                                   targetOf(questions.Pick(r, "magicalTarget", {"easy", "normal", "hard"}))});
        for (std::size_t g = 0; g < d.guests.size() && questions.ok && guests.ok; ++g)
            if (std::count_if(d.questions.begin(), d.questions.end(), [&](const QuestionDef &q) { return q.guest == static_cast<int>(g); }) != 5)
                questions.Bad(questions.file, d.guests[g].id + " needs exactly 5 questions");

        Sheet bonds(diagnostics, folder, "bonds.csv", {"from", "to", "bond"});
        for (int r = 0; r < bonds.Rows(); ++r)
        {
            BondDef b{bonds.Ref(r, "from", d.guests), bonds.Ref(r, "to", d.guests), bonds.Int(r, "bond")};
            if (b.bond < -1000 || b.bond > 1000)
                bonds.Bad(bonds.Where(r, "bond"), "must be -1000 to 1000");
            d.bonds.push_back(b);
        }

        // Keepsakes wait in a room (named in items.csv `room`, once the rooms are known).
        for (int r = 0; r < items.Rows() && items.ok && rooms.ok; ++r)
        {
            ItemDef &i = d.items[static_cast<std::size_t>(r)];
            i.room = items.Ref(r, "room", d.rooms, true);
            if ((i.kind == ItemKind::Keepsake) != (i.room >= 0))
                items.Bad(items.Where(r, "room"), "a keepsake names its room; other items say none");
        }

        // Goals: two or three per guest; the target fits the kind.
        Sheet goals(diagnostics, folder, "goals.csv", {"guest", "id", "kind", "target", "count", "text"});
        for (int r = 0; r < goals.Rows() && guests.ok && items.ok && rooms.ok; ++r)
        {
            GoalDef g;
            g.guest = goals.Ref(r, "guest", d.guests);
            g.id = goals.Text(r, "id");
            g.kind = static_cast<GoalKind>(goals.Pick(r, "kind", {"room", "item", "events", "monsters"}));
            g.count = goals.Int(r, "count");
            g.text = goals.Text(r, "text");
            if (g.kind == GoalKind::Room)
                g.target = goals.Ref(r, "target", d.rooms);
            else if (g.kind == GoalKind::Item)
                g.target = goals.Ref(r, "target", d.items);
            if (g.kind == GoalKind::Item && g.target >= 0 && d.items[static_cast<std::size_t>(g.target)].kind != ItemKind::Keepsake)
                goals.Bad(goals.Where(r, "target"), "an item goal names a keepsake");
            if (g.count < 1)
                goals.Bad(goals.Where(r, "count"), "must be 1 or more");
            d.goals.push_back(g);
        }
        for (std::size_t g = 0; g < d.guests.size() && goals.ok && guests.ok; ++g)
        {
            const auto n = std::count_if(d.goals.begin(), d.goals.end(), [&](const GoalDef &goal) { return goal.guest == static_cast<int>(g); });
            if (n < 2 || n > 3)
                goals.Bad(goals.file, d.guests[g].id + " needs two or three goals");
        }

        if (diagnostics.size() != before)
            return false;
        // The fingerprint: every file's bytes, in a fixed order.
        d.fingerprint = 14695981039346656037ULL;
        for (const char *name : {"rules.json", "powers.json", "buffs.csv", "styles.csv", "items.csv", "monsters.csv", "rooms.csv",
                                 "events.csv", "guests.csv", "minions.csv", "questions.csv", "bonds.csv", "goals.csv"})
        {
            std::ifstream file(folder / name, std::ios::binary);
            for (char c; file.get(c);)
                d.fingerprint = (d.fingerprint ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
        }
        out = std::move(d);
        return true;
    }
}
