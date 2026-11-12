#include "Plan.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <sstream>

namespace umbra::town
{
    using json = nlohmann::json;

    namespace
    {
        const char *const kindNames[] = {"ill", "theft", "feud", "leave", "death"};
        bool Pair(TroubleKind kind) { return kind == TroubleKind::Theft || kind == TroubleKind::Feud; }
        bool Grave(TroubleKind kind) { return kind == TroubleKind::Leave || kind == TroubleKind::Death; }

        // Does `wide` say no to everything `narrow` could be?
        bool Covers(const Pattern &wide, const Pattern &narrow)
        {
            return wide.kind == narrow.kind && (wide.who == anyone || wide.who == narrow.who) && (wide.other == anyone || wide.other == narrow.other);
        }
    }

    const char *KindName(TroubleKind kind) { return kindNames[static_cast<int>(kind)]; }

    bool Pattern::Matches(const Trouble &t) const
    {
        const auto fits = [&](int a, int b) { return (who == anyone || who == a) && (other == anyone || other == b); };
        // A row has no first and second: {feud, tobin, edda} is also Edda rowing with Tobin.
        return kind == t.kind && (fits(t.who, t.other) || (kind == TroubleKind::Feud && fits(t.other, t.who)));
    }

    bool ParsePlan(const std::string &text, const std::string &file, const TownDef &def, PlanDef &out, std::vector<core::Diagnostic> &diagnostics)
    {
        const std::size_t before = diagnostics.size();
        const auto bad = [&](const std::string &code, const std::string &field, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "plan." + code, file, field, message});
        };
        const json root = json::parse(text, nullptr, false);
        if (!root.is_object() || root.value("format", "") != "umbra.town.plan" || root.value("version", 0) != 1)
        {
            bad("format", "", "not an outcome plan (\"format\": \"umbra.town.plan\", \"version\": 1)");
            return false;
        }
        const auto number = [&](const json &at, const char *key, int low, int high, int fallback, const std::string &field) {
            if (!at.is_object() || !at.contains(key))
                return fallback;
            if (at[key].is_number_integer() && at[key].get<long long>() >= low && at[key].get<long long>() <= high)
                return at[key].get<int>();
            bad("number", field + "." + key, "a whole number from " + std::to_string(low) + " to " + std::to_string(high));
            return fallback;
        };
        // 1. One pattern: a kind, and residents by id where it names any.
        const auto patterns = [&](const json &at, const char *key, const std::string &field) {
            std::vector<Pattern> list;
            if (!at.is_object() || !at.contains(key))
                return list;
            for (std::size_t i = 0; i < at[key].size(); ++i)
            {
                const json &p = at[key][i];
                const std::string here = field + key + "[" + std::to_string(i) + "]";
                Pattern pattern;
                const std::string kind = p.is_object() ? p.value("kind", "") : "";
                int found = -1;
                for (int k = 0; k < 5; ++k)
                    if (kind == kindNames[k])
                        found = k;
                if (found < 0)
                {
                    bad("kind", here + ".kind", "one of \"ill\", \"theft\", \"feud\", \"leave\", \"death\"");
                    continue;
                }
                pattern.kind = static_cast<TroubleKind>(found);
                for (const char *role : {"who", "other"})
                    if (p.contains(role))
                    {
                        const int resident = p[role].is_string() ? def.Resident(p[role].get<std::string>()) : -1;
                        if (resident < 0 || (std::string(role) == "other" && !Pair(pattern.kind)))
                            bad("resident", here + "." + role, "the id of a resident (\"other\" only for theft and feud)");
                        (std::string(role) == "who" ? pattern.who : pattern.other) = resident;
                    }
                pattern.from = number(p, "from", 1, noLastDay, 1, here);
                pattern.to = number(p, "to", pattern.from, noLastDay, noLastDay, here);
                list.push_back(pattern);
            }
            return list;
        };
        PlanDef plan;
        const json pacing = root.value("pacing", json::object());
        plan.quietDays = number(pacing, "quietDays", 0, 30, 1, "pacing");
        plan.maxIll = number(pacing, "maxIll", 0, 64, 2, "pacing");
        plan.maxGone = number(pacing, "maxGone", 0, 64, 1, "pacing");
        plan.lookAhead = number(pacing, "lookAhead", 0, 7, 2, "pacing");
        plan.hour = number(pacing, "hour", 0, 23, 12, "pacing");
        plan.never = patterns(root, "never", "");
        plan.allow = patterns(root, "allow", "");

        // 2. Beats, in story order.
        std::set<std::string> ids;
        int graveBeats = 0;
        const json beats = root.value("beats", json::array());
        for (std::size_t i = 0; i < beats.size(); ++i)
        {
            const json &b = beats[i];
            const std::string field = "beats[" + std::to_string(i) + "]";
            BeatDef beat;
            beat.id = b.is_object() ? b.value("id", "") : "";
            beat.name = b.is_object() ? b.value("name", beat.id) : "";
            if (beat.id.empty() || !ids.insert(beat.id).second)
                bad("id", field + ".id", "every beat needs its own \"id\"");
            beat.from = number(b, "from", 1, noLastDay, 1, field);
            beat.to = number(b, "to", beat.from, noLastDay, beat.from, field);
            beat.outcomes = patterns(b, "outcomes", field + ".");
            beat.forbid = patterns(b, "forbid", field + ".");
            // 3. Checks a plan can fail before any town is run.
            if (!plan.beats.empty() && beat.to < plan.beats.back().from)
                bad("order", field + ".to", "this beat's window ends before the beat before it can start");
            bool alive = false, grave = !beat.outcomes.empty();
            for (const Pattern &outcome : beat.outcomes)
            {
                bool dead = false;
                for (const Pattern &no : plan.never)
                    dead = dead || Covers(no, outcome);
                for (const Pattern &no : beat.forbid)
                    dead = dead || Covers(no, outcome);
                alive = alive || !dead;
                grave = grave && Grave(outcome.kind);
            }
            if (!alive)
                bad("dead", field + ".outcomes", "no outcome is left: the beat needs at least one that \"never\" and its own \"forbid\" do not rule out");
            graveBeats += grave;
            plan.beats.push_back(std::move(beat));
        }
        if (graveBeats > plan.maxGone)
            bad("gone", "pacing.maxGone", std::to_string(graveBeats) + " beats can only be reached by someone leaving or dying, but the town may lose " + std::to_string(plan.maxGone));
        if (diagnostics.size() != before)
            return false;
        out = std::move(plan);
        return true;
    }

    bool LoadPlan(const std::filesystem::path &file, const TownDef &def, PlanDef &out, std::vector<core::Diagnostic> &diagnostics)
    {
        std::ifstream in(file, std::ios::binary);
        if (!in)
        {
            diagnostics.push_back({core::Severity::Error, "plan.file", file.generic_string(), "", "cannot open the plan file"});
            return false;
        }
        std::ostringstream text;
        text << in.rdbuf();
        return ParsePlan(text.str(), file.generic_string(), def, out, diagnostics);
    }
}
