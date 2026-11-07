#include "GameFlow.hpp"

#include <Log.hpp>
#include <RealtimeEncounter.hpp>
#include <TurnEncounter.hpp>

#include <glm/geometric.hpp>

#include <algorithm>

using umbra::platform::InputKey;
namespace rpg = umbra::rpg;
namespace game = umbra::game;

namespace
{
    constexpr float enemyTurnDelay = 0.7F; // seconds, so the player can follow enemy turns
    constexpr float enemyFightSpeed = 2.5F; // m/s while fighting in real time
    constexpr float escapeDistance = 12.0F; // real time: this far from every enemy ends the fight
    constexpr std::size_t logLines = 5;

    std::string ItemList(const rpg::Content &content, const std::vector<rpg::ItemStack> &items)
    {
        std::string text;
        for (const rpg::ItemStack &stack : items)
        {
            const rpg::ItemDef *def = content.FindItem(stack.item);
            text += (text.empty() ? "" : ", ") + (def ? def->name : stack.item) + (stack.count > 1 ? " x" + std::to_string(stack.count) : "");
        }
        return text;
    }
}

GameFlow::GameFlow(const rpg::Content &content, SceneRuntime &scene, std::filesystem::path saveFolder, std::string startScene)
    : session_(content, 20261107U), scene_(scene), slots_{std::move(saveFolder)}, startScene_(std::move(startScene))
{
}

// ---------------- Per frame ----------------

void GameFlow::Update(const umbra::platform::Input &input, float deltaSeconds)
{
    contactCooldown_ = std::max(0.0F, contactCooldown_ - deltaSeconds);

    // The world was replaced mid-fight (restart, hot reload, checkpoint): its enemies are gone.
    if (session_.GetPhase() == game::Phase::Fighting && scene_.Generation() != fightGeneration_)
    {
        session_.Flee();
        fighters_.clear();
    }

    switch (session_.GetPhase())
    {
    case game::Phase::Exploring:
        UpdateExploring(input);
        break;
    case game::Phase::Talking:
        UpdateTalking(input);
        break;
    case game::Phase::Fighting:
        if (session_.Mode() == game::CombatMode::Turn)
            UpdateTurnFight(input, deltaSeconds);
        else
            UpdateRealtimeFight(input, deltaSeconds);
        if (std::optional<game::FightResult> result = session_.CheckFightEnd())
            EndFight(*result);
        break;
    case game::Phase::Dead:
        UpdateDeath(input);
        break;
    }
    scene_.SetRoamersActive(session_.GetPhase() == game::Phase::Exploring);

    // Quest news for the screen.
    for (const umbra::story::QuestUpdate &update : session_.TakeQuestUpdates())
    {
        const rpg::QuestDef *quest = session_.Content().FindQuest(update.quest);
        const std::string name = quest ? quest->name : update.quest;
        using Kind = umbra::story::QuestUpdate::Kind;
        Say(update.kind == Kind::Completed ? "Quest complete: " + name
            : update.kind == Kind::Failed  ? "Quest failed: " + name
                                           : "Quest updated: " + name);
    }
}

bool GameFlow::PlayerCanMove() const
{
    const game::Phase phase = session_.GetPhase();
    return phase == game::Phase::Exploring ||
           (phase == game::Phase::Fighting && session_.Mode() == game::CombatMode::Realtime);
}

bool GameFlow::AllowsPortals() const
{
    return session_.GetPhase() == game::Phase::Exploring || session_.GetPhase() == game::Phase::Talking;
}

bool GameFlow::HandleEscape()
{
    if (session_.GetPhase() == game::Phase::Talking)
    {
        session_.EndTalk(); // walking away changes nothing
        return true;
    }
    return session_.GetPhase() == game::Phase::Dead;
}

