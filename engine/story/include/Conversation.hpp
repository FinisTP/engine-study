#pragma once

#include <QuestLog.hpp>

#include <optional>
#include <string>
#include <vector>

namespace umbra::story
{
    // What happened in the story, for quests, HUD and audio.
    struct StoryEvent final
    {
        enum class Kind
        {
            QuestStarted,
            Talked,
            ItemGiven,
            ItemTaken
        };
        Kind kind = Kind::Talked;
        std::string id;
        int count = 1;
    };

    enum class ChoiceResult
    {
        Done,
        Hidden,     // its conditions don't hold (or no such choice)
        NoRoom,     // the npc would give items that don't fit: nothing happened
        MissingItem // the npc would take items the player doesn't have: nothing happened
    };

    // One conversation in progress. Picking a choice applies all its actions or none, then moves
    // on. Walking away (Cancel) at any point changes nothing. See Story.md.
    class Conversation final
    {
    public:
        Conversation(const rpg::Content &content, const rpg::ConversationDef &def);

        [[nodiscard]] bool IsOver() const noexcept { return node_ == nullptr; }
        [[nodiscard]] const rpg::DialogueNode &Node() const { return *node_; }
        // Indices of the current node's choices whose conditions hold, in authored order.
        [[nodiscard]] std::vector<std::size_t> VisibleChoices(const QuestLog &quests, const rpg::Character &hero) const;
        ChoiceResult Choose(std::size_t choice, QuestLog &quests, rpg::Character &hero, std::vector<StoryEvent> &events);
        // A node without choices: acknowledge it and end.
        void Close() { node_ = nullptr; }
        void Cancel() { node_ = nullptr; }

    private:
        const rpg::Content &content_;
        const rpg::ConversationDef &def_;
        const rpg::DialogueNode *node_ = nullptr;
    };

    [[nodiscard]] bool Holds(const rpg::Condition &condition, const QuestLog &quests, const rpg::Character &hero);
}
