#include "Editor.hpp"

#include <Platform.hpp>

#include <imgui.h>

#include <cstdio>
#include <fstream>

// AI quest draft window (roadmap P8B). The creator describes a quest; a local Ollama model writes
// a draft; authoring::PlanDraft checks it and plays it through. Nothing changes until "Approve",
// which is one ordinary undoable edit of the RPG document. Rules in Editor.md.

using umbra::authoring::Json;

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
}

void Editor::LoadDraftSettings()
{
    std::ifstream in(SettingsFile());
    const Json settings = Json::parse(in, nullptr, false);
    if (settings.is_object())
    {
        draftModel_ = settings.value("model", draftModel_);
        draftHost_ = settings.value("host", draftHost_);
    }
}

void Editor::RefreshDraftModels()
{
    // Asks Ollama which models are installed (local, answers in milliseconds).
    draftModelProblems_.clear();
    draftModels_ = umbra::localai::ListModels(draftHost_, draftModelProblems_);
    draftModel_ = umbra::localai::PreferredModel(draftModels_, draftModel_);
    draftModelsChecked_ = true;
}

void Editor::StartDraft(bool fix)
{
    // 1. A new request, or the same conversation plus "please fix these problems".
    if (fix)
        umbra::localai::AddFixRequest(draftRequest_, draftAnswer_, draftProblems_);
    else
        draftRequest_ = umbra::localai::MakeDraftRequest(rpg_->Data(), draftNpc_, draftIdea_, draftModel_, draftHost_);
    draftPlan_ = {};
    draftProblems_.clear();
    draftJob_->Start(draftRequest_);
    std::ofstream(SettingsFile()) << Json{{"model", draftModel_}, {"host", draftHost_}}.dump(2);
}

void Editor::PlanCurrentDraft()
{
    // Checks the last answer against the content as it is now.
    draftProblems_.clear();
    Json draft;
    if (umbra::localai::ParseDraft(draftAnswer_, draft, draftProblems_))
        umbra::authoring::PlanDraft(rpg_->Data(), draft, draftNpc_, umbra::authoring::ScanScenes(assetRoot_ / "scenes"), draftPlan_, draftProblems_);
    draftBase_ = rpg_->Revision();
}

void Editor::ApproveDraft()
{
    // One undo step, only if the content is still what the draft was checked against.
    std::vector<umbra::core::Diagnostic> diagnostics;
    if (!rpg_->Apply(draftPlan_.changes, draftPlan_.label, diagnostics, draftBase_))
    {
        Report(diagnostics, "Draft refused");
        return;
    }
    lastEdited_ = rpg_.get();
    message_ = "Added quest " + draftPlan_.quest + ". Ctrl+Z undoes it; Save keeps it.";
    messageSeconds_ = 6.0F;
    // Show it in the RPG forms for hand editing.
    showRpg_ = true;
    rpgSection_ = "quests";
    rpgSelected_ = static_cast<int>(rpg_->Data()["quests"].size()) - 1;
    draftPlan_ = {};
    draftAnswer_.clear();
}

void Editor::AiDraft()
{
    if (!showDraft_)
        return;
    ImGui::SetNextWindowSize(ImVec2(620, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("AI quest draft", &showDraft_) || !rpg_->IsOpen())
    {
        ImGui::End();
        return;
    }

    // 1. A finished answer: check it right away.
    if (std::optional<umbra::localai::ChatResult> result = draftJob_->Take())
    {
        draftAnswer_ = result->text;
        if (result->ok)
            PlanCurrentDraft();
        else
            draftProblems_ = result->diagnostics;
    }

    // 2. The request.
    if (!draftModelsChecked_)
        RefreshDraftModels();
    ImGui::TextWrapped("Describe a side quest. A local AI model writes a draft; nothing changes until you approve it.");
    const std::vector<std::string> npcs = umbra::authoring::DraftNpcs(rpg_->Data());
    if (draftNpc_.empty() && !npcs.empty())
        draftNpc_ = npcs.front();
    if (ImGui::BeginCombo("Quest giver", draftNpc_.c_str()))
    {
        for (const std::string &npc : npcs)
            if (ImGui::Selectable(npc.c_str(), npc == draftNpc_))
                draftNpc_ = npc;
        ImGui::EndCombo();
    }
    ImGui::TextUnformatted("Idea");
    InputString("##idea", draftIdea_, true);
    // The model: picked from what Ollama has installed.
    if (ImGui::BeginCombo("Model", draftModel_.c_str()))
    {
        for (const std::string &model : draftModels_)
            if (ImGui::Selectable(model.c_str(), model == draftModel_))
                draftModel_ = model;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh"))
        RefreshDraftModels();
    Lines(draftModelProblems_);
    if (ImGui::CollapsingHeader("Ollama settings"))
    {
        InputString("Model name", draftModel_);
        InputString("Address", draftHost_);
        ImGui::TextWrapped("First time: install Ollama from ollama.com, then in a terminal run: ollama pull qwen3.5:9b");
    }

    const bool running = draftJob_->Running();
    if (running)
    {
        ImGui::Text("Writing... %d s, %zu characters", static_cast<int>(draftJob_->Seconds()), draftJob_->Received());
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            draftJob_->Cancel();
    }
    else
    {
        ImGui::BeginDisabled(draftIdea_.empty() || draftNpc_.empty());
        if (ImGui::Button(draftAnswer_.empty() ? "Write draft" : "Write a new draft"))
            StartDraft(false);
        ImGui::EndDisabled();
        if (!draftAnswer_.empty() && umbra::core::HasErrors(draftProblems_))
        {
            ImGui::SameLine();
            if (ImGui::Button("Ask the AI to fix the problems"))
                StartDraft(true);
        }
    }
    ImGui::Separator();

    // 3. The result: preview, play-through, problems, then the decision.
    if (!draftPlan_.preview.empty())
    {
        ImGui::BeginChild("preview", ImVec2(0, 260), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        for (const std::string &line : draftPlan_.preview)
            ImGui::TextUnformatted(line.c_str());
        ImGui::EndChild();
    }
    if (!draftPlan_.play.empty())
        ImGui::TextUnformatted("Play-through (fights are assumed won):");
    for (const std::string &line : draftPlan_.play)
        ImGui::TextColored(line.starts_with("ok") ? ImVec4(0.5F, 1, 0.5F, 1) : ImVec4(1, 0.4F, 0.4F, 1), "  %s", line.c_str());
    Lines(draftProblems_);

    if (!draftPlan_.changes.empty() && !running)
    {
        const bool stale = rpg_->Revision() != draftBase_;
        if (stale)
        {
            ImGui::TextWrapped("The content changed since this draft was checked.");
            if (ImGui::Button("Check again"))
                PlanCurrentDraft();
        }
        else
        {
            ImGui::BeginDisabled(umbra::core::HasErrors(draftProblems_));
            if (ImGui::Button("Approve and add"))
                ApproveDraft();
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            draftPlan_ = {};
            draftProblems_.clear();
            draftAnswer_.clear();
        }
        if (ImGui::CollapsingHeader("Changes"))
            for (const umbra::authoring::Change &change : draftPlan_.changes)
                ImGui::TextWrapped("%s %s", change.op == umbra::authoring::Change::Op::Insert ? "insert" : "set", change.path.c_str());
    }
    if (draftPlan_.changes.empty() && draftAnswer_.empty() && !running)
        ImGui::TextWrapped("After approving: Save (Ctrl+S), then Play (F5). A running game reloads the content by itself.");
    ImGui::End();
}
