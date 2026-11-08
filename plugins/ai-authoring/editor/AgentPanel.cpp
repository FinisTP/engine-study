#include <Agent.hpp>
#include <EditorPanel.hpp>
#include <QuestAssistant.hpp>

#include <Platform.hpp>

#include <imgui.h>

#include <cstdio>
#include <fstream>

// Agent window (roadmap S5, ADR 0008). The creator asks for something ("put a well with a creaky
// sound next to the elder"); a local Ollama model plans it with the agent tools; the plan is
// checked on working copies and shown; nothing changes until "Approve and run". All document
// changes are then one undo step. Generated files (sounds, textures, models) are new files.
// How it works: plugins/ai-authoring/modules/agent/include/Agent.md.

using umbra::agent::Json;
using umbra::editor::EditorHost;
using State = umbra::agent::StepOutcome::State;

namespace
{
    std::filesystem::path SettingsFile() { return umbra::platform::Platform::PreferencesFolder() / "agent.json"; }

    ImVec4 Colour(State state)
    {
        switch (state)
        {
        case State::Checked:
        case State::Done: return {0.5F, 1, 0.5F, 1};
        case State::Failed: return {1, 0.4F, 0.4F, 1};
        case State::Later:
        case State::Running: return {1, 0.8F, 0.3F, 1};
        default: return {0.7F, 0.7F, 0.7F, 1};
        }
    }

    class AgentPanel final : public umbra::editor::EditorPanel
    {
    public:
        AgentPanel() { LoadSettings(); }
        const char *Title() const override { return "Agent"; }
        void Draw(EditorHost &host) override;
        void Tick(EditorHost &host) override;
        void Leave() override
        {
            planner_.Cancel();
            runner_.Cancel();
        }

    private:
        void LoadSettings();
        void Bind(EditorHost &host); // points the workspace at the editor's documents
        void Steps(const std::vector<umbra::agent::StepOutcome> &outcomes);

        umbra::agent::Workspace ws_;
        umbra::agent::Planner planner_;
        umbra::agent::Runner runner_;
        umbra::agent::PlannerSettings settings_;
        std::string prompt_;
        std::vector<std::string> models_;
        std::vector<umbra::core::Diagnostic> modelProblems_;
        bool modelsChecked_ = false;
        bool planned_ = false; // a plan is shown
        bool ran_ = false;     // the plan was run (results shown)
    };
}

UMBRA_EDITOR_PANEL(AgentPanel);

void AgentPanel::LoadSettings()
{
    std::ifstream in(SettingsFile());
    const Json settings = Json::parse(in, nullptr, false);
    if (settings.is_object())
    {
        settings_.model = settings.value("model", settings_.model);
        settings_.host = settings.value("host", settings_.host);
    }
}

void AgentPanel::Bind(EditorHost &host)
{
    // In the editor the agent changes the open scene and the RPG content, as the user would.
    ws_.assets = host.AssetRoot();
    ws_.rpg = &host.Rpg();
    ws_.scene = [&host](const std::string &path, std::vector<umbra::core::Diagnostic> &diagnostics) -> umbra::authoring::Document * {
        umbra::authoring::Document *open = host.Scene();
        const std::string openPath = open ? std::filesystem::relative(open->File(), host.AssetRoot().Project()).generic_string() : "";
        if (open && (path.empty() || path == openPath))
            return open;
        diagnostics.push_back({umbra::core::Severity::Error, "agent.scene", path, "scene",
                               open ? "only the open scene (" + openPath + ") can be changed here; open " + path + " first" : "open a scene first"});
        return nullptr;
    };
}

void AgentPanel::Tick(EditorHost &host)
{
    // Runs every frame, open or not, so a plan or a long generation keeps going.
    Bind(host);
    if (planner_.Running() && planner_.Tick())
        planned_ = true;
    if (runner_.Running() && runner_.Tick())
    {
        ran_ = true;
        for (umbra::authoring::Document *doc : runner_.changed)
            host.Edited(*doc);
        host.RescanAssets(); // generated files
        if (runner_.committed)
            host.Message("Agent: done. Ctrl+Z undoes it; Save keeps it.", 6.0F);
        else
            host.Report(runner_.diagnostics, "Agent stopped; nothing was changed");
    }
}

