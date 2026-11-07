#pragma once

#include <Ollama.hpp>
#include <QuestDraft.hpp>

#include <string>
#include <vector>

namespace umbra::localai
{
    // Glue between the Ollama bridge and authoring::PlanDraft, shared by the editor and UmbraTool.

    // A first request: rules + ids of this content, then the creator's idea.
    [[nodiscard]] ChatRequest MakeDraftRequest(const authoring::Json &rpg, const std::string &npc, const std::string &idea,
                                               const std::string &model, const std::string &host);

    // Follow-up: shows the model its previous answer and the problems found, asks for a fixed draft.
    void AddFixRequest(ChatRequest &request, const std::string &previousAnswer, const std::vector<core::Diagnostic> &problems);

    // The model's text -> draft JSON. Untrusted: only parsed, never run.
    bool ParseDraft(const std::string &text, authoring::Json &draft, std::vector<core::Diagnostic> &diagnostics);
}
