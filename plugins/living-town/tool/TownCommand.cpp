// UmbraTool town-run: lets a town live some days with nobody watching and prints what happened.

#include <ToolCommand.hpp>

#include <Town.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>

namespace
{
    using namespace umbra::town;
    using umbra::tool::Arguments;

    std::string Clock(int tick)
    {
        char text[32];
        std::snprintf(text, sizeof text, "day %d %02d:%02d", tick / ticksPerDay + 1, tick % ticksPerDay / ticksPerHour, tick % ticksPerHour * 10);
        return text;
    }

    // One event as a plain sentence (S22 gives these to residents' memories and the chronicle).
    std::string Sentence(const TownDef &def, const TownEvent &e)
    {
        const auto name = [&](int resident) { return resident >= 0 ? def.residents[static_cast<std::size_t>(resident)].name : std::string("nobody"); };
        switch (e.kind)
        {
        case EventKind::Started: return name(e.who) + ": " + def.activities[static_cast<std::size_t>(e.what)].name;
        case EventKind::Paid: return name(e.who) + " paid " + std::to_string(e.what) + (e.other >= 0 ? " to " + name(e.other) : "");
        case EventKind::Earned: return name(e.who) + " earned " + std::to_string(e.what);
        case EventKind::LevelUp: return name(e.who) + " reached level " + std::to_string(e.what);
        case EventKind::Crisis: return name(e.who) + " cannot bear it: " + def.needs[static_cast<std::size_t>(e.what)].name;
        case EventKind::Friends: return name(e.who) + " now counts " + name(e.other) + " as a friend";
        case EventKind::Rivals: return name(e.who) + " has fallen out with " + name(e.other);
        }
        return {};
    }

    int TownRun(const Arguments &args)
    {
        if (args.positional.size() != 1)
            return umbra::tool::Usage();
        // 1. The town, checked against the RPG content if one is given.
        std::vector<umbra::core::Diagnostic> diagnostics;
        umbra::rpg::Content content;
        const bool withContent = args.Has("content") && umbra::rpg::LoadContent(args.Path("content"), content, diagnostics);
        TownDef def;
        if (umbra::core::HasErrors(diagnostics) || !LoadTown(args.positional[0], withContent ? &content : nullptr, def, diagnostics))
            return umbra::tool::Report(diagnostics, 1, args.json);

        // 2. Live the days. Only the notable events are printed (not every change of activity).
        const int days = args.Int("days", 7);
        TownState state = NewTown(def, static_cast<std::uint64_t>(args.Long("seed", 1)));
        nlohmann::json diary = nlohmann::json::array();
        const auto start = std::chrono::steady_clock::now();
        for (int tick = 0; tick < days * ticksPerDay; ++tick)
        {
            const std::size_t before = state.events.size();
            const std::int32_t last = state.events.empty() ? -1 : state.events.back().tick;
            Tick(def, state);
            // New events are at the end (the list drops old ones from the front when it is full).
            for (std::size_t i = 0; i < state.events.size(); ++i)
            {
                const TownEvent &e = state.events[i];
                const bool fresh = e.tick == tick && (e.tick > last || i >= before);
                if (!fresh || e.kind == EventKind::Started || e.kind == EventKind::Paid || e.kind == EventKind::Earned)
                    continue;
                if (args.json)
                    diary.push_back({{"tick", e.tick}, {"text", Sentence(def, e)}});
                else
                    std::cout << Clock(e.tick) << "  " << Sentence(def, e) << "\n";
            }
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        // 3. Where everyone ended up.
        nlohmann::json people = nlohmann::json::array();
        for (std::size_t i = 0; i < def.residents.size(); ++i)
        {
            const ResidentState &r = state.residents[i];
            nlohmann::json needs;
            for (std::size_t n = 0; n < def.needs.size(); ++n)
                needs[def.needs[n].id] = state.Need(i, n);
            people.push_back({{"name", def.residents[i].name}, {"money", r.money}, {"level", r.level}, {"place", def.places[static_cast<std::size_t>(r.place)].name},
                              {"doing", def.activities[static_cast<std::size_t>(r.activity)].name}, {"needs", needs}});
            if (!args.json)
                std::cout << def.residents[i].name << ": " << r.money << " coins, level " << r.level << ", " << def.activities[static_cast<std::size_t>(r.activity)].name
                          << " at " << def.places[static_cast<std::size_t>(r.place)].name << "\n";
        }
        // 4. What is remembered, and by how many (--facts).
        if (args.Has("facts") || args.positional.size() > 1)
            for (const Fact &f : state.facts)
            {
                int knows = 0;
                for (std::size_t i = 0; i < def.residents.size(); ++i)
                    knows += (f.knows >> i) & 1U;
                std::cout << Clock(f.tick) << "  " << Sentence(def, f) << "  (known to " << knows << ")\n";
            }
        const std::string print = std::to_string(Fingerprint(state));
        const double daysPerSecond = seconds > 0.0 ? days / seconds : 0.0;
        if (args.json)
            std::cout << nlohmann::json{{"days", days}, {"seed", std::to_string(state.seed)}, {"fingerprint", print}, {"daysPerSecond", daysPerSecond},
                                        {"residents", people}, {"diary", diary}}
                             .dump(2)
                      << "\n";
        else
            std::cout << days << " days, seed " << state.seed << ", fingerprint " << print << ", " << static_cast<long long>(daysPerSecond) << " days per second\n";
        return 0;
    }
}

UMBRA_TOOL_COMMAND(TownRun, "town-run", "town-run <town file> [--days 7] [--seed 1] [--content <rpg file>] [--json] [--facts 1]", TownRun);
