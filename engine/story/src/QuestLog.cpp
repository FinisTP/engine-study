#include <QuestLog.hpp>

#include <Inventory.hpp>

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

    std::vector<QuestUpdate> QuestLog::Notify(const GameEvent &event, rpg::Character &hero)
    {
        std::vector<QuestUpdate> updates;
        for (auto &[id, progress] : quests_)
        {
            if (progress.state != rpg::QuestState::Active)
                continue; // finished quests ignore everything: no second reward
            const rpg::QuestDef &quest = *content_->FindQuest(id);

            // 1. Death fails quests that say so.
            if (event.kind == GameEvent::Kind::PlayerDied)
            {
                if (quest.failOnDeath)
                {
                    progress.state = rpg::QuestState::Failed;
                    updates.push_back({QuestUpdate::Kind::Failed, id});
                }
                continue;
            }

            // 2. Only the current objective listens. Defeat and Talk count events; Obtain looks
            //    at what the hero holds, so an item picked up early still counts.
            bool moved = false;
            while (progress.objective < quest.objectives.size())
            {
                const rpg::ObjectiveDef &objective = quest.objectives[progress.objective];
                bool done = false;
                if (objective.kind == rpg::ObjectiveKind::Defeat && event.kind == GameEvent::Kind::Defeated &&
                    event.id == objective.target && !moved)
                    done = ++progress.count >= objective.count;
                else if (objective.kind == rpg::ObjectiveKind::Talk && event.kind == GameEvent::Kind::Talked &&
                         event.id == objective.target && !moved)
                    done = true;
                else if (objective.kind == rpg::ObjectiveKind::Obtain)
                    done = rpg::CountItem(hero, objective.target) >= objective.count;
                if (!done)
                    break;
                ++progress.objective; // next objective; an Obtain right after is checked at once
                progress.count = 0;
                moved = true;
            }
            if (!moved)
                continue;

            // 3. All done: complete and reward once (items that don't fit wait in `pending`).
            if (progress.objective < quest.objectives.size())
            {
                updates.push_back({QuestUpdate::Kind::Progress, id});
                continue;
            }
            progress.state = rpg::QuestState::Completed;
            if (!progress.rewarded)
            {
                progress.rewarded = true;
                rpg::GrantXp(*content_, hero, quest.xp);
                hero.skillPoints += quest.skillPoints;
                std::vector<rpg::ItemStack> items;
                for (const rpg::ItemCount &item : quest.items)
                    items.push_back({item.item, item.count});
                rpg::GiveItems(*content_, hero, items);
            }
            updates.push_back({QuestUpdate::Kind::Completed, id});
        }
        return updates;
    }
}
