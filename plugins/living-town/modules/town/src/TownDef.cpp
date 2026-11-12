#include "TownDef.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <sstream>

namespace umbra::town
{
    using json = nlohmann::json;

    namespace
    {
        template <typename T>
        int IndexOf(const std::vector<T> &list, const std::string &id)
        {
            for (std::size_t i = 0; i < list.size(); ++i)
                if (list[i].id == id)
                    return static_cast<int>(i);
            return -1;
        }

        // FNV-1a over text: the same on every compiler.
        void Mix(std::uint64_t &hash, const std::string &text)
        {
            for (const char c : text + "|")
                hash = (hash ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
        }
    }

    int TownDef::Need(const std::string &id) const { return IndexOf(needs, id); }
    int TownDef::Place(const std::string &id) const { return IndexOf(places, id); }
    int TownDef::Activity(const std::string &id) const { return IndexOf(activities, id); }
    int TownDef::Resident(const std::string &id) const { return IndexOf(residents, id); }

    bool ParseTown(const std::string &text, const std::string &file, const rpg::Content *content, TownDef &out,
                   std::vector<core::Diagnostic> &diagnostics)
    {
        const std::size_t before = diagnostics.size();
        const auto bad = [&](const std::string &code, const std::string &field, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "town." + code, file, field, message});
        };
        const json root = json::parse(text, nullptr, false);
        if (!root.is_object() || root.value("format", "") != "umbra.town" || root.value("version", 0) != 1)
        {
            bad("format", "", "not a town file (\"format\": \"umbra.town\", \"version\": 1)");
            return false;
        }
        // A whole number in a range, or the fallback when the key is absent.
        const auto number = [&](const json &at, const char *key, int low, int high, int fallback, const std::string &field) {
            if (!at.contains(key))
                return fallback;
            if (at[key].is_number_integer() && at[key].get<long long>() >= low && at[key].get<long long>() <= high)
                return at[key].get<int>();
            bad(key, field + "." + key, "a whole number from " + std::to_string(low) + " to " + std::to_string(high));
            return fallback;
        };
        const auto list = [&](const char *key) { return root.contains(key) && root[key].is_array() ? root[key] : json::array(); };
        const auto text_of = [](const json &at, const char *key) { return at.is_object() && at.contains(key) && at[key].is_string() ? at[key].get<std::string>() : std::string(); };
        TownDef def;
        std::set<std::string> ids;
        const auto id_of = [&](const json &at, const std::string &field) {
            const std::string id = text_of(at, "id");
            if (id.empty() || !ids.insert(field.substr(0, field.find('[')) + ":" + id).second)
                bad("id", field + ".id", "every entry needs its own \"id\"");
            return id;
        };
        def.urgent = number(root, "urgent", 100, needFull, 700, "");

        // 1. Needs.
        const json needs = list("needs"), places = list("places"), activities = list("activities");
        for (std::size_t i = 0; i < needs.size(); ++i)
        {
            const json &n = needs[i];
            const std::string field = "needs[" + std::to_string(i) + "]";
            def.needs.push_back({id_of(n, field), text_of(n, "name"), number(n, "risePerDay", 0, 20000, 0, field)});
        }
        if (def.needs.empty() || def.needs.size() > maxNeeds)
            bad("needs", "needs", "a town has 1 to " + std::to_string(maxNeeds) + " needs");

        // 2. Places (owners are resolved after the residents are read).
        for (std::size_t i = 0; i < places.size(); ++i)
        {
            const json &p = places[i];
            def.places.push_back({id_of(p, "places[" + std::to_string(i) + "]"), text_of(p, "name"), text_of(p, "scene"), text_of(p, "marker"), -1});
        }
        if (def.places.empty())
            bad("places", "places", "a town needs at least one place");

        // 3. Activities.
        for (std::size_t i = 0; i < activities.size(); ++i)
        {
            const json &a = activities[i];
            const std::string field = "activities[" + std::to_string(i) + "]";
            ActivityDef activity;
            activity.id = id_of(a, field);
            activity.name = text_of(a, "name");
            activity.where = text_of(a, "where");
            activity.place = def.Place(activity.where);
            if (activity.where != "home" && activity.where != "work" && activity.place < 0)
                bad("where", field + ".where", "\"home\", \"work\" or the id of a place");
            activity.hours = number(a, "hours", 1, 24, 1, field);
            activity.openFrom = number(a, "openFrom", 0, 24, 0, field);
            activity.openTo = number(a, "openTo", 0, 24, 24, field);
            activity.cost = number(a, "cost", 0, 100000, 0, field);
            activity.wage = number(a, "wage", 0, 100000, 0, field);
            activity.xp = number(a, "xp", 0, 100000, 0, field);
            activity.social = a.is_object() && a.value("social", false);
            activity.relief.assign(def.needs.size(), 0);
            if (a.is_object() && a.contains("relief") && a["relief"].is_object())
                for (const auto &[need, amount] : a["relief"].items())
                {
                    if (const int index = def.Need(need); index >= 0 && amount.is_number_integer())
                        activity.relief[static_cast<std::size_t>(index)] = amount.get<int>();
                    else
                        bad("relief", field + ".relief." + need, "a need of this town and a whole number per hour");
                }
            def.activities.push_back(std::move(activity));
        }
        // The first activity is what a resident can always do: free, always open, at home.
        if (def.activities.empty() || def.activities[0].cost != 0 || def.activities[0].where != "home" || def.activities[0].openFrom != 0 ||
            def.activities[0].openTo != 24)
            bad("activities", "activities[0]", "the first activity must be free, at \"home\" and always open (what anyone can fall back on)");

