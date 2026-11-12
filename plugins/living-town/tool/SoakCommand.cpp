// UmbraTool town-soak: lives one town and its outcome plan on many seeds and reports what came
// of the plot (S24, Storyteller.md "The soak test"). Exit code 1 when the plan did not hold.

#include <ToolCommand.hpp>

#include <Soak.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>

namespace
{
    using namespace umbra::town;

    int TownSoak(const umbra::tool::Arguments &args)
    {
        if (args.positional.size() != 2)
            return umbra::tool::Usage();
        std::vector<umbra::core::Diagnostic> diagnostics;
        TownDef def;
        PlanDef plan;
        if (!LoadTown(args.positional[0], nullptr, def, diagnostics) || !LoadPlan(args.positional[1], def, plan, diagnostics))
            return umbra::tool::Report(diagnostics, 1, args.json);
        // Long enough for the last beat's window, unless told otherwise.
        int days = 7;
        for (const BeatDef &beat : plan.beats)
            days = std::max(days, beat.to + 1);
        const auto start = std::chrono::steady_clock::now();
        const SoakReport report = Soak(def, plan, static_cast<std::uint64_t>(args.Long("seed", 1)), args.Int("seeds", 1000), args.Int("days", days));
        if (args.json)
            std::cout << Json(plan, report).dump(2) << "\n";
        else
            std::cout << Text(plan, report) << (report.Holds() ? "the plan holds" : "THE PLAN DOES NOT HOLD") << " ("
                      << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms)\n";
        return report.Holds() ? 0 : 1;
    }
}

UMBRA_TOOL_COMMAND(TownSoak, "town-soak", "town-soak <town file> <plan file> [--seeds 1000] [--days <last window + 1>] [--seed 1] [--json]", TownSoak);
