// UmbraTool fight: the conventional AI plays both sides of a turn-based fight (a quick balance check).

#include <ToolCommand.hpp>

#include <RpgContent.hpp>
#include <TurnEnvironment.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using umbra::core::Diagnostic;
    using umbra::core::Severity;
    using umbra::tool::Arguments;
    using umbra::tool::Report;
    using umbra::tool::Usage;
    using Json = nlohmann::json;

    // A turn fight with the conventional AI on both sides: a quick balance check for content.
    int Fight(const Arguments &args)
    {
        if (args.positional.size() < 3)
            return Usage();
        umbra::rpg::Content content;
        std::vector<Diagnostic> diagnostics;
        if (!umbra::rpg::LoadContent(args.positional[0], content, diagnostics))
            return Report(diagnostics, 1, args.json);

        // 1. The hero is team 0, every enemy team 1.
        std::vector<umbra::turn::ScenarioSlot> slots;
        for (std::size_t i = 1; i < args.positional.size(); ++i)
            slots.push_back({args.positional[i], args.positional[i] + "_" + std::to_string(i), i == 1 ? 0 : 1});
        umbra::turn::TurnEnvironment fight(content, slots);
        if (!fight.Reset())
        {
            std::cerr << "unknown actor (or its starting inventory doesn't fit)\n";
            return 1;
        }
        // 2. Step until someone wins (or a generous limit).
        umbra::turn::StepResult step;
        while (!step.done && fight.Steps() < 10000)
            step = fight.Step(umbra::turn::ChooseTurnAction(content, *fight.Encounter()));

        // 3. Result.
        Json result{{"finished", step.done}, {"winner", step.winner}, {"rounds", fight.Round()}, {"steps", fight.Steps()}, {"fighters", Json::array()}};
        for (const umbra::rpg::Character &c : fight.Characters())
            result["fighters"].push_back({{"id", c.id}, {"health", c.health}, {"energy", c.energy}});
        if (args.json)
            std::cout << result.dump(2) << "\n";
        else
        {
            std::cout << (step.done ? (step.winner == 0 ? "Hero side wins" : "Enemy side wins") : "No result") << " after "
                      << fight.Round() << " round(s), " << fight.Steps() << " action(s)\n";
            for (const umbra::rpg::Character &c : fight.Characters())
                std::cout << "  " << c.id << ": " << c.health << " HP, " << c.energy << " EN\n";
        }
        return step.done ? 0 : 1;
    }
}

UMBRA_TOOL_COMMAND(Fight, "fight", "fight    <rpg file> <hero actor> <enemy actor>... [--json]", Fight);