void AgentPanel::Steps(const std::vector<umbra::agent::StepOutcome> &outcomes)
{
    const auto &steps = planner_.plan.steps;
    if (!ImGui::BeginTable("steps", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        return;
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24);
    ImGui::TableSetupColumn("Step");
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableHeadersRow();
    for (std::size_t i = 0; i < steps.size(); ++i)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%zu", i + 1);
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s %s", steps[i].tool.c_str(), steps[i].args.dump().c_str());
        if (!steps[i].why.empty())
            ImGui::TextDisabled("%s", steps[i].why.c_str());
        const State state = i < outcomes.size() ? outcomes[i].state : State::Waiting;
        if (i < outcomes.size() && !outcomes[i].result.text.empty() && state != State::Later)
            ImGui::TextWrapped("-> %s", outcomes[i].result.text.substr(0, 400).c_str());
        ImGui::TableNextColumn();
        ImGui::TextColored(Colour(state), "%s", umbra::agent::ToString(state));
    }
    ImGui::EndTable();
}

void AgentPanel::Draw(EditorHost &host)
{
    ImGui::SetNextWindowSize(ImVec2(680, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Title(), &open))
    {
        ImGui::End();
        return;
    }
    if (!modelsChecked_)
    {
        // Which models Ollama has (local, answers in milliseconds).
        modelProblems_.clear();
        models_ = umbra::localai::ListModels(settings_.host, modelProblems_);
        settings_.model = umbra::localai::PreferredModel(models_, settings_.model);
        modelsChecked_ = true;
    }

    // 1. The request.
    ImGui::TextWrapped("Ask for a change to the open scene or the RPG content: place things, add components, music or sounds, items. "
                       "A local AI model plans it; you see the plan before anything changes.");
    char buffer[2048];
    std::snprintf(buffer, sizeof(buffer), "%s", prompt_.c_str());
    if (ImGui::InputTextMultiline("##prompt", buffer, sizeof(buffer), ImVec2(-1, 70)))
        prompt_ = buffer;
    if (ImGui::BeginCombo("Model", settings_.model.c_str()))
    {
        for (const std::string &model : models_)
            if (ImGui::Selectable(model.c_str(), model == settings_.model))
                settings_.model = model;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh"))
        modelsChecked_ = false;
    for (const auto &d : modelProblems_)
        ImGui::TextColored({1, 0.4F, 0.4F, 1}, "%s", d.message.c_str());
    if (models_.empty())
        ImGui::TextWrapped("Install Ollama (ollama.com), then in a terminal: ollama pull qwen3:8b");

    const bool busy = planner_.Running() || runner_.Running();
    if (planner_.Running())
    {
        ImGui::TextUnformatted(planner_.Status().c_str());
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            planner_.Cancel();
    }
    else if (!runner_.Running())
    {
        ImGui::BeginDisabled(prompt_.empty() || busy);
        if (ImGui::Button(planned_ ? "Plan again" : "Plan"))
        {
            Bind(host);
            planned_ = ran_ = false;
            planner_.Start(ws_, prompt_, settings_);
            planned_ = true; // shows the progress, and why if it fails at once
            std::ofstream(SettingsFile()) << Json{{"model", settings_.model}, {"host", settings_.host}}.dump(2);
        }
        ImGui::EndDisabled();
    }
    ImGui::Separator();

    // 2. The plan, its dry run, then the run.
    if (planned_)
    {
        for (const std::string &line : planner_.log)
            ImGui::TextDisabled("%s", line.substr(0, 200).c_str());
        for (const auto &d : planner_.diagnostics)
            ImGui::TextColored({1, 0.4F, 0.4F, 1}, "%s", d.message.c_str());
        if (!planner_.plan.summary.empty())
            ImGui::TextWrapped("Plan: %s", planner_.plan.summary.c_str());
        Steps(ran_ || runner_.Running() ? runner_.outcomes : planner_.check);
        if (runner_.Running())
        {
            ImGui::TextUnformatted("Running... (a generator may take minutes)");
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
                runner_.Cancel();
        }
        else if (planner_.ok && !ran_ && !planner_.plan.steps.empty())
        {
            if (ImGui::Button("Approve and run"))
                runner_.Start(ws_, planner_.plan, "Agent: " + prompt_.substr(0, 40));
            ImGui::SameLine();
            if (ImGui::Button("Discard"))
                planned_ = false;
        }
        else if (ran_)
            ImGui::TextWrapped(runner_.committed ? "Done: one undo step (Ctrl+Z). Save (Ctrl+S) keeps it." : "Stopped at the failed step; nothing was changed. Generated files stay in the asset folder.");
    }
    ImGui::End();
}
