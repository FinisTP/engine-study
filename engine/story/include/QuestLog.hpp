#pragma once

#include <Character.hpp>

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

    // Changing quest state for one player. Definitions come from rpg::Content.
    class QuestLog final
    {
    public:
        explicit QuestLog(const rpg::Content &content) : content_(&content) {}

        [[nodiscard]] rpg::QuestState GetState(const std::string &quest) const;
        [[nodiscard]] const QuestProgress *Get(const std::string &quest) const;
        // NotStarted -> Active. False for an unknown quest or one already started.
        bool Start(const std::string &quest);

        [[nodiscard]] const std::map<std::string, QuestProgress> &All() const noexcept { return quests_; }
        void Restore(std::map<std::string, QuestProgress> quests) { quests_ = std::move(quests); }

    private:
        friend class QuestEvents; // progress rules (P6.2)
        const rpg::Content *content_; // pointer so a QuestLog can be copied (all-or-nothing updates)
        std::map<std::string, QuestProgress> quests_; // only quests that have started
    };
}
