#include "Agent.hpp"

#include <chrono>
#include <thread>

namespace umbra::agent
{
    namespace
    {
        using core::Severity;
        using State = StepOutcome::State;

        const char *systemPrompt =
            "You are the Umbra editor agent. You change a 3D RPG project only through the tools listed below.\n"
            "Always answer with JSON: {\"look\": [{\"tool\", \"args\"}], \"summary\": \"...\", \"steps\": [{\"tool\", \"args\", \"why\"}]}.\n"
            "- To read more first, list read tools in \"look\" and leave \"steps\" empty; you get their results and answer again.\n"
            "- When you know enough, give the whole plan in \"steps\" with \"look\" empty. The user approves it before anything changes.\n"
            "- A step can use an earlier step's result by writing \"$<step number>\" (1-based) as an argument value, e.g. a generated sound path or a new entity id.\n"
            "- Positions are meters, y is up. Use only entity ids and asset paths that exist or that earlier steps make.\n"
            "- Keep plans short; do only what was asked. If it can't be done with these tools, give no steps and say why in \"summary\".";

        // One line per tool: name(arg*: type, ...) [kind]: description. * = required.
        std::string ToolList()
        {
            std::string text;
            for (const Tool &tool : Tools())
            {
                std::string args;
                for (const auto &[name, p] : tool.schema["properties"].items())
                {
                    bool required = false;
                    for (const Json &r : tool.schema["required"])
                        required |= r == name;
                    args += (args.empty() ? "" : ", ") + name + (required ? "*" : "") + (p.contains("type") ? ": " + p["type"].get<std::string>() : "");
                    if (p.contains("enum"))
                        args += " " + p["enum"].dump();
                }
                const char *kind = tool.kind == Tool::Kind::Read ? "read" : tool.kind == Tool::Kind::Change ? "change" : "generate, slow";
                text += tool.name + "(" + args + ") [" + kind + "]: " + tool.description + "\n";
            }
            return text;
        }

        // The answer's shape, forced by Ollama's structured output; tool names are an enum.
        Json AnswerSchema()
        {
            Json names = Json::array();
            for (const Tool &tool : Tools())
                names.push_back(tool.name);
            const Json call = {{"type", "object"},
                               {"properties", {{"tool", {{"type", "string"}, {"enum", names}}}, {"args", {{"type", "object"}}}, {"why", {{"type", "string"}}}}},
                               {"required", {"tool", "args"}}};
            return {{"type", "object"},
                    {"properties", {{"look", {{"type", "array"}, {"items", call}}}, {"summary", {{"type", "string"}}}, {"steps", {{"type", "array"}, {"items", call}}}}},
                    {"required", {"look", "summary", "steps"}}};
        }

        // Runs a tool by name, checking its arguments first.
        ToolResult Call(Workspace &ws, Edits &edits, const std::string &name, const Json &args)
        {
            const Tool *tool = FindTool(name);
            if (!tool)
                return {false, "no tool named " + name, "", {}};
            if (const std::string problem = CheckArguments(*tool, args); !problem.empty())
                return {false, name + ": " + problem, "", {}};
            return tool->run(ws, edits, args);
        }

        bool UsesGenerated(const Json &value, const Plan &plan, std::size_t step)
        {
            if (value.is_string() && value.get<std::string>().starts_with("$"))
            {
                const std::size_t n = std::strtoul(value.get<std::string>().c_str() + 1, nullptr, 10);
                if (n >= 1 && n <= step)
                {
                    const Tool *tool = FindTool(plan.steps[n - 1].tool);
                    return (tool && tool->kind == Tool::Kind::Generate) || UsesGenerated(plan.steps[n - 1].args, plan, n - 1);
                }
            }
            if (value.is_structured())
                for (const Json &item : value)
                    if (UsesGenerated(item, plan, step))
                        return true;
            return false;
        }
    }

    const char *ToString(StepOutcome::State state)
    {
        switch (state)
        {
        case State::Waiting: return "waiting";
        case State::Checked: return "checked";
        case State::Later: return "checked when it runs";
        case State::Running: return "running";
        case State::Done: return "done";
        case State::Failed: return "failed";
        case State::Skipped: return "skipped";
        }
        return "?";
    }

    std::string FillReferences(Json &args, const std::vector<StepOutcome> &outcomes, std::size_t step)
    {
        if (args.is_string() && args.get<std::string>().starts_with("$"))
        {
            const std::string text = args;
            char *end = nullptr;
            const std::size_t n = std::strtoul(text.c_str() + 1, &end, 10);
            if (*end != '\0' || n < 1 || n > step || n > outcomes.size())
                return text + " must name an earlier step (1.." + std::to_string(step) + ")";
            if (outcomes[n - 1].result.value.empty())
                return "step " + std::to_string(n) + " gives no value to use as " + text;
            args = outcomes[n - 1].result.value;
            return "";
        }
        if (args.is_structured())
            for (Json &item : args)
                if (std::string problem = FillReferences(item, outcomes, step); !problem.empty())
                    return problem;
        return "";
    }