bool GameFlow::MenuInput(const umbra::platform::Input &input, std::size_t count)
{
    if (count == 0)
        return false;
    const int move = (input.IsKeyPressed(InputKey::S) || input.IsKeyPressed(InputKey::Down) ? 1 : 0) -
                     (input.IsKeyPressed(InputKey::W) || input.IsKeyPressed(InputKey::Up) ? 1 : 0);
    // Wrap around, and keep the cursor valid when the list got shorter.
    selected_ = (selected_ + move + static_cast<int>(count)) % static_cast<int>(count);
    return input.IsKeyPressed(InputKey::Enter);
}

void GameFlow::Say(const std::string &text)
{
    message_ = message_.empty() ? text : message_ + "  |  " + text;
}

// ---------------- Exploring and talking ----------------

void GameFlow::UpdateExploring(const umbra::platform::Input &input)
{
    if (input.IsKeyPressed(InputKey::Tab) && session_.ToggleMode())
        Say(session_.Mode() == game::CombatMode::Turn ? "Combat: turn-based" : "Combat: real-time");
    if (contactCooldown_ <= 0.0F)
        if (const std::optional<std::size_t> touched = scene_.TouchingRoamer())
            StartFight(*touched);
}

bool GameFlow::Interact(const umbra::world::Interactable &interactable)
{
    if (!interactable.conversation.empty())
    {
        if (session_.Talk(interactable.conversation))
            selected_ = 0;
        else
            Say("Nothing to say (unknown conversation " + interactable.conversation + ")");
        return true;
    }
    if (interactable.save)
    {
        Say(Save(slots_.Manual()) ? "Game saved" : "Save failed (see log)");
        return true;
    }
    return false;
}

void GameFlow::UpdateTalking(const umbra::platform::Input &input)
{
    umbra::story::Conversation *talk = session_.CurrentConversation();
    if (talk == nullptr)
        return;
    const std::vector<std::size_t> visible = talk->VisibleChoices(session_.Quests(), session_.Hero());
    // A line without answers: Enter ends the conversation.
    if (visible.empty())
    {
        if (MenuInput(input, 1))
            session_.EndTalk();
        return;
    }
    if (!MenuInput(input, visible.size()))
        return;
    const umbra::story::ChoiceResult result = session_.Choose(visible[static_cast<std::size_t>(selected_)]);
    selected_ = 0;
    if (result == umbra::story::ChoiceResult::NoRoom)
        Say("Your bags are full");
    else if (result == umbra::story::ChoiceResult::MissingItem)
        Say("You don't have that");
}

// ---------------- Fights ----------------

void GameFlow::StartFight(std::size_t roamer)
{
    // 1. Who fights: the touched enemy plus random companions from its area's pool.
    const RoamerSlot touched = scene_.Roamers()[roamer]; // copy: AddRoamer below grows the list
    const std::vector<std::string> group = session_.PickGroup(touched.roamer.Actor(), touched.pool, touched.group);
    if (!session_.StartFight(group))
        return;
    fighters_ = {roamer};
    fightGeneration_ = scene_.Generation();

    // 2. Companions appear beside the touched enemy, on the side away from the player (fights happen in place).
    const glm::vec3 base = scene_.RoamerFeet(roamer);
    const glm::vec3 player = scene_.GetPlayerSpot().feet;
    glm::vec3 away{base.x - player.x, 0.0F, base.z - player.z};
    away = glm::length(away) > 0.01F ? glm::normalize(away) : glm::vec3{0.0F, 0.0F, -1.0F};
    const glm::vec3 side{-away.z, 0.0F, away.x};
    for (std::size_t i = 1; i < group.size(); ++i)
    {
        const glm::vec3 feet = base + away * 0.9F + side * (i % 2 == 1 ? 1.1F : -1.1F);
        fighters_.push_back(scene_.AddRoamer(group[i], feet, feet, 0.0F, false));
    }

    // 3. Fresh log; enemies wait a moment before their first turn.
    log_ = {"A fight with " + std::to_string(group.size()) + " enemies!"};
    selected_ = 0;
    enemyDelay_ = enemyTurnDelay;
}

