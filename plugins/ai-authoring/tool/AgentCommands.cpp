// UmbraTool agent and mcp (roadmap S5): the agent prompt without the editor, and an MCP server
// for external coding agents. See plugins/ai-authoring/modules/agent/include/Agent.md.

#include <ToolCommand.hpp>

#include <Agent.hpp>
#include <Mcp.hpp>
#include <Validate.hpp>

#include <cctype>
#include <cstdio>
#include <iostream>

namespace
{
    using umbra::agent::Json;
    using umbra::tool::Arguments;
    using umbra::tool::Usage;
    using State = umbra::agent::StepOutcome::State;

    void PrintSteps(const umbra::agent::Plan &plan, const std::vector<umbra::agent::StepOutcome> &outcomes)
    {
        for (std::size_t i = 0; i < plan.steps.size(); ++i)
        {
            const auto &o = outcomes[i];
            std::cout << i + 1 << ". " << plan.steps[i].tool << " " << plan.steps[i].args.dump() << "  [" << umbra::agent::ToString(o.state) << "]\n";
            if (!plan.steps[i].why.empty())
                std::cout << "   why: " << plan.steps[i].why << "\n";
            if (!o.result.text.empty() && o.state != State::Later)
                std::cout << "   -> " << o.result.text.substr(0, 300) << "\n";
        }
    }

    // A prompt -> a plan (checked on working copies) -> with --apply: run and save.
    int Agent(const Arguments &args)
    {
        if (args.positional.size() != 1 || !args.Has("assets"))
            return Usage();
        umbra::agent::FileWorkspace files(args.Assets());
        files.SetOpenScene(args.Text("scene"));
        umbra::agent::PlannerSettings settings;
        settings.model = args.Text("model", settings.model);
        settings.host = args.Text("host", settings.host);
        umbra::agent::Planner planner;
        std::cerr << "Planning with " << settings.model << "...\n";
        umbra::agent::PlanNow(planner, files.Get(), args.positional[0], settings);
        for (const std::string &line : planner.log)
            std::cerr << "  " << line.substr(0, 200) << "\n";
        if (!planner.ok)
            return umbra::tool::Report(planner.diagnostics, 0, args.json);
        std::cout << "Plan: " << planner.plan.summary << "\n";
        PrintSteps(planner.plan, planner.check);
        if (!args.apply)
        {
            std::cout << "Nothing changed. Add --apply to run it.\n";
            return 0;
        }
        umbra::agent::Runner runner;
        umbra::agent::RunNow(runner, files.Get(), planner.plan, "Agent: " + args.positional[0].substr(0, 40));
        std::cout << "Run:\n";
        PrintSteps(planner.plan, runner.outcomes);
        if (!runner.committed || !files.Save(runner.diagnostics))
            return umbra::tool::Report(runner.diagnostics, 0, args.json);
        std::cout << "Saved.\n";
        return 0;
    }

    // Runs a fixed command in the build folder; the client only picks build or tests and a filter.
    std::pair<bool, std::string> RunCommand(const std::string &command)
    {
#ifdef _WIN32
        FILE *pipe = _popen((command + " 2>&1").c_str(), "r");
#else
        FILE *pipe = popen((command + " 2>&1").c_str(), "r");
#endif
        if (!pipe)
            return {false, "couldn't start: " + command};
        std::string output;
        char buffer[4096];
        while (const std::size_t n = std::fread(buffer, 1, sizeof(buffer), pipe))
            output.append(buffer, n);
#ifdef _WIN32
        const int code = _pclose(pipe);
#else
        const int code = pclose(pipe);
#endif
        if (output.size() > 12000)
            output = "...\n" + output.substr(output.size() - 12000); // the end has the errors and the summary
        return {code == 0, command + "\n" + output};
    }

    // MCP over stdio. Only stdout carries the protocol; notes go to stderr.
    int Mcp(const Arguments &args)
    {
        if (!args.Has("assets"))
            return Usage();
        umbra::agent::FileWorkspace files(args.Assets());
        files.SetOpenScene(args.Text("scene"));
        umbra::agent::McpOptions options;
        if (args.Has("build"))
        {
            const std::string dir = args.Text("build"), config = args.Text("config", "Debug");
            options.command = [dir, config](const std::string &tool, const Json &input) {
                if (tool == "build")
                    return RunCommand("cmake --build \"" + dir + "\" --config " + config);
                std::string filter = input.value("filter", std::string());
                for (char c : filter)
                    if (!std::isalnum(static_cast<unsigned char>(c)) && std::string("._|*-").find(c) == std::string::npos)
                        return std::pair<bool, std::string>{false, "the filter may only use letters, digits and . _ | * -"};
                return RunCommand("ctest --test-dir \"" + dir + "\" -C " + config + " --output-on-failure" + (filter.empty() ? "" : " -R \"" + filter + "\""));
            };
        }
        std::cerr << "Umbra MCP server: " << args.Text("assets") << (args.Has("build") ? ", build " + args.Text("build") : "") << ". Waiting on stdin.\n";
        umbra::agent::RunMcp(files, options, std::cin, std::cout);
        return 0;
    }
}

UMBRA_TOOL_COMMAND(Agent, "agent", "agent \"<request>\" --assets <folder> [--scene <scene path>] [--apply] [--model <name>] [--host <url>]", Agent);
UMBRA_TOOL_COMMAND(Mcp, "mcp", "mcp --assets <folder> [--scene <scene path>] [--build <build folder> [--config Debug]]   (MCP server on stdio)", Mcp);
