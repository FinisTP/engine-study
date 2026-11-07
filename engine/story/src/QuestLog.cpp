#include <QuestLog.hpp>

namespace umbra::story
{
    rpg::QuestState QuestLog::GetState(const std::string &quest) const
    {
        const QuestProgress *progress = Get(quest);
        return progress ? progress->state : rpg::QuestState::NotStarted;
    }

    const QuestProgress *QuestLog::Get(const std::string &quest) const
    {
        const auto it = quests_.find(quest);
        return it == quests_.end() ? nullptr : &it->second;
    }

    bool QuestLog::Start(const std::string &quest)
    {
        if (content_->FindQuest(quest) == nullptr || quests_.contains(quest))
            return false;
        quests_[quest] = {rpg::QuestState::Active};
        return true;
    }
}
