#pragma once

#include <Character.hpp>
#include <Progression.hpp>

#include <map>
#include <string>
#include <vector>

namespace umbra::story
{
    // Progress of one quest. Saved by quest id (P6.3).
    struct QuestProgress final
    {
        rpg::QuestState state = rpg::QuestState::NotStarted;
        std::size_t objective = 0; // index of the current objective
        int count = 0;             // progress on the current objective (Defeat)
        bool rewarded = false;     // rewards are given exactly once
    };

    // Something that may move a quest forward (or fail it).
    struct GameEvent final
    {
        enum class Kind
        {
            Defeated,     // id = actor definition of a defeated enemy
            Talked,       // id = npc
            ItemsChanged, // the hero's inventory changed: re-check Obtain objectives
            PlayerDied
        };
        Kind kind = Kind::ItemsChanged;
        std::string id;
    };

    // What changed, for the HUD ("Quest updated", "Quest complete").
    struct QuestUpdate final
    {
        enum class Kind
        {
            Progress,
            Completed,
            Failed
        };
        Kind kind = Kind::Progress;
        std::string quest;
    };

    // Changing quest state for one player. Definitions come from rpg::Content.
    class QuestLog final
    {
    public:
        explicit QuestLog(const rpg::Content &content) : content_(&content) {}

        [[nodiscard]] rpg::QuestState GetState(const std::string &quest) const;
        [[nodiscard]] const QuestProgress *Get(const std::string &quest) const;
        // NotStarted -> Active. False for an unknown quest or one already started.
        // Call Notify(ItemsChanged) after it so an item already held counts.
        bool Start(const std::string &quest);
        // Applies the event to every active quest (rules in Story.md). Rewards of a completed quest
        // go to `hero` exactly once.
        std::vector<QuestUpdate> Notify(const GameEvent &event, rpg::Character &hero);

        [[nodiscard]] const std::map<std::string, QuestProgress> &All() const noexcept { return quests_; }
        void Restore(std::map<std::string, QuestProgress> quests) { quests_ = std::move(quests); }

    private:
        const rpg::Content *content_; // pointer so a QuestLog can be copied (all-or-nothing updates)
        std::map<std::string, QuestProgress> quests_; // only quests that have started
    };
}
