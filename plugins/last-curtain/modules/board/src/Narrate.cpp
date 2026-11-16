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
            "died", "hit", "killed", "goalDone", "goalFailed", "cleared",
            "askSpare", "askItem", "askHunt", "asked", "answered", "grantSpare", "grantItem", "grantHunt", "refused",
            "passMight", "failMight", "passSanity", "failSanity", "passSpeed", "failSpeed", "replyYes", "replyNo"}; // Play's roll results and chat replies

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

        std::string GuestName(const NightDef &def, const NightState &state, int guest)
        {
            return guest >= 0 && U(guest) < def.guests.size() ? ShownName(def, state, guest) : "the house";
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
            Vars v{{"guest", GuestName(def, after, actor)}};
            const int cell = after.guests[U(actor)].cell;
            switch (action.kind)
            {
            case ActionKind::Move:
                for (const LogEntry &e : entries)
                    if (e.kind == LogKind::Opened)
                        return v["room"] = RoomName(def, after, e.a), Fill(captions, "open", v);
                return v["room"] = RoomName(def, after, cell), Fill(captions, "move", v);
            case ActionKind::Attack:
                v["foe"] = action.a >= 0 ? MonsterName(def, before, action.a) : GuestName(def, after, action.b);
                return Fill(captions, "fight", v);
            case ActionKind::Strike:
            {
                const Fighter &f = before.battle.fighters[U(action.a)];
                v["foe"] = f.monster >= 0 ? MonsterName(def, before, f.monster) : GuestName(def, after, f.guest);
                return Fill(captions, "strike", v);
            }
            case ActionKind::Chat: // a request (Night.md "Persuading"): spare me, an item, go after someone
                v["other"] = GuestName(def, after, action.a);
                if (action.b >= 0)
                    v["foe"] = GuestName(def, after, action.b);
                return Fill(captions, action.b == -1 ? "askSpare" : action.b == -2 ? "askItem" : "askHunt", v);
            case ActionKind::OpenChat:
                return v["other"] = GuestName(def, after, action.a), Fill(captions, "chat", v);
            case ActionKind::Power:
                return v["power"] = def.powers[U(def.guests[U(actor)].power)].id, Fill(captions, "power", v);
            case ActionKind::Answer: // the question put, word for word
                if (before.chat.question < 0)
                    return Fill(captions, "answer", v);
                v["other"] = GuestName(def, after, before.chat.asker), v["question"] = def.questions[U(before.chat.question)].text;
                return Fill(captions, "asked", v);
            case ActionKind::EndTurn: return Fill(captions, "endTurn", v);
            case ActionKind::Guard: return Fill(captions, "guard", v);
            case ActionKind::Leave: return Fill(captions, "leave", v);
            case ActionKind::UseItem: // the item by name (action.a: the bag slot)
            {
                const int item = action.a >= 0 && action.a < maxCarry ? before.guests[U(actor)].items[U(action.a)] : -1;
                return v["item"] = item >= 0 ? def.items[U(item)].name : "item", Fill(captions, "item", v);
            }
            case ActionKind::PickUp: return Fill(captions, "pickup", v);
            case ActionKind::DropItem: return Fill(captions, "drop", v);
            case ActionKind::LevelUp: return Fill(captions, "level", v);
            }
            return {};
        }

        // What came of a request: granted (by its kind) or not.
        std::string Told(const NightDef &def, const NightState &after, const LogEntry &e, const Captions &captions)
        {
            Vars v{{"who", GuestName(def, after, e.who)}, {"other", GuestName(def, after, e.other)}};
            if (e.b >= 0)
                v["foe"] = GuestName(def, after, e.b);
            return Fill(captions, !e.a ? "refused" : e.b == -1 ? "grantSpare" : e.b == -2 ? "grantItem" : "grantHunt", v);
        }

        // The outcome lines from the log, joined with "; ".
        std::string Result(const NightDef &def, const NightState &before, const NightState &after, const std::vector<LogEntry> &entries,
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
                    line = Fill(captions, e.kind == LogKind::Down ? "down" : "died", {{"who", GuestName(def, after, e.who)}});
                else if (e.kind == LogKind::Cleared)
                    line = Fill(captions, "cleared", {{"who", GuestName(def, after, e.who)}, {"event", def.events[U(e.a)].name}});
                else if (e.kind == LogKind::Goal)
                    line = Fill(captions, e.a ? "goalDone" : "goalFailed", {{"who", GuestName(def, after, e.who)}});
                else if (e.kind == LogKind::Persuade)
                    line = Told(def, after, e, captions);
                else if (e.kind == LogKind::Hit)
                {
                    // Who struck whom (a monster: the battle's first one), the damage and what is left.
                    const auto side = [&](int g) {
                        if (g >= 0)
                            return GuestName(def, after, g);
                        for (int i = 0; i < before.battle.count; ++i)
                            if (before.battle.fighters[U(i)].monster >= 0)
                                return "the " + MonsterName(def, before, before.battle.fighters[U(i)].monster);
                        return std::string{"the monster"};
                    };
                    line = Fill(captions, "hit", {{"who", side(e.who)}, {"target", side(e.other)}, {"damage", std::to_string(e.a)}, {"hp", std::to_string(e.b)}});
                }
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
                 const Captions &captions, bool light)
    {
        // 1. Seen when the start cell was lit, or the end cell or an opened room is lit now.
        const BoardView was = MakeView(def, before, viewer, light), now = MakeView(def, after, viewer, light);
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
        if (action.kind == ActionKind::Answer && before.chat.question >= 0) // the answer chosen, and what came of it
        {
            const QuestionDef &q = def.questions[U(before.chat.question)];
            beat.answer = Fill(captions, "answered", {{"guest", GuestName(def, after, actor)}, {"answer", action.a == 0 ? q.physicalText : q.magicalText}});
        }
        for (const LogEntry &e : entries)
            if (e.kind == LogKind::Persuade)
                beat.told = Told(def, after, e, captions);
        // 4. Gone into the dark: what happened there stays there.
        beat.dark = to >= 0 && !now.cells[U(to)].lit;
        if (!beat.dark)
            beat.result = Result(def, before, after, entries, captions);
        return beat;
    }
}