    std::vector<StepOutcome> CheckPlan(Workspace &ws, const Plan &plan)
    {
        std::vector<StepOutcome> outcomes(plan.steps.size());
        Edits scratch; // thrown away
        for (std::size_t i = 0; i < plan.steps.size(); ++i)
        {
            const Step &step = plan.steps[i];
            const Tool *tool = FindTool(step.tool);
            StepOutcome &out = outcomes[i];
            // 1. Known tool, fitting arguments, references to earlier steps only.
            Json args = step.args;
            std::string problem = !tool ? "no tool named " + step.tool : CheckArguments(*tool, args);
            if (problem.empty() && (tool->kind == Tool::Kind::Generate || UsesGenerated(args, plan, i)))
            {
                out.state = State::Later; // can't be tried before the file exists
                out.result.ok = true;
                out.result.value = "$" + std::to_string(i + 1);
                continue;
            }
            if (problem.empty())
                problem = FillReferences(args, outcomes, i);
            if (!problem.empty())
            {
                out.state = State::Failed;
                out.result.text = problem;
                continue;
            }
            // 2. Try it on the working copies.
            out.result = tool->run(ws, scratch, args);
            out.state = out.result.ok ? State::Checked : State::Failed;
        }
        return outcomes;
    }

    bool Passed(const std::vector<StepOutcome> &outcomes)
    {
        for (const StepOutcome &o : outcomes)
            if (o.state == State::Failed)
                return false;
        return true;
    }

    // ---------------- Planner ----------------

    bool Planner::Start(Workspace &ws, const std::string &prompt, PlannerSettings settings)
    {
        if (running_)
            return false;
        ws_ = &ws;
        settings_ = std::move(settings);
        ok = false;
        plan = {};
        check.clear();
        log.clear();
        diagnostics.clear();
        looks_ = fixes_ = 0;

        // What the model starts with: the tools, the open scene and the RPG ids, the request.
        Edits none;
        std::string context = "Tools:\n" + ToolList();
        if (const ToolResult scene = Call(ws, none, "scene_outline", Json::object()); scene.ok)
            context += "\nOpen scene entities: " + scene.text + "\n";
        if (const ToolResult rpg = Call(ws, none, "rpg_outline", Json::object()); rpg.ok)
            context += "\nRPG content ids: " + rpg.text + "\n";
        request_ = {};
        request_.host = settings_.host;
        request_.model = settings_.model;
        request_.format = AnswerSchema();
        request_.temperature = 0.2;
        request_.timeoutSeconds = settings_.timeoutSeconds;
        request_.messages = {{"system", systemPrompt}, {"user", context + "\nRequest: " + prompt}};
        running_ = true;
        return Ask();
    }

    bool Planner::Ask()
    {
        if (job_.Start(request_))
            return true;
        diagnostics.push_back({Severity::Error, "agent.busy", "agent", "", "the model is still answering"});
        Finish(false);
        return false;
    }

    void Planner::Cancel()
    {
        job_.Cancel();
    }

    std::string Planner::Status() const
    {
        if (!running_)
            return ok ? "Plan ready" : "Stopped";
        return "Asking " + settings_.model + "... " + std::to_string(static_cast<int>(job_.Seconds())) + " s, " + std::to_string(job_.Received()) + " characters" +
               (looks_ + fixes_ > 0 ? " (round " + std::to_string(looks_ + fixes_ + 1) + ")" : "");
    }

    bool Planner::Tick()
    {
        if (!running_)
            return true;
        auto result = job_.Take();
        if (!result)
            return false;
        if (!result->ok)
        {
            diagnostics = result->diagnostics;
            Finish(false);
            return true;
        }
        return Answer(result->text);
    }

