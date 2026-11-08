#pragma once

#include <AgentTools.hpp>
#include <Ollama.hpp>

#include <atomic>
#include <future>
#include <string>
#include <vector>

// The agent loop (roadmap S5, ADR 0008): prompt -> local model plans with the tools -> the user
// approves -> the steps run -> one undo step. Both classes are driven by Tick() from one thread
// (the editor's frame), so documents are only touched there. See Agent.md.
namespace umbra::agent
{
    struct Step final
    {
        std::string tool;
        Json args;
        std::string why;
    };

    struct Plan final
    {
        std::string summary;
        std::vector<Step> steps;
    };

    struct StepOutcome final
    {
        enum class State
        {
            Waiting,
            Checked,   // dry run passed
            Later,     // checked when it runs (a generator, or uses a generated file)
            Running,
            Done,
            Failed,
            Skipped    // an earlier step failed
        };
        State state = State::Waiting;
        ToolResult result;
    };
    [[nodiscard]] const char *ToString(StepOutcome::State state);

    // Replaces argument strings "$<n>" with the value of step n (1-based). "" if fine, else why not.
    std::string FillReferences(Json &args, const std::vector<StepOutcome> &outcomes, std::size_t step);

    // Runs the plan on throwaway working copies (generators are not run). The plan is ready when
    // no step failed.
    [[nodiscard]] std::vector<StepOutcome> CheckPlan(Workspace &ws, const Plan &plan);
    [[nodiscard]] bool Passed(const std::vector<StepOutcome> &outcomes);

    struct PlannerSettings final
    {
        std::string host = "http://localhost:11434"; // Ollama, this computer only
        std::string model = "qwen3:8b";
        int maxLooks = 4;  // rounds of read tools before the plan
        int maxFixes = 2;  // times a broken plan is sent back with the reason
        int timeoutSeconds = 600;
    };

    // Prompt -> plan. The model may first ask for read tools ("look"); their results are sent
    // back. A plan with an unknown tool, bad arguments or a step that fails the dry run is sent
    // back with the reason. The JSON answer is forced by a schema (reliable on small models).
    class Planner final
    {
    public:
        bool Start(Workspace &ws, const std::string &prompt, PlannerSettings settings);
        bool Tick(); // true once finished (also when it failed)
        void Cancel();
        [[nodiscard]] bool Running() const noexcept { return running_; }
        [[nodiscard]] std::string Status() const;

        // After it finished:
        bool ok = false;
        Plan plan;
        std::vector<StepOutcome> check;            // the dry run of `plan`
        std::vector<std::string> log;              // what happened: looks, fixes
        std::vector<core::Diagnostic> diagnostics; // why it failed

    private:
        bool Ask();
        bool Answer(const std::string &text); // true when finished
        void Finish(bool success);

        Workspace *ws_ = nullptr;
        PlannerSettings settings_;
        localai::ChatRequest request_;
        localai::ChatJob job_;
        int looks_ = 0;
        int fixes_ = 0;
        bool running_ = false;
    };

    // Runs an approved plan: read and change steps at once, generators on a worker thread. A
    // failed step stops the rest and nothing is committed; otherwise all document changes land as
    // one undo step per document labelled `label`. Generated files stay (they are new files).
    class Runner final
    {
    public:
        Runner() = default;
        Runner(const Runner &) = delete;
        Runner &operator=(const Runner &) = delete;
        ~Runner();

        bool Start(Workspace &ws, Plan plan, std::string label);
        bool Tick(); // true once finished
        void Cancel();
        [[nodiscard]] bool Running() const noexcept { return running_; }

        std::vector<StepOutcome> outcomes;
        bool committed = false;
        std::vector<authoring::Document *> changed; // tell the editor: Ctrl+Z undoes them
        std::vector<core::Diagnostic> diagnostics;

    private:
        void Fail(std::size_t step);
        Workspace *ws_ = nullptr;
        Plan plan_;
        std::string label_;
        Edits edits_;
        std::size_t next_ = 0;
        std::future<ToolResult> slow_;
        std::atomic<bool> stop_ = false;
        bool running_ = false;
    };

    // The same, waiting (UmbraTool, tests).
    Planner &PlanNow(Planner &planner, Workspace &ws, const std::string &prompt, const PlannerSettings &settings);
    Runner &RunNow(Runner &runner, Workspace &ws, const Plan &plan, const std::string &label);
}