        // 4. Residents.
        const json residents = list("residents");
        for (std::size_t i = 0; i < residents.size(); ++i)
        {
            const json &r = residents[i];
            const std::string field = "residents[" + std::to_string(i) + "]";
            ResidentDef resident;
            resident.id = id_of(r, field);
            resident.name = text_of(r, "name");
            resident.actor = text_of(r, "actor");
            resident.voice = text_of(r, "voice");
            if (r.contains("pronoun"))
                resident.pronoun = text_of(r, "pronoun");
            if (resident.pronoun != "he" && resident.pronoun != "she" && resident.pronoun != "they")
                bad("pronoun", field + ".pronoun", "\"he\", \"she\" or \"they\"");
            if (content != nullptr && content->FindActor(resident.actor) == nullptr)
                bad("actor", field + ".actor", "no actor \"" + resident.actor + "\" in the RPG content");
            resident.home = def.Place(text_of(r, "home"));
            resident.work = def.Place(text_of(r, "work"));
            if (resident.home < 0)
                bad("home", field + ".home", "the id of a place");
            if (resident.work < 0)
                resident.work = resident.home; // no job: "work" activities happen at home
            resident.money = number(r, "money", 0, 1000000, 0, field);
            if (r.is_object() && r.contains("schedule") && r["schedule"].is_array())
                for (std::size_t s = 0; s < r["schedule"].size(); ++s)
                {
                    const json &entry = r["schedule"][s];
                    const std::string at = field + ".schedule[" + std::to_string(s) + "]";
                    const int activity = def.Activity(text_of(entry, "activity"));
                    if (activity < 0)
                        bad("schedule", at + ".activity", "the id of an activity");
                    resident.schedule.push_back({number(entry, "from", 0, 24, 0, at), number(entry, "to", 0, 24, 24, at), std::max(activity, 0)});
                }
            def.residents.push_back(std::move(resident));
        }
        if (def.residents.empty() || def.residents.size() > 64)
            bad("residents", "residents", "a town has 1 to 64 residents");

        // 5. Things that name residents: place owners and starting bonds.
        for (std::size_t i = 0; i < def.places.size(); ++i)
            if (const std::string owner = text_of(places[i], "owner"); !owner.empty())
                if ((def.places[i].owner = def.Resident(owner)) < 0)
                    bad("owner", "places[" + std::to_string(i) + "].owner", "the id of a resident");
        for (std::size_t i = 0; i < def.residents.size(); ++i)
            if (residents[i].is_object() && residents[i].contains("bonds") && residents[i]["bonds"].is_object())
                for (const auto &[other, value] : residents[i]["bonds"].items())
                {
                    if (const int index = def.Resident(other); index >= 0 && value.is_number_integer() && std::abs(value.get<int>()) <= 1000)
                        def.residents[i].bonds.emplace_back(index, value.get<int>());
                    else
                        bad("bonds", "residents[" + std::to_string(i) + "].bonds." + other, "a resident and a whole number from -1000 to 1000");
                }

        // 5b. Favours residents ask of the player.
        const json favours = list("favours");
        for (std::size_t i = 0; i < favours.size(); ++i)
        {
            const std::string field = "favours[" + std::to_string(i) + "]";
            FavourDef favour{id_of(favours[i], field), def.Resident(text_of(favours[i], "resident")), def.Resident(text_of(favours[i], "target")),
                             text_of(favours[i], "ask"), text_of(favours[i], "thanks"), number(favours[i], "hours", 1, 24 * 30, 24, field)};
            if (favour.resident < 0 || favour.target < 0 || favour.resident == favour.target || favour.ask.empty() || favour.thanks.empty())
                bad("favour", field, "a favour needs a \"resident\" who asks, another as \"target\", and the \"ask\" and \"thanks\" lines");
            def.favours.push_back(std::move(favour));
        }

        // 6. The fingerprint: every id in order (a save made with another town must not load).
        def.fingerprint = 14695981039346656037ULL;
        for (const NeedDef &n : def.needs) Mix(def.fingerprint, n.id);
        for (const PlaceDef &p : def.places) Mix(def.fingerprint, p.id);
        for (const ActivityDef &a : def.activities) Mix(def.fingerprint, a.id);
        for (const ResidentDef &r : def.residents) Mix(def.fingerprint, r.id);
        for (const FavourDef &f : def.favours) Mix(def.fingerprint, f.id);
        if (diagnostics.size() != before)
            return false;
        out = std::move(def);
        return true;
    }

    bool LoadTown(const std::filesystem::path &file, const rpg::Content *content, TownDef &out, std::vector<core::Diagnostic> &diagnostics)
    {
        std::ifstream in(file, std::ios::binary);
        if (!in)
        {
            diagnostics.push_back({core::Severity::Error, "town.read", file.generic_string(), "", "cannot open the file"});
            return false;
        }
        std::ostringstream text;
        text << in.rdbuf();
        return ParseTown(text.str(), file.generic_string(), content, out, diagnostics);
    }
}