    bool Planner::Answer(const std::string &text)
    {
        request_.messages.push_back({"assistant", text});
        // Sends `feedback` back for another round, or gives up when out of fixes.
        const auto retry = [this](const std::string &feedback, bool look) {
            log.push_back(feedback);
            if (!look && ++fixes_ > settings_.maxFixes)
            {
                diagnostics.push_back({Severity::Error, "agent.plan", "agent", "", "the model's plan is still wrong after " + std::to_string(settings_.maxFixes) + " fixes: " + feedback});
                Finish(false);
                return true;
            }
            request_.messages.push_back({"user", feedback});
            return !Ask();
        };

        const Json answer = Json::parse(text, nullptr, false);
        if (answer.is_discarded() || !answer.is_object())
            return retry("Your answer was not valid JSON. Answer again with the JSON object only.", false);
        plan.summary = answer.value("summary", std::string());
        const Json looks = answer.value("look", Json::array());
        const Json steps = answer.value("steps", Json::array());

        // 1. Look: run the read tools it asked for and send the results.
        if (steps.empty() && !looks.empty())
        {
            if (++looks_ > settings_.maxLooks)
                return retry("No more looking: give the plan now in \"steps\".", false);
            std::string results = "Results:\n";
            Edits none;
            for (const Json &call : looks)
            {
                const std::string name = call.value("tool", std::string());
                const Tool *tool = FindTool(name);
                const ToolResult r = tool && tool->kind != Tool::Kind::Read ? ToolResult{false, name + " is not a read tool; put it in steps", "", {}}
                                                                            : Call(*ws_, none, name, call.value("args", Json::object()));
                results += name + " " + call.value("args", Json::object()).dump() + " -> " + (r.ok ? "" : "ERROR ") + r.text + "\n";
            }
            log.push_back("looked: " + std::to_string(looks.size()) + " tool call(s)");
            return retry(results, true);
        }

        // 2. The plan: well-formed, then a dry run on working copies.
        plan.steps.clear();
        for (const Json &s : steps)
            plan.steps.push_back({s.value("tool", std::string()), s.value("args", Json::object()), s.value("why", std::string())});
        check = CheckPlan(*ws_, plan);
        std::string problems;
        for (std::size_t i = 0; i < check.size(); ++i)
            if (check[i].state == State::Failed)
                problems += "Step " + std::to_string(i + 1) + " (" + plan.steps[i].tool + "): " + check[i].result.text + "\n";
        if (!problems.empty())
            return retry("These steps fail; send the whole corrected plan:\n" + problems, false);
        Finish(true);
        return true;
    }

    void Planner::Finish(bool success)
    {
        ok = success;
        running_ = false;
    }

    // ---------------- Runner ----------------

    Runner::~Runner()
    {
        Cancel();
        if (slow_.valid())
            slow_.wait();
    }

    bool Runner::Start(Workspace &ws, Plan plan, std::string label)
    {
        if (running_)
            return false;
        ws_ = &ws;
        plan_ = std::move(plan);
        label_ = std::move(label);
        edits_ = {};
        outcomes.assign(plan_.steps.size(), {});
        committed = false;
        changed.clear();
        diagnostics.clear();
        next_ = 0;
        stop_ = false;
        ws_->stop = &stop_;
        running_ = true;
        return true;
    }

    void Runner::Cancel()
    {
        stop_ = true;
    }

    void Runner::Fail(std::size_t step)
    {
        outcomes[step].state = State::Failed;
        for (std::size_t i = step + 1; i < outcomes.size(); ++i)
            outcomes[i].state = State::Skipped;
        diagnostics.push_back({Severity::Error, "agent.step", "agent", "step " + std::to_string(step + 1), outcomes[step].result.text});
        running_ = false; // nothing is committed
        ws_->stop = nullptr;
    }

    bool Runner::Tick()
    {
        if (!running_)
            return true;
        // 1. A generator is still working: wait for it.
        if (slow_.valid())
        {
            if (slow_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                return false;
            outcomes[next_].result = slow_.get();
            if (!outcomes[next_].result.ok)
            {
                Fail(next_);
                return true;
            }
            outcomes[next_++].state = State::Done;
        }
        if (stop_ && next_ < outcomes.size())
        {
            outcomes[next_].result.text = "stopped";
            Fail(next_);
            return true;
        }
        // 2. Run quick steps until a slow one starts or the plan ends.
        while (next_ < outcomes.size())
        {
            const Step &step = plan_.steps[next_];
            StepOutcome &out = outcomes[next_];
            Json args = step.args;
            const Tool *tool = FindTool(step.tool);
            const std::string problem = !tool ? "no tool named " + step.tool : FillReferences(args, outcomes, next_);
            out.result.text = problem.empty() ? CheckArguments(*tool, args) : problem;
            if (!out.result.text.empty())
            {
                Fail(next_);
                return true;
            }
            out.state = State::Running;
            if (tool->kind == Tool::Kind::Generate)
            {
                // Generators only write new files, never the documents, so a worker thread is safe.
                slow_ = std::async(std::launch::async, [tool, ws = ws_, args] {
                    Edits none;
                    return tool->run(*ws, none, args);
                });
                return false;
            }
            out.result = tool->run(*ws_, edits_, args);
            if (!out.result.ok)
            {
                Fail(next_);
                return true;
            }
            out.state = State::Done;
            ++next_;
        }
        // 3. Every step worked: one undo step per changed document.
        committed = edits_.Empty() || edits_.Commit(label_, changed, diagnostics);
        running_ = false;
        ws_->stop = nullptr;
        return true;
    }

    Planner &PlanNow(Planner &planner, Workspace &ws, const std::string &prompt, const PlannerSettings &settings)
    {
        if (planner.Start(ws, prompt, settings))
            while (!planner.Tick())
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return planner;
    }

    Runner &RunNow(Runner &runner, Workspace &ws, const Plan &plan, const std::string &label)
    {
        if (runner.Start(ws, plan, label))
            while (!runner.Tick())
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return runner;
    }
}
