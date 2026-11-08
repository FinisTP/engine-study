#include "GameFlow.hpp"

#include <Log.hpp>
#include <Profiling.hpp>
#include <PolicyContract.hpp>
#include <RealtimeEncounter.hpp>
#include <TurnEncounter.hpp>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

using umbra::platform::InputKey;
namespace rpg = umbra::rpg;
namespace game = umbra::game;

namespace
{
    constexpr float enemyTurnDelay = 0.7F; // seconds, so the player can follow enemy turns
    constexpr float enemyFightSpeed = 2.5F; // m/s while fighting in real time
    constexpr std::size_t logLines = 5;
    const std::string arenaScene = "scenes/arena.scene.json"; // every fight happens here

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

GameFlow::GameFlow(rpg::Content &content, std::filesystem::path contentFile, SceneRuntime &scene, std::filesystem::path saveFolder,
                   std::string startScene, bool offerContinue)
    : content_(content), contentFile_(std::move(contentFile)), session_(content, 20261107U), scene_(scene),
      slots_{std::move(saveFolder)}, startScene_(std::move(startScene))
{
    startMenu_ = offerContinue && (std::filesystem::exists(slots_.Manual()) || std::filesystem::exists(slots_.Autosave()));
    contentWatcher_.SetFiles({contentFile_});

    // Enemies look like their actor's model (RPG content); without one, the tinted cube.
    scene_.SetRoamerLook([this](const std::string &actor) -> std::optional<RoamerLook> {
        const rpg::ActorDef *def = content_.FindActor(actor);
        if (def == nullptr || def->model.empty())
            return std::nullopt;
        RoamerLook look;
        look.model = def->model;
        look.color = {def->color[0], def->color[1], def->color[2], def->color[3]};
        look.scale = glm::vec3{def->size};
        return look;
    });
    LoadPolicies();
}

void GameFlow::LoadPolicies()
{
    // Each actor that names a learned policy gets it only if the file loads and still fits the
    // content; otherwise the standard AI plays it, and the screen says so (roadmap P8A.6).
    learned_.clear();
    const std::filesystem::path assets = contentFile_.parent_path().parent_path(); // assets/rpg/x.rpg.json
    for (const auto &[id, actor] : content_.actors)
    {
        if (actor.policy.empty())
            continue;
        LearnedEnemy learned;
        std::vector<umbra::core::Diagnostic> diagnostics;
        const std::filesystem::path file = assets / actor.policy;
        if (umbra::policy::LoadPolicy(file, learned.model, diagnostics) &&
            umbra::policy::CheckCompatible(learned.model, content_, file.string(), diagnostics))
        {
            learned.decider = std::make_unique<umbra::policy::PolicyDecider>(learned.model.network);
            learned_.emplace(id, std::move(learned));
            umbra::diagnostics::Info("Learned AI loaded for " + id + " from " + actor.policy);
            continue;
        }
        for (const umbra::core::Diagnostic &d : diagnostics)
            umbra::diagnostics::Error(umbra::core::Format(d));
        Say("Learned AI for " + actor.name + " refused (" + (diagnostics.empty() ? "unknown" : diagnostics.back().message) + "); standard AI plays it");
    }
}

void GameFlow::ReloadContent()
{
    // Only between fights and conversations: those hold references into the content.
    if (session_.GetPhase() != game::Phase::Exploring)
        return;
    reloadPending_ = false;
    umbra::core::Stopwatch watch;
    std::vector<umbra::core::Diagnostic> diagnostics;
    const auto refuse = [&](const std::string &why) {
        for (const umbra::core::Diagnostic &d : diagnostics)
            umbra::diagnostics::Error(umbra::core::Format(d));
        Say("RPG content not reloaded: " + why + (diagnostics.empty() ? "" : " (" + diagnostics.front().message + ")"));
    };

    // 1. The new file must load on its own...
    rpg::Content fresh;
    if (!rpg::LoadContent(contentFile_, fresh, diagnostics))
        return refuse("the file has errors");

    // 2. ...and the running game must fit it: the same check as loading a save made right now.
    const umbra::gameplay::SpawnPoint spot = scene_.GetPlayerSpot();
    const std::optional<game::SaveData> now = session_.MakeSave(scene_.GetScenePath(), spot.feet, spot.yaw);
    const std::filesystem::path check = slots_.directory / "reload-check.sav";
    game::SaveData fitted;
    if (!now || !game::WriteSave(check, *now, diagnostics) || !game::ReadSave(check, fresh, fitted, diagnostics))
        return refuse("the current game uses something it removed");

    // 3. Swap in place (everything refers to this one object) and re-apply the state.
    content_ = std::move(fresh);
    LoadPolicies(); // a changed skill may no longer fit a trained model
    session_.ApplySave(fitted);
    rpg::ClampResources(content_, session_.Hero()); // a lower maximum must not leave extra health
    char text[64];
    std::snprintf(text, sizeof(text), "RPG content reloaded (%.0f ms)", watch.Milliseconds());
    Say(text);
}

// ---------------- Per frame ----------------

void GameFlow::Update(const umbra::platform::Input &input, float deltaSeconds)
{
    contactCooldown_ = std::max(0.0F, contactCooldown_ - deltaSeconds);

    // Edited RPG content (e.g. saved from the editor) is picked up at the next safe moment.
    if (!contentWatcher_.Poll(umbra::assets::FileWatcher::Clock::now()).empty())
        reloadPending_ = true;
    if (reloadPending_)
        ReloadContent();

    // The world was replaced mid-fight (restart, hot reload, checkpoint): its enemies are gone.
    if (session_.GetPhase() == game::Phase::Fighting && scene_.Generation() != fightGeneration_)
        Flee();

    if (startMenu_)
    {
        UpdateStartMenu(input);
        scene_.SetRoamersActive(false);
        return;
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
    if (startMenu_)
        return false;
    const game::Phase phase = session_.GetPhase();
    return phase == game::Phase::Exploring ||
           (phase == game::Phase::Fighting && session_.Mode() == game::CombatMode::Realtime);
}

bool GameFlow::AllowsPortals() const
{
    return !startMenu_ && (session_.GetPhase() == game::Phase::Exploring || session_.GetPhase() == game::Phase::Talking);
}

bool GameFlow::HandleEscape()
{
    if (startMenu_)
        return true; // the start menu stays until a choice is made
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
    const RoamerSlot &touched = scene_.Roamers()[roamer];
    const std::vector<std::string> group = session_.PickGroup(touched.roamer.Actor(), touched.pool, touched.group);
    touched_ = roamer;

    // 2. Go to the battle arena; the dungeon waits, unchanged, until the fight is over.
    if (!scene_.EnterArena(arenaScene))
    {
        Say("Cannot load the battle arena (see warnings)");
        contactCooldown_ = 3.0F;
        return;
    }
    if (!session_.StartFight(group))
    {
        scene_.LeaveArena();
        return;
    }

    // 3. Enemies stand on the arena's enemy markers, facing the player.
    const glm::vec3 player = scene_.GetPlayerSpot().feet;
    fighters_.clear();
    for (std::size_t i = 0; i < group.size(); ++i)
    {
        const std::optional<umbra::gameplay::SpawnPoint> marker = scene_.FindMarker("enemy_" + std::to_string(i + 1));
        const glm::vec3 feet = marker ? marker->feet : player + glm::vec3{2.0F * static_cast<float>(i) - 2.0F, 0.0F, -5.0F};
        const std::size_t index = scene_.AddRoamer(group[i], feet, feet, 0.0F, false);
        scene_.Roamers()[index].yaw = std::atan2(-(player.x - feet.x), -(player.z - feet.z));
        fighters_.push_back(index);
    }

    // 4. Fresh log; enemies wait a moment before their first turn.
    fightGeneration_ = scene_.Generation();
    log_ = {"A fight with " + std::to_string(group.size()) + (group.size() == 1 ? " enemy!" : " enemies!")};
    policyFor_ = static_cast<std::size_t>(-1);
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
    // 2. An enemy's turn: it decides after a short pause, through the same rules. An enemy with a
    //    learned policy asks it in the background when its turn starts; the pause is the deadline.
    else if (current)
    {
        const auto learned = learned_.find(turn->GetRoster()[*current].character->actor);
        if (learned != learned_.end() && policyFor_ != *current)
        {
            const umbra::policy::Contract &contract = learned->second.model.contract;
            policyFor_ = *current;
            policyRequest_ = session_.NextDecisionId();
            policyMask_ = umbra::policy::Mask(*turn, contract, *current, 0);
            if (!learned->second.decider->Request(policyRequest_, umbra::policy::Observe(session_.Content(), *turn, *current, 0, 30), policyMask_))
                policyRequest_ = 0; // busy: this decision falls back
        }
        enemyDelay_ -= deltaSeconds;
        if (enemyDelay_ <= 0.0F)
        {
            rpg::ActionIntent intent = umbra::turn::ChooseTurnAction(session_.Content(), *turn);
            if (learned != learned_.end())
            {
                // Use the answer only if it is for this decision, arrived in time and is legal.
                const std::optional<int> slot = policyRequest_ ? learned->second.decider->Take(policyRequest_) : std::nullopt;
                if (slot && *slot >= 0 && policyMask_[static_cast<std::size_t>(*slot)] != 0)
                    intent = umbra::policy::ToIntent(session_.Content(), learned->second.model.contract, static_cast<std::size_t>(*slot), *current, 0);
                else
                    log_.push_back(FighterName(*current) + ": learned AI gave no usable answer, standard AI acts");
            }
            Act(intent, false);
            enemyDelay_ = enemyTurnDelay;
            policyFor_ = static_cast<std::size_t>(-1);
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

    // 2. The player: 1-4 use skills, Q drinks a potion, F flees.
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
    }

    // 4. Time passes; then see what happened.
    session_.Combat().Update(deltaSeconds);
    ReadEvents(fight->TakeEvents());
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
            // Feedback: whoever was hit flashes red.
            if (e.target == 0)
                scene_.Flash(scene_.GetWorld().Find("player"));
            else if (e.target <= fighters_.size() && scene_.Roamers()[fighters_[e.target - 1]].alive)
                scene_.Flash(scene_.Roamers()[fighters_[e.target - 1]].visual);
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
    // Fleeing always works: back to the dungeon spot. The touched enemy calms down and
    // can't start a new fight for a few seconds, so the player can get away.
    session_.Flee();
    fighters_.clear();
    if (scene_.LeaveArena() && touched_ < scene_.Roamers().size())
        scene_.Roamers()[touched_].roamer.Calm();
    contactCooldown_ = 3.0F;
    Say("You got away");
}

void GameFlow::EndFight(const game::FightResult &result)
{
    fighters_.clear();
    selected_ = 0;
    // Defeat: stay in the arena with the death menu (loading replaces everything).
    if (!result.won)
    {
        Say("You died");
        return;
    }
    // Victory: back to the dungeon; the touched enemy is gone until the area is entered again.
    if (scene_.LeaveArena())
        scene_.RemoveRoamer(touched_);
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

std::vector<std::string> GameFlow::StartMenuItems() const
{
    // "Continue" = the newer of the save-stone save and the autosave; the other one by name.
    std::vector<std::string> items{"Continue"};
    if (std::filesystem::exists(slots_.Manual()) && std::filesystem::exists(slots_.Autosave()))
        items.push_back("Load the other save");
    items.push_back("New game");
    return items;
}

void GameFlow::UpdateStartMenu(const umbra::platform::Input &input)
{
    const std::vector<std::string> items = StartMenuItems();
    if (!MenuInput(input, items.size()))
        return;
    // 1. Which save is newer (a missing file counts as oldest).
    std::error_code ignored;
    const auto time = [&](const std::filesystem::path &file) { return std::filesystem::last_write_time(file, ignored); };
    const bool haveManual = std::filesystem::exists(slots_.Manual()), haveAuto = std::filesystem::exists(slots_.Autosave());
    const bool manualNewer = haveManual && (!haveAuto || time(slots_.Manual()) >= time(slots_.Autosave()));
    const std::filesystem::path newest = manualNewer ? slots_.Manual() : slots_.Autosave();
    const std::filesystem::path other = manualNewer ? slots_.Autosave() : slots_.Manual();
    // 2. Load it, or start fresh where the game opened.
    const std::string &picked = items[static_cast<std::size_t>(selected_)];
    if (picked == "New game" || Load(picked == "Continue" ? newest : other))
    {
        startMenu_ = false;
        selected_ = 0;
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
    if (startMenu_)
    {
        hud.menuTitle = "Umbra";
        hud.menuItems = StartMenuItems();
        hud.menuSelected = selected_;
        hud.menuHint = "W/S choose, Enter";
        return;
    }
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
                    hud.status.push_back(FighterName(i) + ": " + std::to_string((*roster)[i].character->health) + " HP" +
                                         (learned_.contains((*roster)[i].character->actor) && session_.Mode() == game::CombatMode::Turn ? "  (learned AI)" : ""));
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
