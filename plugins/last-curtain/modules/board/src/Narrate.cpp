#include <Narrate.hpp>

#include <BoardView.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

namespace umbra::night
{
    namespace
    {
        using Vars = std::map<std::string, std::string>;

        constexpr const char *keys[] = {
            "move", "open", "fight", "chat", "answer", "endTurn", "power", "strike",
            "guard", "leave", "item", "pickup", "drop", "level", "check", "passed", "failed", "found", "down",
            "died", "hit", "killed", "goalDone", "goalFailed", "joined"};

        std::size_t U(int i) { return static_cast<std::size_t>(i); }

        // Replaces each {name} in the line with its value.
        std::string Fill(const Captions &captions, const std::string &key, const Vars &vars)
        {
            std::string text = captions.lines.at(key);
            for (const auto &[name, value] : vars)
                for (std::size_t at = text.find("{" + name + "}"); at != std::string::npos;
                     at = text.find("{" + name + "}", at + value.size()))
                    text.replace(at, name.size() + 2, value);
            return text;
        }

        std::string GuestName(const NightDef &def, int guest)
        {
            return guest >= 0 && U(guest) < def.guests.size() ? def.guests[U(guest)].name : "the house";
        }

        std::string RoomName(const NightDef &def, const NightState &state, int cell)
        {
            const int room = cell >= 0 ? state.cells[U(cell)].room : -1;
            return room >= 0 ? def.rooms[U(room)].name : "dark";
        }

        std::string MonsterName(const NightDef &def, const NightState &state, int monster)
        {
            if (monster < 0 || U(monster) >= state.monsters.size())
                return "thing";
            return def.monsters[U(state.monsters[U(monster)].kind)].name;
        }

        // The decision line for the action.
        std::string Decision(const NightDef &def, const NightState &before, const NightState &after,
                             const std::vector<LogEntry> &entries, const Action &action, int actor,
                             const Captions &captions)
        {
            Vars v{{"guest", GuestName(def, actor)}};
            const int cell = after.guests[U(actor)].cell;
            switch (action.kind)
            {
            case ActionKind::Move:
                for (const LogEntry &e : entries)
                    if (e.kind == LogKind::Opened)
                        return v["room"] = RoomName(def, after, e.a), Fill(captions, "open", v);
                return v["room"] = RoomName(def, after, cell), Fill(captions, "move", v);
            case ActionKind::Attack:
                v["foe"] = action.a >= 0 ? MonsterName(def, before, action.a) : GuestName(def, action.b);
                return Fill(captions, "fight", v);
            case ActionKind::Strike:
            {
                const Fighter &f = before.battle.fighters[U(action.a)];
                v["foe"] = f.monster >= 0 ? MonsterName(def, before, f.monster) : GuestName(def, f.guest);
                return Fill(captions, "strike", v);
            }
            case ActionKind::Chat:
            case ActionKind::OpenChat:
                return v["other"] = GuestName(def, action.a), Fill(captions, "chat", v);
            case ActionKind::Power:
                return v["power"] = def.powers[U(def.guests[U(actor)].power)].id, Fill(captions, "power", v);
            case ActionKind::Answer: return Fill(captions, "answer", v);
            case ActionKind::EndTurn: return Fill(captions, "endTurn", v);
            case ActionKind::Guard: return Fill(captions, "guard", v);
            case ActionKind::Leave: return Fill(captions, "leave", v);
            case ActionKind::UseItem: return Fill(captions, "item", v);
            case ActionKind::PickUp: return Fill(captions, "pickup", v);
            case ActionKind::DropItem: return Fill(captions, "drop", v);
            case ActionKind::LevelUp: return Fill(captions, "level", v);
            }
            return {};
        }

        // The outcome lines from the log, joined with "; ".
        std::string Result(const NightDef &def, const std::vector<LogEntry> &entries,
                            const Captions &captions)
        {
            std::string out;
            for (const LogEntry &e : entries)
            {
                std::string line;
                if (e.kind == LogKind::Check)
                    line = Fill(captions, "check", {{"die", std::to_string(e.a)}, {"stat", std::to_string(e.other)},
                                                    {"target", std::to_string(e.b)},
                                                    {"verdict", captions.lines.at(e.a + e.other >= e.b ? "passed" : "failed")}});
                else if (e.kind == LogKind::Item && e.a >= 0)
                    line = Fill(captions, "found", {{"item", def.items[U(e.a)].name}});
                else if (e.kind == LogKind::Down || e.kind == LogKind::Died)
                    line = Fill(captions, e.kind == LogKind::Down ? "down" : "died", {{"who", GuestName(def, e.who)}});
                else if (e.kind == LogKind::Goal)
                    line = Fill(captions, e.a ? "goalDone" : "goalFailed", {{"who", GuestName(def, e.who)}});
                else if (e.kind == LogKind::Persuade && e.a && e.b >= def.rules.persuade.approvals)
                    line = Fill(captions, "joined", {{"who", GuestName(def, e.who)}, {"other", GuestName(def, e.other)}});
                else if (e.kind == LogKind::Hit)
                    line = Fill(captions, "hit", {{"damage", std::to_string(e.a)}});
                else if (e.kind == LogKind::Monster && e.other == 1 && e.a >= 0)
                    line = Fill(captions, "killed", {{"foe", def.monsters[U(e.a)].name}});
                if (!line.empty())
                    out += (out.empty() ? "" : "; ") + line;
            }
            return out;
        }
    }

    bool LoadCaptions(const std::filesystem::path &file, Captions &out, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. Parse; 2. every key must be a string.
        std::ifstream stream(file);
        const nlohmann::json json = nlohmann::json::parse(stream, nullptr, false);
        const std::string name = file.filename().string();
        if (json.is_discarded() || !json.is_object())
        {
            diagnostics.push_back({core::Severity::Error, "night.captions", name, "", "not a JSON object"});
            return false;
        }
        bool ok = true;
        for (const char *key : keys)
        {
            if (json.contains(key) && json[key].is_string())
                out.lines[key] = json[key].get<std::string>();
            else
                ok = false, diagnostics.push_back({core::Severity::Error, "night.captions", name, key, "missing line"});
        }
        return ok;
    }

    Beat Narrate(const NightDef &def, const NightState &before, const NightState &after,
                 const std::vector<LogEntry> &entries, const Action &action, int actor, int viewer,
                 const Captions &captions)
    {
        // 1. Seen when the start cell was lit, or the end cell or an opened room is lit now.
        const BoardView was = MakeView(def, before, viewer), now = MakeView(def, after, viewer);
        const int from = before.guests[U(actor)].cell, to = after.guests[U(actor)].cell;
        Beat beat;
        beat.guest = actor;
        beat.seen = (from >= 0 && was.cells[U(from)].lit) || (to >= 0 && now.cells[U(to)].lit);
        for (const LogEntry &e : entries)
            if (e.kind == LogKind::Opened && now.cells[U(e.a)].lit)
                beat.seen = true;
        if (!beat.seen)
            return beat; // 2. In the dark: only the opened room shows, from the state itself.

        // 3. The camera, the walk and the two lines.
        beat.camera = to;
        if (from != to)
            beat.path = {from, to};
        beat.caption = Decision(def, before, after, entries, action, actor, captions);
        beat.result = Result(def, entries, captions);
        return beat;
    }
}