void GameFlow::Act(rpg::ActionIntent intent, bool player)
{
    intent.id = session_.NextDecisionId();
    const rpg::Rejection rejection = session_.Combat().Submit(session_.FightId(), intent);
    if (player && rejection != rpg::Rejection::None)
        Say(std::string{"Can't: "} + rpg::ToString(rejection));
}

void GameFlow::UpdateTurnFight(const umbra::platform::Input &input, float deltaSeconds)
{
    umbra::turn::TurnEncounter *turn = session_.Combat().Turn();
    if (turn == nullptr)
        return;
    if (input.IsKeyPressed(InputKey::F))
        return Flee();

    // 1. The hero's turn: pick an action from the menu (the last item is Flee).
    const std::optional<std::size_t> current = turn->Current();
    if (current && *current == 0)
    {
        const std::vector<rpg::ActionIntent> legal = turn->LegalActions();
        if (MenuInput(input, legal.size() + 1))
        {
            if (static_cast<std::size_t>(selected_) == legal.size())
                return Flee();
            Act(legal[static_cast<std::size_t>(selected_)], true);
            enemyDelay_ = enemyTurnDelay;
        }
    }
    // 2. An enemy's turn: it decides after a short pause, through the same rules.
    else if (current)
    {
        enemyDelay_ -= deltaSeconds;
        if (enemyDelay_ <= 0.0F)
        {
            Act(umbra::turn::ChooseTurnAction(session_.Content(), *turn), false);
            enemyDelay_ = enemyTurnDelay;
        }
    }
    ReadEvents(turn->TakeEvents());
}

void GameFlow::UpdateRealtimeFight(const umbra::platform::Input &input, float deltaSeconds)
{
    umbra::realtime::RealtimeEncounter *fight = session_.Combat().Realtime();
    if (fight == nullptr)
        return;
    std::vector<RoamerSlot> &roamers = scene_.Roamers();

    // 1. Tell the fight where everyone stands (movement belongs to physics).
    const umbra::gameplay::SpawnPoint player = scene_.GetPlayerSpot();
    fight->SetPlacement(0, {player.feet.x, player.feet.z}, player.yaw);
    for (std::size_t i = 0; i < fighters_.size(); ++i)
    {
        const glm::vec3 feet = scene_.RoamerFeet(fighters_[i]);
        fight->SetPlacement(i + 1, {feet.x, feet.z}, roamers[fighters_[i]].yaw);
    }

    // 2. The player: 1-4 use skills, Q drinks a potion, F (or running far away) flees.
    const std::vector<std::string> skills = RealtimeSkills();
    const InputKey keys[] = {InputKey::Num1, InputKey::Num2, InputKey::Num3, InputKey::Num4};
    for (std::size_t i = 0; i < 4 && i < skills.size(); ++i)
        if (input.IsKeyPressed(keys[i]))
            Act(rpg::ActionIntent{0, 0, skills[i], "", 0}, true);
    if (input.IsKeyPressed(InputKey::Q))
    {
        const std::string potion = PotionItem();
        if (potion.empty())
            Say("No potions left");
        else
            Act(rpg::ActionIntent{0, 0, "", potion, 0}, true);
    }
    if (input.IsKeyPressed(InputKey::F))
        return Flee();

    // 3. Enemies: walk and attack as the conventional real-time AI decides.
    bool anyoneNear = false;
    const rpg::Roster &roster = fight->GetRoster();
    for (std::size_t i = 0; i < fighters_.size(); ++i)
    {
        RoamerSlot &slot = roamers[fighters_[i]];
        if (!roster[i + 1].character->IsAlive() || !slot.alive)
            continue;
        const umbra::realtime::Decision decision = umbra::realtime::ChooseRealtimeAction(session_.Content(), *fight, i + 1);
        slot.order = glm::vec3{decision.move.x, 0.0F, decision.move.y} * enemyFightSpeed;
        slot.yaw = decision.yaw;
        if (decision.act)
            Act(*decision.act, false);
        const glm::vec3 feet = scene_.RoamerFeet(fighters_[i]);
        anyoneNear |= glm::length(glm::vec2{feet.x - player.feet.x, feet.z - player.feet.z}) < escapeDistance;
    }

    // 4. Time passes; then see what happened.
    session_.Combat().Update(deltaSeconds);
    ReadEvents(fight->TakeEvents());
    if (!anyoneNear && session_.Combat().GetState() == umbra::encounter::State::Realtime)
        Flee();
}

