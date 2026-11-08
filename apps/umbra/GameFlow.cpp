#include "GameFlow.hpp"

#include <Log.hpp>
#include <Platform.hpp>
#include <Profiling.hpp>
#include <Inventory.hpp>
#include <SkillTree.hpp>
// Optional plugins (UMBRA_PLUGIN_* from the build): each combat mode and the learned enemy.
#if UMBRA_PLUGIN_LEARNED_AI
#include <PolicyContract.hpp>
#endif
#if UMBRA_PLUGIN_REALTIME_COMBAT
#include <RealtimeEncounter.hpp>
#endif
#if UMBRA_PLUGIN_TURN_COMBAT
#include <TurnEncounter.hpp>
#endif

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

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
#if UMBRA_PLUGIN_LEARNED_AI
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
#endif
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
        scene_.SetRoamersActive(false); // the start screen waits for a choice (HandleUi)
        return;
    }
    switch (session_.GetPhase())
    {
    case game::Phase::Exploring:
        UpdateExploring(input);
        break;
    case game::Phase::Talking:
        break; // answers come from the conversation screen (HandleUi)
    case game::Phase::Fighting:
#if UMBRA_PLUGIN_TURN_COMBAT
        if (session_.Mode() == game::CombatMode::Turn)
            UpdateTurnFight(input, deltaSeconds);
#endif
#if UMBRA_PLUGIN_REALTIME_COMBAT
        if (session_.Mode() == game::CombatMode::Realtime)
            UpdateRealtimeFight(input, deltaSeconds);
#endif
        if (std::optional<game::FightResult> result = session_.CheckFightEnd())
            EndFight(*result);
        break;
    case game::Phase::Dead:
        break; // the death screen (HandleUi)
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
        if (!session_.Talk(interactable.conversation))
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

void GameFlow::Answer(std::size_t visibleIndex)
{
    // The conversation screen's choice: the n-th visible answer (none visible: the line ends it).
    umbra::story::Conversation *talk = session_.CurrentConversation();
    if (talk == nullptr)
        return;
    const std::vector<std::size_t> visible = talk->VisibleChoices(session_.Quests(), session_.Hero());
    if (visible.empty())
        return session_.EndTalk();
    if (visibleIndex >= visible.size())
        return;
    const umbra::story::ChoiceResult result = session_.Choose(visible[visibleIndex]);
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
    enemyDelay_ = enemyTurnDelay;
    fightStep_.clear();
    pendingSkill_.clear();
}

void GameFlow::Act(rpg::ActionIntent intent, bool player)
{
    intent.id = session_.NextDecisionId();
    const rpg::Rejection rejection = session_.Combat().Submit(session_.FightId(), intent);
    if (player && rejection != rpg::Rejection::None)
        Say(std::string{"Can't: "} + rpg::ToString(rejection));
}

#if UMBRA_PLUGIN_TURN_COMBAT
void GameFlow::UpdateTurnFight(const umbra::platform::Input &input, float deltaSeconds)
{
    umbra::turn::TurnEncounter *turn = session_.Combat().Turn();
    if (turn == nullptr)
        return;
    if (input.IsKeyPressed(InputKey::F))
        return Flee();

    // The hero's turn comes from the fight screen (HandleUi). An enemy's turn: it decides after a
    // short pause, through the same rules. An enemy with a learned policy asks it in the background
    // when its turn starts; the pause is the deadline.
    const std::optional<std::size_t> current = turn->Current();
    if (current && *current != 0)
    {
#if UMBRA_PLUGIN_LEARNED_AI
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
#endif
        enemyDelay_ -= deltaSeconds;
        if (enemyDelay_ <= 0.0F)
        {
            rpg::ActionIntent intent = umbra::turn::ChooseTurnAction(session_.Content(), *turn);
#if UMBRA_PLUGIN_LEARNED_AI
            if (learned != learned_.end())
            {
                // Use the answer only if it is for this decision, arrived in time and is legal.
                const std::optional<int> slot = policyRequest_ ? learned->second.decider->Take(policyRequest_) : std::nullopt;
                if (slot && *slot >= 0 && policyMask_[static_cast<std::size_t>(*slot)] != 0)
                    intent = umbra::policy::ToIntent(session_.Content(), learned->second.model.contract, static_cast<std::size_t>(*slot), *current, 0);
                else
                    log_.push_back(FighterName(*current) + ": learned AI gave no usable answer, standard AI acts");
            }
#endif
            Act(intent, false);
            enemyDelay_ = enemyTurnDelay;
            policyFor_ = static_cast<std::size_t>(-1);
        }
    }
    ReadEvents(turn->TakeEvents());
}
#endif

#if UMBRA_PLUGIN_REALTIME_COMBAT
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

    // 2. The player: keys 1-4 use what the hotkeys screen put on them (H), F flees.
    const InputKey keys[] = {InputKey::Num1, InputKey::Num2, InputKey::Num3, InputKey::Num4};
    for (std::size_t i = 0; i < 4; ++i)
        if (input.IsKeyPressed(keys[i]))
            UseHotkey(i);
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
#endif

void GameFlow::ReadEvents(const rpg::Events &events)
{
    // Fight lines for the log; a defeated enemy leaves the world at once.
    for (const rpg::Event &e : events)
    {
        std::string line;
        switch (e.kind)
        {
        case rpg::EventKind::Damage: {
            line = FighterName(e.actor) + " hit " + FighterName(e.target) + " for " + std::to_string(e.amount);
            const rpg::SkillDef *skill = session_.Content().FindSkill(e.what);
            sounds_.push_back(skill && !skill->sound.empty() ? skill->sound : "audio/hit.wav");
            // Feedback: whoever was hit flashes red.
            if (e.target == 0)
                scene_.Flash(scene_.GetWorld().Find("player"));
            else if (e.target <= fighters_.size() && scene_.Roamers()[fighters_[e.target - 1]].alive)
                scene_.Flash(scene_.Roamers()[fighters_[e.target - 1]].visual);
            break;
        }
        case rpg::EventKind::Heal:
            line = FighterName(e.target) + " healed " + std::to_string(e.amount);
            break;
        case rpg::EventKind::ItemUsed:
            line = FighterName(e.actor) + " used " + ItemList(session_.Content(), {{e.what, 1}});
            if (const rpg::ItemDef *item = session_.Content().FindItem(e.what); item && !item->sound.empty())
                sounds_.push_back(item->sound);
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

void GameFlow::Restart()
{
    // A fresh start in the first area.
    session_.NewGame();
    if (!scene_.ChangeScene(startScene_, "player"))
        Say("Cannot load " + startScene_);
}

void GameFlow::StartChoice(const std::string &action)
{
    // "continue" = the newer of the save-stone save and the autosave; "other" = the other one.
    std::error_code ignored;
    const auto time = [&](const std::filesystem::path &file) { return std::filesystem::last_write_time(file, ignored); };
    const bool haveManual = std::filesystem::exists(slots_.Manual()), haveAuto = std::filesystem::exists(slots_.Autosave());
    const bool manualNewer = haveManual && (!haveAuto || time(slots_.Manual()) >= time(slots_.Autosave()));
    const std::filesystem::path newest = manualNewer ? slots_.Manual() : slots_.Autosave();
    const std::filesystem::path other = manualNewer ? slots_.Autosave() : slots_.Manual();
    if (action == "new" || Load(action == "continue" ? newest : other))
        startMenu_ = false;
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
    if (saved)
        umbra::platform::Platform::PersistFiles(); // browser: into IndexedDB
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
#if UMBRA_PLUGIN_TURN_COMBAT
    if (umbra::turn::TurnEncounter *turn = session_.Combat().Turn())
        return &turn->GetRoster();
#endif
#if UMBRA_PLUGIN_REALTIME_COMBAT
    if (umbra::realtime::RealtimeEncounter *fight = session_.Combat().Realtime())
        return &fight->GetRoster();
#endif
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

std::vector<float> GameFlow::FighterHead(std::size_t index)
{
    // Roster 0 is the hero; enemy i stands where its arena roamer is.
    glm::vec3 feet{0.0F};
    if (index == 0)
        feet = scene_.GetPlayer().GetFeet(scene_.GetPhysics());
    else if (index - 1 < fighters_.size())
        feet = scene_.RoamerFeet(fighters_[index - 1]);
    return {feet.x, feet.y + 2.1F, feet.z};
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

std::string GameFlow::LearnedTag([[maybe_unused]] std::size_t index)
{
    // "(learned AI)" after an enemy that a learned policy plays (turn fights only).
#if UMBRA_PLUGIN_LEARNED_AI
    const rpg::Roster *roster = FightRoster();
    if (roster && index < roster->size() && learned_.contains((*roster)[index].character->actor) && session_.Mode() == game::CombatMode::Turn)
        return "  (learned AI)";
#endif
    return {};
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

std::vector<std::string> &GameFlow::Hotkeys()
{
    // A new game (or an old save) gets the default set: the real-time skills, a potion on the last key.
    std::vector<std::string> &keys = session_.Hotkeys();
    if (keys.empty())
    {
        keys.clear();
        for (const std::string &skill : RealtimeSkills())
            if (keys.size() < 3)
                keys.push_back("skill:" + skill);
        if (const std::string potion = PotionItem(); !potion.empty())
            keys.push_back("item:" + potion);
    }
    keys.resize(4);
    return keys;
}

nlohmann::json GameFlow::HotkeyEntry(const std::string &key)
{
    const rpg::Content &content = session_.Content();
    if (key.starts_with("skill:"))
    {
        const rpg::SkillDef *skill = content.FindSkill(key.substr(6));
        return {{"id", key}, {"name", skill ? skill->name : key.substr(6)}, {"kind", "Skill"},
                {"detail", skill && skill->energyCost > 0 ? std::to_string(skill->energyCost) + " EN" : ""}};
    }
    if (key.starts_with("item:"))
    {
        const rpg::ItemDef *item = content.FindItem(key.substr(5));
        return {{"id", key}, {"name", item ? item->name : key.substr(5)}, {"kind", "Item"},
                {"detail", "x" + std::to_string(rpg::CountItem(session_.Hero(), key.substr(5)))}};
    }
    return {{"id", ""}, {"name", "(empty)"}, {"kind", ""}, {"detail", ""}};
}

void GameFlow::UseHotkey(std::size_t slot)
{
    // Through the same rules as everything else; the fight says why it can't (no energy, cooldown...).
    const std::string key = Hotkeys()[slot];
    if (key.starts_with("skill:"))
        Act(rpg::ActionIntent{0, 0, key.substr(6), "", 0}, true);
    else if (key.starts_with("item:"))
    {
        if (rpg::CountItem(session_.Hero(), key.substr(5)) <= 0)
            Say("None left: " + HotkeyEntry(key)["name"].get<std::string>());
        else
            Act(rpg::ActionIntent{0, 0, "", key.substr(5), 0}, true);
    }
    else
        Say("Nothing on key " + std::to_string(slot + 1) + " (H: hotkeys)");
}

std::string GameFlow::Screen() const
{
    // The game screen this moment needs (the rpg-ui plugin's screens; ui/README.md there).
    if (startMenu_)
        return "start";
    switch (session_.GetPhase())
    {
    case game::Phase::Talking:
        return "conversation";
    case game::Phase::Dead:
        return "death";
    case game::Phase::Fighting:
#if UMBRA_PLUGIN_TURN_COMBAT
        if (umbra::turn::TurnEncounter *turn = const_cast<game::GameSession &>(session_).Combat().Turn())
            if (turn->Current() && *turn->Current() == 0)
                return fightStep_.empty() ? "fight" : "fight-" + fightStep_;
#endif
        return {};
    case game::Phase::Exploring:
        return {};
    }
    return {};
}

bool GameFlow::HandleUi(const std::string &screen, const std::string &action, const std::string &arg)
{
    // What the player chose on a game screen. Numbers in `arg` are list positions.
    const auto number = [&] { return arg.empty() ? std::size_t{0} : static_cast<std::size_t>(std::stoul(arg)); };
    if (screen == "start")
        StartChoice(action);
    else if (screen == "conversation")
        action == "leave" ? session_.EndTalk() : Answer(number());
    else if (screen == "death")
    {
        if (action == "load-manual")
            (void)Load(slots_.Manual());
        else if (action == "load-auto")
            (void)Load(slots_.Autosave());
        else
            Restart();
    }
#if UMBRA_PLUGIN_TURN_COMBAT
    else if (screen.starts_with("fight"))
    {
        // The hero's turn: Attack -> a move -> a target arrow, or Item -> an item, or Flee.
        // "back" steps back one menu. Only legal actions are offered, so a choice always fits.
        umbra::turn::TurnEncounter *turn = session_.Combat().Turn();
        const auto choose = [&](const auto &fits) {
            for (const rpg::ActionIntent &intent : turn->LegalActions())
                if (fits(intent))
                {
                    Act(intent, true);
                    enemyDelay_ = enemyTurnDelay;
                    fightStep_.clear();
                    return;
                }
        };
        if (action == "flee" || turn == nullptr)
            Flee();
        else if (action == "attack" || action == "items")
            fightStep_ = action == "attack" ? "moves" : "items";
        else if (action == "back")
            fightStep_ = fightStep_ == "target" ? "moves" : "";
        else if (action == "move")
        {
            pendingSkill_ = arg;
            fightStep_ = "target";
        }
        else if (action == "target")
            choose([&](const rpg::ActionIntent &i) { return i.item.empty() && i.skill == pendingSkill_ && i.target == number(); });
        else if (action == "item")
            choose([&](const rpg::ActionIntent &i) { return i.item == arg; });
    }
#endif
    else if (screen == "inventory" && session_.GetPhase() == game::Phase::Exploring)
    {
        rpg::Character &hero = session_.Hero();
        if (action == "equip" && !rpg::Equip(session_.Content(), hero, arg))
            Say("That can't be equipped");
        if (action == "unequip" && !rpg::Unequip(session_.Content(), hero, arg == "weapon" ? rpg::Slot::Weapon : rpg::Slot::Armor))
            Say("Your bags are full");
    }
    else if (screen == "hotkeys")
    {
        // Pick a key, then what goes on it ("" clears it).
        if (action == "slot")
            hotkeySlot_ = std::min<std::size_t>(number(), 3);
        else if (action == "assign")
        {
            Hotkeys()[hotkeySlot_] = arg;
            Say("Key " + std::to_string(hotkeySlot_ + 1) + ": " + HotkeyEntry(arg)["name"].get<std::string>());
        }
    }
    else if (screen == "skills" && action == "unlock" && session_.GetPhase() == game::Phase::Exploring)
    {
        const rpg::UnlockResult result = rpg::Unlock(session_.Content(), session_.Hero(), arg);
        const rpg::SkillDef *skill = session_.Content().FindSkill(arg);
        Say(result == rpg::UnlockResult::Unlocked ? "Learned " + (skill ? skill->name : arg) : std::string{"Can't learn it: "} + rpg::ToString(result));
    }
    else
        return false;
    return true;
}

nlohmann::json GameFlow::UiData()
{
    // Everything the screens show, as one JSON object (bindings like {hero.health}).
    using nlohmann::json;
    const rpg::Content &content = session_.Content();
    rpg::Character &hero = session_.Hero();
    const rpg::StatBlock stats = rpg::DerivedStats(content, hero);
    json data;
    const rpg::ActorDef *heroDef = content.FindActor(hero.actor);

    // 1. The hero, the combat mode.
    data["hero"] = {{"name", heroDef ? heroDef->name : "Hero"}, {"level", hero.level}, {"health", hero.health}, {"maxHealth", rpg::Get(stats, rpg::Stat::MaxHealth)},
                    {"energy", hero.energy}, {"maxEnergy", rpg::Get(stats, rpg::Stat::MaxEnergy)}, {"xp", hero.xp}, {"nextXp", rpg::XpForLevel(hero.level + 1)},
                    {"skillPoints", hero.skillPoints},
                    {"stats", {{"attack", rpg::Get(stats, rpg::Stat::Attack)}, {"defense", rpg::Get(stats, rpg::Stat::Defense)}, {"speed", rpg::Get(stats, rpg::Stat::Speed)}}}};
    data["mode"] = session_.Mode() == game::CombatMode::Turn ? "turn-based" : "real-time";
    const bool exploring = !startMenu_ && session_.GetPhase() == game::Phase::Exploring;
    data["switchHint"] = exploring && game::hasTurnMode && game::hasRealtimeMode ? "   [Tab] switch" : "";
    data["exploring"] = exploring;
    data["canManage"] = exploring;

    // 2. Quests: the journal and the tracker.
    data["quests"] = json::array();
    data["activeQuests"] = json::array();
    for (const auto &[id, progress] : session_.Quests().All())
    {
        const rpg::QuestDef *quest = content.FindQuest(id);
        if (quest == nullptr || progress.state == rpg::QuestState::NotStarted)
            continue;
        const bool active = progress.state == rpg::QuestState::Active && progress.objective < quest->objectives.size();
        const std::string objective = active ? quest->objectives[progress.objective].text : "";
        const char *state = progress.state == rpg::QuestState::Completed ? "Completed" : progress.state == rpg::QuestState::Failed ? "Failed" : "Active";
        data["quests"].push_back({{"name", quest->name}, {"description", quest->description}, {"state", state}, {"objective", objective}});
        if (active)
            data["activeQuests"].push_back({{"name", quest->name}, {"objective", objective}});
    }
    data["noQuests"] = data["quests"].empty();

    // 3. Bag and equipment.
    data["inventory"] = json::array();
    int used = 0;
    for (const rpg::ItemStack &stack : hero.inventory)
    {
        const rpg::ItemDef *item = content.FindItem(stack.item);
        ++used;
        const bool equippable = item && item->slot != rpg::Slot::None;
        data["inventory"].push_back({{"id", stack.item}, {"name", item ? item->name : stack.item}, {"count", stack.count},
                                     {"cannotEquip", !equippable},
                                     {"hint", equippable ? (item->slot == rpg::Slot::Weapon ? "Weapon - choose to equip" : "Armor - choose to equip")
                                              : item && !item->use.empty() ? "Use it in fights" : "Item"}});
    }
    data["bagUsed"] = used;
    data["bagSize"] = hero.inventorySlots;
    data["equipment"] = json::array();
    for (const auto &[slot, label] : {std::pair{rpg::Slot::Weapon, "Weapon"}, std::pair{rpg::Slot::Armor, "Armor"}})
    {
        const auto it = hero.equipment.find(slot);
        const rpg::ItemDef *item = it == hero.equipment.end() ? nullptr : content.FindItem(it->second);
        data["equipment"].push_back({{"slot", slot == rpg::Slot::Weapon ? "weapon" : "armor"}, {"label", label},
                                     {"name", item ? item->name : it == hero.equipment.end() ? "(nothing)" : it->second}});
    }

    // 4. Skills: unlocked, unlockable, or what is missing.
    data["skills"] = json::array();
    for (const auto &[id, skill] : content.skills)
    {
        const bool unlocked = std::find(hero.skills.begin(), hero.skills.end(), id) != hero.skills.end();
        const bool can = !unlocked && rpg::CanUnlock(content, hero, id);
        std::string detail = std::to_string(skill.energyCost) + " EN";
        detail += skill.turn && skill.realtime ? ", both modes" : skill.turn ? ", turn-based" : ", real-time";
        if (!skill.prerequisites.empty())
        {
            detail += ", needs";
            for (const std::string &p : skill.prerequisites)
                detail += " " + (content.FindSkill(p) ? content.FindSkill(p)->name : p);
        }
        data["skills"].push_back({{"id", id}, {"name", skill.name}, {"unlocked", unlocked}, {"locked", !unlocked && !can},
                                  {"state", unlocked ? "Learned" : can ? "Unlock for " + std::to_string(skill.pointCost) + " point(s)"
                                                                       : "Needs " + std::to_string(skill.pointCost) + " point(s) and its requirements"},
                                  {"detail", detail}});
    }

    // 5. A conversation.
    if (umbra::story::Conversation *talk = session_.CurrentConversation(); talk && session_.GetPhase() == game::Phase::Talking)
    {
        json choices = json::array();
        for (std::size_t choice : talk->VisibleChoices(session_.Quests(), hero))
            choices.push_back(talk->Node().choices[choice].text);
        data["talk"] = {{"speaker", talk->Node().speaker.empty() ? "..." : talk->Node().speaker}, {"text", talk->Node().text},
                        {"choices", choices}, {"leaveOnly", choices.empty()}};
    }

    // 6. A fight: enemies, the log, the hero's turn menu or the real-time keys.
    if (session_.GetPhase() == game::Phase::Fighting)
    {
        json fight{{"log", log_}, {"enemies", json::array()}, {"actions", json::array()}, {"keys", ""}};
        if (const rpg::Roster *roster = FightRoster())
            for (std::size_t i = 1; i < roster->size(); ++i)
                if (const rpg::Character &c = *(*roster)[i].character; c.IsAlive())
                    fight["enemies"].push_back({{"name", FighterName(i)}, {"health", c.health},
                                                {"maxHealth", rpg::Get(rpg::DerivedStats(content, c), rpg::Stat::MaxHealth)}, {"tag", LearnedTag(i)}});
#if UMBRA_PLUGIN_TURN_COMBAT
        if (umbra::turn::TurnEncounter *turn = session_.Combat().Turn())
        {
            fight["round"] = turn->Round();
            // The turn menu: each attack and item once, then the targets of the chosen attack.
            json moves = json::array(), items = json::array(), targets = json::array();
            std::set<std::string> seen;
            for (const rpg::ActionIntent &intent : turn->LegalActions())
            {
                if (!intent.item.empty() && seen.insert("i:" + intent.item).second)
                {
                    const rpg::ItemDef *item = content.FindItem(intent.item);
                    items.push_back({{"id", intent.item}, {"name", item ? item->name : intent.item}, {"count", rpg::CountItem(hero, intent.item)}});
                }
                else if (intent.item.empty() && !intent.skill.empty() && seen.insert("s:" + intent.skill).second)
                {
                    const rpg::SkillDef *skill = content.FindSkill(intent.skill);
                    moves.push_back({{"id", intent.skill}, {"name", skill ? skill->name : intent.skill},
                                     {"cost", skill && skill->energyCost > 0 ? std::to_string(skill->energyCost) + " EN" : ""}});
                }
                if (intent.item.empty() && intent.skill == pendingSkill_)
                    targets.push_back({{"index", intent.target}, {"name", FighterName(intent.target)}, {"world", FighterHead(intent.target)}});
            }
            fight["moves"] = moves;
            fight["items"] = items;
            fight["noItems"] = items.empty();
            fight["targets"] = targets;
            if (const rpg::SkillDef *skill = content.FindSkill(pendingSkill_))
                fight["move"] = skill->name;
            if (const std::optional<std::size_t> current = turn->Current(); current && *current != 0)
                data["prompt"] = FighterName(*current) + " is acting...";
        }
        else
#endif
            fight["realtime"] = true; // the HUD shows the hotkey bar
        data["fight"] = fight;
    }

    // 7. Hotkeys: the four keys, and what can go on them (real-time skills learned, usable items).
    data["hotkeys"] = json::array();
    const std::vector<std::string> &keys = Hotkeys();
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        json entry = HotkeyEntry(keys[i]);
        entry["key"] = std::to_string(i + 1);
        entry["slot"] = i;
        entry["chosen"] = i == hotkeySlot_;
        data["hotkeys"].push_back(entry);
    }
    data["assignables"] = json::array();
    for (const std::string &skill : RealtimeSkills())
        data["assignables"].push_back(HotkeyEntry("skill:" + skill));
    for (const rpg::ItemStack &stack : hero.inventory)
        if (const rpg::ItemDef *item = content.FindItem(stack.item); item && !item->use.empty())
            data["assignables"].push_back(HotkeyEntry("item:" + stack.item));
    data["hotkeySlot"] = std::to_string(hotkeySlot_ + 1);

    // 8. The start menu: Continue (newest save), the other save if both exist, New game.
    data["start"]["items"] = json::array({{{"label", "Continue"}, {"action", "continue"}}});
    if (std::filesystem::exists(slots_.Manual()) && std::filesystem::exists(slots_.Autosave()))
        data["start"]["items"].push_back({{"label", "Load the other save"}, {"action", "other"}});
    data["start"]["items"].push_back({{"label", "New game"}, {"action", "new"}});
    return data;
}
