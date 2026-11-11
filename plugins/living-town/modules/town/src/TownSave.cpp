#include "Town.hpp"

#include <nlohmann/json.hpp>

namespace umbra::town
{
    using json = nlohmann::json;

    json SaveTown(const TownDef &def, const TownState &state)
    {
        // 64-bit numbers go as text: JSON readers only keep 53 bits of a number.
        json out = {{"format", "umbra.town.state"}, {"version", 1}, {"town", std::to_string(def.fingerprint)}, {"seed", std::to_string(state.seed)},
                    {"random", std::to_string(state.random)}, {"tick", state.tick}, {"minted", state.minted}};
        for (std::size_t i = 0; i < state.residents.size(); ++i)
        {
            const ResidentState &r = state.residents[i];
            json needs = json::object(), bonds = json::object();
            for (std::size_t n = 0; n < def.needs.size(); ++n)
                needs[def.needs[n].id] = r.needs[n];
            for (std::size_t other = 0; other < state.residents.size(); ++other)
                if (state.Bond(i, other) != 0)
                    bonds[def.residents[other].id] = state.Bond(i, other);
            out["residents"].push_back({{"id", def.residents[i].id}, {"place", def.places[static_cast<std::size_t>(r.place)].id},
                                        {"activity", def.activities[static_cast<std::size_t>(r.activity)].id}, {"until", r.until}, {"money", r.money},
                                        {"xp", r.xp}, {"level", r.level}, {"crisis", r.crisis}, {"needs", needs}, {"bonds", bonds}});
        }
        out["events"] = json::array();
        for (const TownEvent &e : state.events)
            out["events"].push_back({e.tick, static_cast<int>(e.kind), e.who, e.other, e.what});
        // S22: what is remembered, what residents think of the player, favours.
        out["facts"] = json::array();
        for (const Fact &f : state.facts)
            out["facts"].push_back({f.tick, static_cast<int>(f.kind), f.who, f.other, f.what, std::to_string(f.saw), std::to_string(f.knows), std::to_string(f.told)});
        out["regard"] = state.regard;
        out["said"] = json::array();
        for (const SaidLine &line : state.said)
            out["said"].push_back({line.tick, line.speaker, line.text});
        out["favours"] = json::array();
        for (const FavourState &f : state.favours)
            out["favours"].push_back({static_cast<int>(f.status), f.due});
        return out;
    }

    bool LoadTownState(const TownDef &def, const json &saved, TownState &out, std::vector<core::Diagnostic> &diagnostics)
    {
        const auto fail = [&](const std::string &code, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "town.state." + code, "", "", message});
            return false;
        };
        try
        {
            if (!saved.is_object() || saved.value("format", "") != "umbra.town.state" || saved.value("version", 0) != 1)
                return fail("format", "not a saved town");
            if (saved.at("town").get<std::string>() != std::to_string(def.fingerprint))
                return fail("town", "this save is from a different town file (its needs, places, activities or residents changed)");
            TownState state;
            state.seed = std::stoull(saved.at("seed").get<std::string>());
            state.random = std::stoull(saved.at("random").get<std::string>());
            state.tick = saved.at("tick").get<std::int32_t>();
            state.minted = saved.at("minted").get<std::int64_t>();
            const std::size_t count = def.residents.size();
            if (saved.at("residents").size() != count || state.random == 0 || state.tick < 0)
                return fail("residents", "the saved town does not fit this town file");
            state.residents.resize(count);
            state.bonds.assign(count * count, 0);
            for (std::size_t i = 0; i < count; ++i)
            {
                const json &from = saved["residents"][i];
                ResidentState &r = state.residents[i];
                const int place = def.Place(from.at("place").get<std::string>()), activity = def.Activity(from.at("activity").get<std::string>());
                if (from.at("id").get<std::string>() != def.residents[i].id || place < 0 || activity < 0)
                    return fail("resident", "resident " + std::to_string(i) + " names something this town does not have");
                r.place = static_cast<std::int16_t>(place);
                r.activity = static_cast<std::int16_t>(activity);
                r.until = from.at("until").get<std::int32_t>();
                r.money = from.at("money").get<std::int32_t>();
                r.xp = from.at("xp").get<std::int32_t>();
                r.level = from.at("level").get<std::int16_t>();
                r.crisis = from.at("crisis").get<std::uint16_t>();
                for (std::size_t n = 0; n < def.needs.size(); ++n)
                    r.needs[n] = std::clamp(from.at("needs").at(def.needs[n].id).get<std::int32_t>(), 0, needFull * ticksPerDay);
                for (const auto &[other, value] : from.at("bonds").items())
                {
                    const int index = def.Resident(other);
                    if (index < 0)
                        return fail("bond", "a bond names someone who does not live here");
                    state.bonds[i * count + static_cast<std::size_t>(index)] = static_cast<std::int16_t>(std::clamp(value.get<int>(), -1000, 1000));
                }
            }
            for (const json &e : saved.at("events"))
                state.events.push_back({e.at(0).get<std::int32_t>(), static_cast<EventKind>(e.at(1).get<int>()), e.at(2).get<std::int16_t>(),
                                        e.at(3).get<std::int16_t>(), e.at(4).get<std::int32_t>()});
            if (state.events.size() > TownState::maxEvents)
                return fail("events", "too many events");
            // S22 parts: a save from before them has none (nobody remembers anything yet).
            state.regard.assign(count, 0);
            state.favours.assign(def.favours.size(), {});
            if (saved.contains("facts"))
            {
                for (const json &f : saved.at("facts"))
                    state.facts.push_back({f.at(0).get<std::int32_t>(), static_cast<FactKind>(f.at(1).get<int>()), f.at(2).get<std::int16_t>(), f.at(3).get<std::int16_t>(),
                                           f.at(4).get<std::int32_t>(), std::stoull(f.at(5).get<std::string>()), std::stoull(f.at(6).get<std::string>()),
                                           std::stoull(f.at(7).get<std::string>())});
                state.regard = saved.at("regard").get<std::vector<std::int16_t>>();
                for (std::size_t i = 0; i < saved.at("favours").size() && i < state.favours.size(); ++i)
                    state.favours[i] = {static_cast<FavourStatus>(saved["favours"][i].at(0).get<int>()), saved["favours"][i].at(1).get<std::int32_t>()};
                if (saved.contains("said"))
                    for (const json &line : saved.at("said"))
                        if (const std::int16_t speaker = line.at(1).get<std::int16_t>(); speaker >= 0 && static_cast<std::size_t>(speaker) < count && state.said.size() < TownState::maxSaid)
                            state.said.push_back({line.at(0).get<std::int32_t>(), speaker, line.at(2).get<std::string>()});
                if (state.facts.size() > TownState::maxFacts || state.regard.size() != count)
                    return fail("facts", "the saved memories do not fit this town");
            }
            out = std::move(state);
            return true;
        }
        catch (const std::exception &error)
        {
            return fail("read", std::string("the saved town is damaged: ") + error.what());
        }
    }
}