void GameFlow::ReadEvents(const rpg::Events &events)
{
    // Fight lines for the log; a defeated enemy leaves the world at once.
    for (const rpg::Event &e : events)
    {
        std::string line;
        switch (e.kind)
        {
        case rpg::EventKind::Damage:
            line = FighterName(e.actor) + " hit " + FighterName(e.target) + " for " + std::to_string(e.amount);
            break;
        case rpg::EventKind::Heal:
            line = FighterName(e.target) + " healed " + std::to_string(e.amount);
            break;
        case rpg::EventKind::ItemUsed:
            line = FighterName(e.actor) + " used " + ItemList(session_.Content(), {{e.what, 1}});
            break;
        case rpg::EventKind::StatusApplied:
            line = FighterName(e.target) + " is affected by " + e.what;
            break;
        case rpg::EventKind::Death:
            line = FighterName(e.target) + (e.target == 0 ? " fell" : " was defeated");
            if (e.target >= 1 && e.target <= fighters_.size())
                scene_.RemoveRoamer(fighters_[e.target - 1]);
            break;
        default:
            break;
        }
        if (!line.empty())
            log_.push_back(line);
    }
    if (log_.size() > logLines)
        log_.erase(log_.begin(), log_.end() - static_cast<std::ptrdiff_t>(logLines));
}

void GameFlow::Flee()
{
    // Fleeing always works. Companions vanish; the touched enemy stays, calm, and can't
    // start a new fight for a few seconds.
    session_.Flee();
    for (std::size_t i = 1; i < fighters_.size(); ++i)
        scene_.RemoveRoamer(fighters_[i]);
    if (!fighters_.empty() && fighters_[0] < scene_.Roamers().size())
    {
        RoamerSlot &touched = scene_.Roamers()[fighters_[0]];
        touched.order.reset();
        touched.roamer.Calm();
    }
    fighters_.clear();
    contactCooldown_ = 3.0F;
    Say("You got away");
}

void GameFlow::EndFight(const game::FightResult &result)
{
    // Victory: every enemy of the fight is gone until the area is entered again.
    for (std::size_t roamer : fighters_)
        scene_.RemoveRoamer(roamer);
    fighters_.clear();
    selected_ = 0;
    if (!result.won)
    {
        Say("You died");
        return;
    }
    std::string text = "Victory! +" + std::to_string(result.victory.xp) + " XP";
    if (result.victory.levels > 0)
        text += ", level " + std::to_string(session_.Hero().level) + "!";
    if (!result.victory.loot.empty())
        text += "  Loot: " + ItemList(session_.Content(), result.victory.loot);
    if (!session_.Hero().pending.empty())
        text += " (bags full: some loot waits)";
    Say(text);
}

// ---------------- Death and saves ----------------

void GameFlow::UpdateDeath(const umbra::platform::Input &input)
{
    if (!MenuInput(input, 3))
        return;
    switch (selected_)
    {
    case 0:
        (void)Load(slots_.Manual());
        break;
    case 1:
        (void)Load(slots_.Autosave());
        break;
    default:
        // A fresh start in the first area.
        session_.NewGame();
        if (!scene_.ChangeScene(startScene_, "player"))
            Say("Cannot load " + startScene_);
        break;
    }
}

