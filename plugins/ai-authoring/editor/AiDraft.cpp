#include <EditorPanel.hpp>
#include <QuestAssistant.hpp>
#include <QuestDraft.hpp>

#include <Platform.hpp>

#include <imgui.h>

#include <cstdio>
#include <fstream>

// AI quest draft window (roadmap P8B). The creator describes a quest; a local Ollama model writes
// a draft; authoring::PlanDraft checks it and plays it through. Nothing changes until "Approve",
// which is one ordinary undoable edit of the RPG document. Rules in apps/editor/Editor.md.

using umbra::authoring::Json;
using umbra::editor::EditorHost;

namespace
{
    std::filesystem::path SettingsFile()
    {
        return umbra::platform::Platform::PreferencesFolder() / "ai-draft.json";
    }

    void InputString(const char *label, std::string &value, bool multiline = false)
    {
        char buffer[4096];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        const bool changed = multiline ? ImGui::InputTextMultiline(label, buffer, sizeof(buffer), ImVec2(-1, 70))
                                       : ImGui::InputText(label, buffer, sizeof(buffer));
        if (changed)
            value = buffer;
    }

    void Lines(const std::vector<umbra::core::Diagnostic> &diagnostics)
    {
        for (const umbra::core::Diagnostic &d : diagnostics)
        {
            const bool error = d.severity == umbra::core::Severity::Error;
            ImGui::PushStyleColor(ImGuiCol_Text, error ? ImVec4(1, 0.4F, 0.4F, 1) : ImVec4(1, 0.8F, 0.3F, 1));
            ImGui::TextWrapped("%s %s%s", error ? "Error:" : "Note:", d.field.empty() ? "" : (d.field + ": ").c_str(), d.message.c_str());
            ImGui::PopStyleColor();
        }
    }

    class AiDraftPanel final : public umbra::editor::EditorPanel
    {
    public:
        AiDraftPanel() { LoadSettings(); }
        const char *Title() const override { return "AI quest draft"; }
        void Draw(EditorHost &host) override;

    private:
        void LoadSettings();
        void RefreshModels();
        void Start(EditorHost &host, bool fix);
        void Plan(EditorHost &host);
        void Approve(EditorHost &host);

        // The request runs on a worker; the plan is checked against base_ (an RPG revision).
        std::unique_ptr<umbra::localai::ChatJob> job_ = std::make_unique<umbra::localai::ChatJob>();
        std::string idea_, npc_, model_ = "qwen2.5:7b", host_ = "http://localhost:11434";
        umbra::localai::ChatRequest request_;
        std::string answer_;
        umbra::authoring::DraftPlan plan_;
        std::vector<umbra::core::Diagnostic> problems_;
        std::uint64_t base_ = 0;
        std::vector<std::string> models_; // installed in Ollama
        std::vector<umbra::core::Diagnostic> modelProblems_;
        bool modelsChecked_ = false;
    };
}

UMBRA_EDITOR_PANEL(AiDraftPanel);

void AiDraftPanel::LoadSettings()
{
    std::ifstream in(SettingsFile());
    const Json settings = Json::parse(in, nullptr, false);
    if (settings.is_object())
    {
        model_ = settings.value("model", model_);
        host_ = settings.value("host", host_);
    }
}

void AiDraftPanel::RefreshModels()
{
    // Asks Ollama which models are installed (local, answers in milliseconds).
    modelProblems_.clear();
    models_ = umbra::localai::ListModels(host_, modelProblems_);
    model_ = umbra::localai::PreferredModel(models_, model_);
    modelsChecked_ = true;
}

void AiDraftPanel::Start(EditorHost &host, bool fix)
{
    // 1. A new request, or the same conversation plus "please fix these problems".
    if (fix)
        umbra::localai::AddFixRequest(request_, answer_, problems_);
    else
        request_ = umbra::localai::MakeDraftRequest(host.Rpg().Data(), npc_, idea_, model_, host_);
    plan_ = {};
    problems_.clear();
    job_->Start(request_);
    std::ofstream(SettingsFile()) << Json{{"model", model_}, {"host", host_}}.dump(2);
}

void AiDraftPanel::Plan(EditorHost &host)
{
    // Checks the last answer against the content as it is now.
    problems_.clear();
    Json draft;
    if (umbra::localai::ParseDraft(answer_, draft, problems_))
        umbra::authoring::PlanDraft(host.Rpg().Data(), draft, npc_, umbra::authoring::ScanScenes(host.AssetRoot() / "scenes"), plan_, problems_);
    base_ = host.Rpg().Revision();
}

