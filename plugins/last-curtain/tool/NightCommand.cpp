// UmbraTool night-check, night-run, night-soak: the night of The Last Curtain without graphics
// (plan 1; Balance.md). Every command reads the data folder, by default the plugin's own.

#include <ToolCommand.hpp>

#include <NightBots.hpp>
#include <NightSave.hpp>
#include <NightSoak.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <iostream>

namespace
{
    using namespace umbra::night;

    std::filesystem::path DataFolder(const umbra::tool::Arguments &args)
    {
        if (args.Has("data"))
            return args.Path("data");
        if (!args.positional.empty())
            return args.positional[0];
        return std::filesystem::path(UMBRA_NIGHT_DEFAULT_DATA);
    }

    int NightCheck(const umbra::tool::Arguments &args)
    {
        std::vector<umbra::core::Diagnostic> diagnostics;
        NightDef def;
        LoadNight(DataFolder(args), def, diagnostics);
        return umbra::tool::Report(diagnostics, 11, args.json);
    }

    // One line of the night's story, from a log entry.
    std::string Line(const NightDef &def, const NightState &state, const LogEntry &e)
    {
        auto who = [&](int g) { return g >= 0 && g < state.count ? def.guests[static_cast<std::size_t>(g)].name : std::string("a monster"); };
        const int round = e.tick / 8, minutes = (round - 1) * 30;
        char clock[8];
        std::snprintf(clock, sizeof clock, "%02d:%02d", (def.rules.clock.startHour + minutes / 60) % 24, minutes % 60);
        std::string text = std::string(clock) + "  ";
        switch (e.kind)
        {
        case LogKind::Opened: return text + who(e.who) + " opens " + def.rooms[static_cast<std::size_t>(state.cells[static_cast<std::size_t>(e.a)].room)].name;
        case LogKind::Check: return text + who(e.who) + " rolls " + std::to_string(e.a) + " + " + std::to_string(e.other) + " against " + std::to_string(e.b) + (e.a + e.other >= e.b ? ": passed" : ": failed");
        case LogKind::Item: return text + who(e.who) + " finds " + def.items[static_cast<std::size_t>(e.a)].name;
        case LogKind::Down: return text + who(e.who) + " is down" + (e.other >= 0 ? " (by " + who(e.other) + ")" : "");
        case LogKind::Woke: return text + who(e.who) + " is woken by " + who(e.other);
        case LogKind::Battle: return text + "battle: " + who(e.who) + " against " + who(e.other);
        case LogKind::Omen: return text + who(e.who) + " takes " + def.omens[static_cast<std::size_t>(e.a)].name + (e.b ? "" : ": the curse takes hold, time jumps to midnight");
        case LogKind::Turned: return text + "THE TURN: " + who(e.who) + " rises as " + def.guests[static_cast<std::size_t>(e.who)].taken;
        case LogKind::Power: return text + def.guests[static_cast<std::size_t>(e.who)].taken + " uses " + def.powers[static_cast<std::size_t>(def.guests[static_cast<std::size_t>(e.who)].power)].id;
        case LogKind::Died: return text + who(e.who) + " dies";
        case LogKind::Monster: return text + (e.other == 1 ? who(e.who) + " kills " : "appears: ") + def.monsters[static_cast<std::size_t>(e.a)].name;
        case LogKind::Level: return text + who(e.who) + " reaches level " + std::to_string(e.a);
        case LogKind::Persuade: return text + who(e.who) + (e.a ? " wins over " : " fails to win over ") + who(e.other);
        case LogKind::Ended:
        {
            static const char *ends[] = {"running", "the company destroys the Taken", "the Taken wins"};
            return text + "END: " + ends[e.a];
        }
        default: return {};
        }
    }

    int NightRun(const umbra::tool::Arguments &args)
    {
        std::vector<umbra::core::Diagnostic> diagnostics;
        NightDef def;
        if (!LoadNight(DataFolder(args), def, diagnostics))
            return umbra::tool::Report(diagnostics, 11, args.json);
        const int player = args.Has("player") ? NightDef::Find(def.guests, args.Text("player")) : -1;
        NightState state = NewNight(def, static_cast<std::uint64_t>(args.Long("seed", 1)), player);
        nlohmann::json lines = nlohmann::json::array();
        int applies = 0;
        while (Current(state) >= 0 && applies++ < 5000)
        {
            const std::size_t since = state.logCount;
            Apply(def, state, ChooseAction(def, state, Current(state)));
            for (const LogEntry &e : NewLog(state, since))
                if (const std::string line = Line(def, state, e); !line.empty())
                    args.json ? lines.push_back(line) : void(std::cout << line << "\n");
        }
        if (args.json)
            std::cout << nlohmann::json{{"lines", lines}, {"fingerprint", Fingerprint(state)}}.dump(2) << "\n";
        return Result(state) == Outcome::Running ? 1 : 0;
    }

    int NightSoak(const umbra::tool::Arguments &args)
    {
        std::vector<umbra::core::Diagnostic> diagnostics;
        NightDef def;
        if (!LoadNight(DataFolder(args), def, diagnostics))
            return umbra::tool::Report(diagnostics, 11, args.json);
        const auto start = std::chrono::steady_clock::now();
        const SoakReport report = Soak(def, static_cast<std::uint64_t>(args.Long("seed", 1)), args.Int("nights", 1000), args.Int("strategy", 0));
        if (args.json)
            std::cout << Json(def, report).dump(2) << "\n";
        else
            std::cout << Text(def, report) << (report.Holds(def) ? "the data holds" : "THE DATA DOES NOT HOLD") << " ("
                      << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms)\n";
        return report.Holds(def) ? 0 : 1;
    }
}

UMBRA_TOOL_COMMAND(NightCheck, "night-check", "night-check [<data folder>] [--json]", NightCheck);
UMBRA_TOOL_COMMAND(NightRun, "night-run", "night-run [--data <folder>] [--seed 1] [--player <guest id>] [--json]", NightRun);
UMBRA_TOOL_COMMAND(NightSoak, "night-soak", "night-soak [--data <folder>] [--nights 1000] [--seed 1] [--strategy 0] [--json]", NightSoak);
