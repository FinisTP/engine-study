#include <Conversation.hpp>

#include <Inventory.hpp>

#include <algorithm>

namespace umbra::story
{
    Conversation::Conversation(const rpg::Content &content, const rpg::ConversationDef &def)
        : content_(content), def_(def), node_(&def.nodes.at(def.start)) // start node checked on load
    {
    }

    bool Holds(const rpg::Condition &condition, const QuestLog &quests, const rpg::Character &hero)
    {
        if (condition.kind == rpg::Condition::Kind::QuestIs)
            return quests.GetState(condition.id) == condition.state;
        return rpg::CountItem(hero, condition.id) >= condition.count;
    }

    std::vector<std::size_t> Conversation::VisibleChoices(const QuestLog &quests, const rpg::Character &hero) const
    {
        std::vector<std::size_t> visible;
        for (std::size_t i = 0; node_ && i < node_->choices.size(); ++i)
        {
            bool all = true;
            for (const rpg::Condition &condition : node_->choices[i].conditions)
                all = all && Holds(condition, quests, hero);
            if (all)
                visible.push_back(i);
        }
        return visible;
    }

    ChoiceResult Conversation::Choose(std::size_t choice, QuestLog &quests, rpg::Character &hero, std::vector<StoryEvent> &events)
    {
        // 1. Only a visible choice of the current node.
        const std::vector<std::size_t> visible = VisibleChoices(quests, hero);
        if (std::find(visible.begin(), visible.end(), choice) == visible.end())
            return ChoiceResult::Hidden;
        const rpg::DialogueChoice &picked = node_->choices[choice];

        // 2. Apply every action to copies; give up without a trace if one can't happen.
        QuestLog newQuests = quests;
        rpg::Character newHero = hero;
        std::vector<StoryEvent> newEvents;
        for (const rpg::StoryAction &action : picked.actions)
        {
            switch (action.kind)
            {
            case rpg::StoryAction::Kind::StartQuest:
                if (newQuests.Start(action.id))
                    newEvents.push_back({StoryEvent::Kind::QuestStarted, action.id});
                break;
            case rpg::StoryAction::Kind::Talk:
                newEvents.push_back({StoryEvent::Kind::Talked, action.id});
                break;
            case rpg::StoryAction::Kind::Give:
                if (!rpg::AddItem(content_, newHero, action.id, action.count))
                    return ChoiceResult::NoRoom;
                newEvents.push_back({StoryEvent::Kind::ItemGiven, action.id, action.count});
                break;
            case rpg::StoryAction::Kind::Take:
                if (!rpg::RemoveItem(newHero, action.id, action.count))
                    return ChoiceResult::MissingItem;
                newEvents.push_back({StoryEvent::Kind::ItemTaken, action.id, action.count});
                break;
            }
        }

        // 3. Commit, then move to the next node (or end).
        quests = std::move(newQuests);
        hero = std::move(newHero);
        events.insert(events.end(), newEvents.begin(), newEvents.end());
        node_ = picked.next.empty() ? nullptr : &def_.nodes.at(picked.next);
        return ChoiceResult::Done;
    }
}