void AiDraftPanel::Approve(EditorHost &host)
{
    // One undo step, only if the content is still what the draft was checked against.
    std::vector<umbra::core::Diagnostic> diagnostics;
    if (!host.Rpg().Apply(plan_.changes, plan_.label, diagnostics, base_))
    {
        host.Report(diagnostics, "Draft refused");
        return;
    }
    host.Edited(host.Rpg());
    host.Message("Added quest " + plan_.quest + ". Ctrl+Z undoes it; Save keeps it.", 6.0F);
    // Show it in the RPG forms for hand editing.
    host.Reveal("quests", static_cast<int>(host.Rpg().Data()["quests"].size()) - 1);
    plan_ = {};
    answer_.clear();
}

void AiDraftPanel::Draw(EditorHost &host)
{
    ImGui::SetNextWindowSize(ImVec2(620, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Title(), &open) || !host.Rpg().IsOpen())
    {
        ImGui::End();
        return;
    }

    // 1. A finished answer: check it right away.
    if (std::optional<umbra::localai::ChatResult> result = job_->Take())
    {
        answer_ = result->text;
        if (result->ok)
            Plan(host);
        else
            problems_ = result->diagnostics;
    }

    // 2. The request.
    if (!modelsChecked_)
        RefreshModels();
    ImGui::TextWrapped("Describe a side quest. A local AI model writes a draft; nothing changes until you approve it.");
    const std::vector<std::string> npcs = umbra::authoring::DraftNpcs(host.Rpg().Data());
    if (npc_.empty() && !npcs.empty())
        npc_ = npcs.front();
    if (ImGui::BeginCombo("Quest giver", npc_.c_str()))
    {
        for (const std::string &npc : npcs)
            if (ImGui::Selectable(npc.c_str(), npc == npc_))
                npc_ = npc;
        ImGui::EndCombo();
    }
    ImGui::TextUnformatted("Idea");
    InputString("##idea", idea_, true);
    // The model: picked from what Ollama has installed.
    if (ImGui::BeginCombo("Model", model_.c_str()))
    {
        for (const std::string &model : models_)
            if (ImGui::Selectable(model.c_str(), model == model_))
                model_ = model;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh"))
        RefreshModels();
    Lines(modelProblems_);
    if (ImGui::CollapsingHeader("Ollama settings"))
    {
        InputString("Model name", model_);
        InputString("Address", host_);
        ImGui::TextWrapped("First time: install Ollama from ollama.com, then in a terminal run: ollama pull qwen3.5:9b");
    }

    const bool running = job_->Running();
    if (running)
    {
        ImGui::Text("Writing... %d s, %zu characters", static_cast<int>(job_->Seconds()), job_->Received());
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            job_->Cancel();
    }
    else
    {
        ImGui::BeginDisabled(idea_.empty() || npc_.empty());
        if (ImGui::Button(answer_.empty() ? "Write draft" : "Write a new draft"))
            Start(host, false);
        ImGui::EndDisabled();
        if (!answer_.empty() && umbra::core::HasErrors(problems_))
        {
            ImGui::SameLine();
            if (ImGui::Button("Ask the AI to fix the problems"))
                Start(host, true);
        }
    }
    ImGui::Separator();

    // 3. The result: preview, play-through, problems, then the decision.
    if (!plan_.preview.empty())
    {
        ImGui::BeginChild("preview", ImVec2(0, 260), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        for (const std::string &line : plan_.preview)
            ImGui::TextUnformatted(line.c_str());
        ImGui::EndChild();
    }
    if (!plan_.play.empty())
        ImGui::TextUnformatted("Play-through (fights are assumed won):");
    for (const std::string &line : plan_.play)
        ImGui::TextColored(line.starts_with("ok") ? ImVec4(0.5F, 1, 0.5F, 1) : ImVec4(1, 0.4F, 0.4F, 1), "  %s", line.c_str());
    Lines(problems_);

    if (!plan_.changes.empty() && !running)
    {
        const bool stale = host.Rpg().Revision() != base_;
        if (stale)
        {
            ImGui::TextWrapped("The content changed since this draft was checked.");
            if (ImGui::Button("Check again"))
                Plan(host);
        }
        else
        {
            ImGui::BeginDisabled(umbra::core::HasErrors(problems_));
            if (ImGui::Button("Approve and add"))
                Approve(host);
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            plan_ = {};
            problems_.clear();
            answer_.clear();
        }
        if (ImGui::CollapsingHeader("Changes"))
            for (const umbra::authoring::Change &change : plan_.changes)
                ImGui::TextWrapped("%s %s", change.op == umbra::authoring::Change::Op::Insert ? "insert" : "set", change.path.c_str());
    }
    if (plan_.changes.empty() && answer_.empty() && !running)
        ImGui::TextWrapped("After approving: Save (Ctrl+S), then Play (F5). A running game reloads the content by itself.");
    ImGui::End();
}