void GameFlow::OnAreaEntered()
{
    if (!Save(slots_.Autosave()))
        Say("Autosave failed (see log)");
}

bool GameFlow::Save(const std::filesystem::path &file)
{
    const umbra::gameplay::SpawnPoint spot = scene_.GetPlayerSpot();
    const std::optional<game::SaveData> data = session_.MakeSave(scene_.GetScenePath(), spot.feet, spot.yaw);
    std::vector<umbra::core::Diagnostic> diagnostics;
    const bool saved = data && game::WriteSave(file, *data, diagnostics);
    for (const umbra::core::Diagnostic &diagnostic : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(diagnostic));
    return saved;
}

bool GameFlow::Load(const std::filesystem::path &file)
{
    // 1. Read and check the whole save first (a damaged file falls back to its backup).
    game::SaveData data;
    std::vector<umbra::core::Diagnostic> diagnostics;
    const bool read = game::ReadSaveOrBackup(file, session_.Content(), data, diagnostics);
    for (const umbra::core::Diagnostic &diagnostic : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(diagnostic));
    if (!read)
    {
        Say("No usable save");
        return false;
    }
    // 2. The area, then the RPG state (a broken area leaves everything as it was).
    if (!scene_.ChangeSceneAt(data.scene, {data.feet, data.yaw}))
    {
        Say("Cannot load area " + data.scene);
        return false;
    }
    session_.ApplySave(data);
    Say("Loaded");
    return true;
}

// ---------------- Names and screen ----------------

const rpg::Roster *GameFlow::FightRoster()
{
    if (umbra::turn::TurnEncounter *turn = session_.Combat().Turn())
        return &turn->GetRoster();
    if (umbra::realtime::RealtimeEncounter *fight = session_.Combat().Realtime())
        return &fight->GetRoster();
    return nullptr;
}

std::string GameFlow::FighterName(std::size_t index)
{
    if (index == 0)
        return "You";
    const rpg::Roster *roster = FightRoster();
    if (roster == nullptr || index >= roster->size())
        return "Enemy " + std::to_string(index);
    const rpg::ActorDef *actor = session_.Content().FindActor((*roster)[index].character->actor);
    return (actor ? actor->name : (*roster)[index].character->actor) + " " + std::to_string(index);
}

std::string GameFlow::ActionName(const rpg::ActionIntent &intent)
{
    if (!intent.item.empty())
        return "Use " + ItemList(session_.Content(), {{intent.item, 1}});
    if (intent.skill.empty())
        return "Wait";
    const rpg::SkillDef *skill = session_.Content().FindSkill(intent.skill);
    std::string text = skill ? skill->name : intent.skill;
    if (skill && skill->energyCost > 0)
        text += " (" + std::to_string(skill->energyCost) + " EN)";
    if (intent.target != intent.actor)
        text += " -> " + FighterName(intent.target);
    return text;
}

std::vector<std::string> GameFlow::RealtimeSkills()
{
    // Unlocked real-time skills in a fixed (alphabetical) order, for keys 1-4.
    std::vector<std::string> skills;
    for (const std::string &id : session_.Hero().skills)
        if (const rpg::SkillDef *skill = session_.Content().FindSkill(id); skill && skill->realtime)
            skills.push_back(id);
    return skills;
}

std::string GameFlow::PotionItem()
{
    // The first usable item in the bag.
    for (const rpg::ItemStack &stack : session_.Hero().inventory)
        if (const rpg::ItemDef *item = session_.Content().FindItem(stack.item); item && !item->use.empty())
            return stack.item;
    return {};
}

