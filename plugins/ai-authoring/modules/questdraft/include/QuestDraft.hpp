#pragma once

#include <Document.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace umbra::authoring
{
    // AI-assisted quest authoring (roadmap P8B). A "draft" is the JSON a language model returns
    // (shape: DraftSchema). It never touches content directly: PlanDraft turns it into ordinary
    // additive changes, checks them like a manual edit and plays the quest through. See QuestDraft.md.

    // What the scenes offer, so the play-through knows what the player can meet.
    struct WorldFacts final
    {
        std::set<std::string> spawned;       // actor ids in any spawner pool
        std::set<std::string> conversations; // conversation ids some NPC in a scene uses
    };
    [[nodiscard]] WorldFacts ScanScenes(const std::filesystem::path &sceneFolder);

    // The JSON schema the model must follow (Ollama "format").
    [[nodiscard]] Json DraftSchema();
    // System instructions: the rules plus the ids this content allows, for one NPC.
    [[nodiscard]] std::string DraftInstructions(const Json &rpg, const std::string &npc);
    // NPC ids that own a conversation, for pickers.
    [[nodiscard]] std::vector<std::string> DraftNpcs(const Json &rpg);

    struct DraftPlan final
    {
        std::string quest;                // final quest id
        std::string conversation;         // conversation that holds the new lines
        std::string label;                // undo step name
        std::vector<Change> changes;      // only additions (CheckDraftScope)
        Json result;                      // the content after the changes
        std::vector<std::string> preview; // readable summary of what will be added
        std::vector<std::string> play;    // play-through steps, "ok ..." or "FAIL ..."
    };

    // Draft -> changes, validated by the game's loader and played through with the real story
    // rules. Errors (bad draft, invalid content, quest that can't be finished) block approval;
    // warnings (e.g. no scene spawns an enemy yet) don't.
    bool PlanDraft(const Json &rpg, const Json &draft, const std::string &npc, const WorldFacts &world, DraftPlan &plan,
                   std::vector<core::Diagnostic> &diagnostics);

    // True if every change only adds: a new quest, a new conversation, or new nodes/choices in an
    // existing conversation. Anything else is refused (roadmap T21).
    bool CheckDraftScope(const Json &rpg, const std::vector<Change> &changes, std::vector<core::Diagnostic> &diagnostics);
}
