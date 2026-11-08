#include <GameSession.hpp>

namespace umbra::game
{
    GameSession::GameSession(const rpg::Content &content, std::uint32_t seed)
        : content_(content), random_(seed), quests_(content), combat_(content)
    {
        NewGame();
    }

    void GameSession::NewGame()
    {
        if (combat_.GetState() != encounter::State::Exploration)
            combat_.Finish();
        hero_ = *rpg::MakeCharacter(content_, "hero", "hero"); // the content must define "hero"
        quests_ = story::QuestLog(content_);
        mode_ = AvailableMode();
        hotkeys_.clear(); // the game fills in its default keys
        conversation_.reset();
        enemies_.clear();
        phase_ = Phase::Exploring;
    }

    bool GameSession::ToggleMode()
    {
        if (phase_ != Phase::Exploring || !(hasTurnMode && hasRealtimeMode))
            return false; // only a project with both combat plugins can switch
        mode_ = mode_ == CombatMode::Turn ? CombatMode::Realtime : CombatMode::Turn;
        return true;
    }

    void GameSession::Notify(const story::GameEvent &event)
    {
        const std::vector<story::QuestUpdate> updates = quests_.Notify(event, hero_);
        updates_.insert(updates_.end(), updates.begin(), updates.end());
    }

    // ---------------- Conversations ----------------

    bool GameSession::Talk(const std::string &conversation)
    {
        const rpg::ConversationDef *def = content_.FindConversation(conversation);
        if (phase_ != Phase::Exploring || def == nullptr)
            return false;
        conversation_ = std::make_unique<story::Conversation>(content_, *def);
        phase_ = Phase::Talking;
        return true;
    }

    story::ChoiceResult GameSession::Choose(std::size_t choice)
    {
        if (phase_ != Phase::Talking || !conversation_)
            return story::ChoiceResult::Hidden;
        std::vector<story::StoryEvent> events;
        const story::ChoiceResult result = conversation_->Choose(choice, quests_, hero_, events);
        // Story events move quests: talking counts for Talk objectives, item changes for Obtain.
        for (const story::StoryEvent &e : events)
        {
            if (e.kind == story::StoryEvent::Kind::Talked)
                Notify({story::GameEvent::Kind::Talked, e.id});
            else
                Notify({story::GameEvent::Kind::ItemsChanged, ""});
        }
        if (conversation_->IsOver())
            EndTalk();
        return result;
    }

    void GameSession::EndTalk()
    {
        conversation_.reset();
        if (phase_ == Phase::Talking)
            phase_ = Phase::Exploring;
    }

    // ---------------- Fights ----------------

    std::vector<std::string> GameSession::PickGroup(const std::string &touched, const std::vector<std::string> &pool, int maxSize)
    {
        std::vector<std::string> group{touched};
        if (pool.empty() || maxSize <= 1)
            return group;
        // 0 to maxSize-1 companions, each a random pool member.
        const int extra = std::uniform_int_distribution<int>(0, maxSize - 1)(random_);
        for (int i = 0; i < extra; ++i)
            group.push_back(pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(random_)]);
        return group;
    }

    bool GameSession::StartFight(const std::vector<std::string> &enemyActors)
    {
        if (phase_ != Phase::Exploring || enemyActors.empty())
            return false;
        // 1. Fresh enemies (each placement is its own character, roadmap O06).
        enemies_.clear();
        enemies_.reserve(enemyActors.size()); // the roster points into this vector
        for (std::size_t i = 0; i < enemyActors.size(); ++i)
        {
            std::optional<rpg::Character> enemy = rpg::MakeCharacter(content_, enemyActors[i], enemyActors[i] + "_" + std::to_string(i + 1));
            if (!enemy)
                return false;
            enemies_.push_back(std::move(*enemy));
        }
        // 2. Hero first, then the enemies; the mode chosen beforehand decides the controller.
        rpg::Roster roster{{&hero_, 0}};
        for (rpg::Character &enemy : enemies_)
            roster.push_back({&enemy, 1});
        fightId_ = mode_ == CombatMode::Turn ? combat_.BeginTurn(roster) : combat_.BeginRealtime(roster);
        phase_ = Phase::Fighting;
        return fightId_ != 0;
    }

    bool GameSession::Flee()
    {
        if (phase_ != Phase::Fighting)
            return false;
        combat_.Finish(); // statuses end, the controller and its pending decisions are gone
        enemies_.clear();
        phase_ = Phase::Exploring;
        return true;
    }

    std::optional<FightResult> GameSession::CheckFightEnd()
    {
        if (phase_ != Phase::Fighting || combat_.GetState() != encounter::State::Resolution)
            return std::nullopt;
        FightResult result;
        const std::size_t updatesBefore = updates_.size();
        const int winner = combat_.Finish();

        if (winner == 0)
        {
            // Victory: experience and loot once, then every defeated enemy counts for quests.
            std::vector<const rpg::Character *> defeated;
            for (const rpg::Character &enemy : enemies_)
                defeated.push_back(&enemy);
            result.won = true;
            result.victory = rpg::AwardVictory(content_, hero_, defeated);
            for (const rpg::Character &enemy : enemies_)
                Notify({story::GameEvent::Kind::Defeated, enemy.actor});
            Notify({story::GameEvent::Kind::ItemsChanged, ""});
            phase_ = Phase::Exploring;
        }
        else
        {
            Notify({story::GameEvent::Kind::PlayerDied, ""});
            phase_ = Phase::Dead;
        }
        enemies_.clear();
        result.quests.assign(updates_.begin() + static_cast<std::ptrdiff_t>(updatesBefore), updates_.end());
        return result;
    }

    // ---------------- Saves ----------------

    std::optional<SaveData> GameSession::MakeSave(const std::string &scene, glm::vec3 feet, float yaw) const
    {
        if (phase_ != Phase::Exploring)
            return std::nullopt; // only at a safe boundary
        return SaveData{scene, feet, yaw, hero_, quests_.All(), mode_, hotkeys_};
    }

    void GameSession::ApplySave(const SaveData &data)
    {
        if (combat_.GetState() != encounter::State::Exploration)
            combat_.Finish();
        hero_ = data.hero;
        hero_.statuses.clear();
        quests_.Restore(data.quests);
        mode_ = data.mode;
        hotkeys_ = data.hotkeys;
        conversation_.reset();
        enemies_.clear();
        updates_.clear();
        phase_ = Phase::Exploring;
    }
}