void GameFlow::FillHud(umbra::core::Hud &hud)
{
    const rpg::Content &content = session_.Content();
    rpg::Character &hero = session_.Hero();
    const rpg::StatBlock stats = rpg::DerivedStats(content, hero);

    // 1. Status lines: the hero, the combat mode, active quests.
    hud.status.push_back("Level " + std::to_string(hero.level) + "   HP " + std::to_string(hero.health) + "/" +
                         std::to_string(rpg::Get(stats, rpg::Stat::MaxHealth)) + "   EN " + std::to_string(hero.energy) +
                         "/" + std::to_string(rpg::Get(stats, rpg::Stat::MaxEnergy)) + "   XP " + std::to_string(hero.xp) +
                         "/" + std::to_string(rpg::XpForLevel(hero.level + 1)));
    hud.status.push_back(std::string{"Combat: "} + (session_.Mode() == game::CombatMode::Turn ? "turn-based" : "real-time") +
                         (session_.GetPhase() == game::Phase::Exploring ? "  [Tab] switch" : ""));
    for (const auto &[id, progress] : session_.Quests().All())
    {
        const rpg::QuestDef *quest = content.FindQuest(id);
        if (quest == nullptr || progress.state != rpg::QuestState::Active || progress.objective >= quest->objectives.size())
            continue;
        hud.status.push_back("Quest: " + quest->name + " - " + quest->objectives[progress.objective].text);
    }

    switch (session_.GetPhase())
    {
    case game::Phase::Exploring:
        break;

    // 2. Talking: the line and the answers.
    case game::Phase::Talking:
        if (umbra::story::Conversation *talk = session_.CurrentConversation())
        {
            hud.menuTitle = talk->Node().speaker.empty() ? "..." : talk->Node().speaker;
            hud.menuText = talk->Node().text;
            for (std::size_t choice : talk->VisibleChoices(session_.Quests(), hero))
                hud.menuItems.push_back(talk->Node().choices[choice].text);
            if (hud.menuItems.empty())
                hud.menuItems.push_back("(Leave)");
            hud.menuSelected = selected_;
            hud.menuHint = "W/S choose, Enter answer, Esc walk away";
        }
        break;

    // 3. Fighting: enemy health and the log; a menu on the hero's turn, key help in real time.
    case game::Phase::Fighting:
        if (const rpg::Roster *roster = FightRoster())
            for (std::size_t i = 1; i < roster->size(); ++i)
                if ((*roster)[i].character->IsAlive())
                    hud.status.push_back(FighterName(i) + ": " + std::to_string((*roster)[i].character->health) + " HP");
        hud.status.insert(hud.status.end(), log_.begin(), log_.end());
        if (umbra::turn::TurnEncounter *turn = session_.Combat().Turn())
        {
            const std::optional<std::size_t> current = turn->Current();
            hud.menuDim = false;
            if (current && *current == 0)
            {
                hud.menuTitle = "Round " + std::to_string(turn->Round()) + " - your turn";
                for (const rpg::ActionIntent &intent : turn->LegalActions())
                    hud.menuItems.push_back(ActionName(intent));
                hud.menuItems.push_back("Flee");
                hud.menuSelected = selected_;
                hud.menuHint = "W/S choose, Enter act, F flee";
            }
            else if (current)
                hud.prompt = FighterName(*current) + " is acting...";
        }
        else
        {
            const std::vector<std::string> skills = RealtimeSkills();
            std::string keys;
            for (std::size_t i = 0; i < 4 && i < skills.size(); ++i)
            {
                const rpg::SkillDef *skill = content.FindSkill(skills[i]);
                keys += "[" + std::to_string(i + 1) + "] " + (skill ? skill->name : skills[i]) + "  ";
            }
            hud.prompt = keys + "[Q] potion  [F] flee";
        }
        break;

    // 4. Dead: how to go on.
    case game::Phase::Dead:
        hud.menuTitle = "You died";
        hud.menuItems = {"Load save", "Load last autosave", "New game"};
        hud.menuSelected = selected_;
        hud.menuHint = "W/S choose, Enter";
        break;
    }
}
