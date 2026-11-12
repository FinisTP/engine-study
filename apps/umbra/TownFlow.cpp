#include "TownFlow.hpp"

#if UMBRA_PLUGIN_LIVING_TOWN

#include <Log.hpp>
#if UMBRA_TOWN_LLAMA
#include <LlamaMind.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

using namespace umbra::town;

TownFlow::TownFlow(const std::filesystem::path &assets, const umbra::rpg::Content &content, SceneRuntime &scene) : scene_(scene)
{
    // The first town file by name (a project has one town).
    std::vector<std::filesystem::path> files;
    std::error_code ignored;
    for (const auto &entry : std::filesystem::directory_iterator(assets / "town", ignored))
        if (entry.path().filename().string().ends_with(".town.json"))
            files.push_back(entry.path());
    if (files.empty())
        return;
    std::sort(files.begin(), files.end());
    std::vector<umbra::core::Diagnostic> diagnostics;
    ready_ = LoadTown(files.front(), &content, def_, diagnostics);
    for (const umbra::core::Diagnostic &diagnostic : diagnostics)
    {
        problems_.push_back(umbra::core::Format(diagnostic));
        umbra::diagnostics::Error(problems_.back());
    }
    // The outcome plan beside it (settlement.town.json -> settlement.plan.json), if there is one:
    // with it the storyteller lets trouble happen and keeps the plot (S24). Without, a quiet town.
    std::filesystem::path planFile = files.front();
    planFile.replace_extension().replace_extension(".plan.json");
    if (ready_ && std::filesystem::is_regular_file(planFile, ignored))
    {
        hasPlan_ = LoadPlan(planFile, def_, plan_, diagnostics);
        for (const umbra::core::Diagnostic &diagnostic : diagnostics)
            umbra::diagnostics::Error(umbra::core::Format(diagnostic));
    }
#if UMBRA_TOWN_LLAMA
    // The built-in model: <project>/models/town.gguf (beside assets, not packed with them).
    // Without the file the town runs on its rules and template lines.
    if (ready_)
    {
        LineGuard guard;
        LoadLineRules(assets / "town/lines.rules.json", guard.rules, diagnostics);
        LlamaOptions options;
        options.threads = std::max(2, static_cast<int>(std::thread::hardware_concurrency()) / 2);
        std::string error;
        if (std::unique_ptr<Mind> model = LoadLlamaMind(assets.parent_path() / "models/town.gguf", options, error))
        {
            mind_ = std::make_unique<TownMind>(std::move(model), std::move(guard));
            umbra::diagnostics::Info("Living town: model " + mind_->ModelName() + " loaded (" + (LlamaHasGpu() ? "graphics card" : "CPU") + ")");
        }
        else
            umbra::diagnostics::Info("Living town: no model (" + error + "); residents use rules and template lines");
    }
#endif
    if (ready_)
        NewGame();
}

void TownFlow::NewGame()
{
    if (!ready_)
        return;
    // The seed is this playthrough's own; it is saved, so a loaded game keeps its town.
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    state_ = NewTown(def_, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count()));
    const float hour = scene_.GetWorld().Hour();
    story_ = NewStory(plan_);
    for (int tick = static_cast<int>(hour * static_cast<float>(ticksPerHour)); tick > 0; --tick)
    {
        Tick(def_, state_);
        if (hasPlan_)
            StoryStep(def_, plan_, state_, story_);
    }
    lastHour_ = hour;
    owed_ = 0.0F;
    generation_ = -1; // bodies are placed again
    if (mind_)
        mind_->Forget();
}

void TownFlow::Update()
{
    if (!ready_)
        return;
    // 1. How far the world's clock moved since the last frame (it wraps at midnight). A jump of
    //    more than an hour is a new area with its own fixed hour or a loaded game, not time passing.
    const float hour = scene_.GetWorld().Hour();
    float passed = lastHour_ < 0.0F ? 0.0F : hour - lastHour_;
    if (passed < 0.0F)
        passed += 24.0F;
    lastHour_ = hour;
    if (passed <= 1.0F)
        owed_ += passed * static_cast<float>(ticksPerHour);
    // 2. Whole ticks are lived; the rest waits.
    //    With a model, residents about to be free are asked what to do; its legal picks are used.
    if (mind_)
        mind_->Collect();
    for (; owed_ >= 1.0F; owed_ -= 1.0F)
    {
        if (mind_)
            mind_->Think(def_, state_);
        Tick(def_, state_, mind_ ? TownMind::Decide : nullptr, mind_.get());
        // The storyteller's turn (once a day); a forced or missed beat is logged for the developer.
        const std::size_t warned = story_.warnings.size();
        if (hasPlan_)
            StoryStep(def_, plan_, state_, story_);
        if (story_.warnings.size() > warned)
            umbra::diagnostics::Info("Living town: " + story_.warnings.back());
    }
    ShowResidents();
}

void TownFlow::ShowResidents()
{
    // A new area: its bodies are gone with the old world.
    if (scene_.Generation() != generation_)
    {
        generation_ = scene_.Generation();
        shown_.assign(def_.residents.size(), {});
    }
    for (std::size_t i = 0; i < def_.residents.size(); ++i)
    {
        const int place = state_.residents[i].place;
        const PlaceDef &where = def_.places[static_cast<std::size_t>(place)];
        const bool here = where.scene == scene_.GetScenePath() && !where.marker.empty() && state_.residents[i].gone == Gone::No;
        Shown &shown = shown_[i];
        // 1. Left this place (or this area): the body goes.
        if (shown.roamer != static_cast<std::size_t>(-1) && (!here || shown.place != place))
        {
            scene_.RemoveRoamer(shown.roamer);
            shown = {};
        }
        // 2. Arrived at a place in this area: a body appears at its marker, a step aside per
        //    resident so people at one place don't stand in each other.
        if (here && shown.roamer == static_cast<std::size_t>(-1))
            if (const auto marker = scene_.FindMarker(where.marker))
            {
                const float angle = static_cast<float>(i) * 2.4F;
                const glm::vec3 feet = marker->feet + glm::vec3{std::cos(angle), 0.0F, std::sin(angle)} * 0.9F;
                shown = {scene_.AddRoamer(def_.residents[i].actor, feet, marker->feet, 1.6F, false), place};
                scene_.Roamers()[shown.roamer].peaceful = true;
            }
    }
}

int TownFlow::Near(glm::vec3 feet) const
{
    int nearest = -1;
    float best = 2.2F; // meters: arm's length and a step
    for (std::size_t i = 0; i < shown_.size() && ready_; ++i)
        if (shown_[i].roamer != static_cast<std::size_t>(-1))
            if (const float distance = glm::length(scene_.RoamerFeet(shown_[i].roamer) - feet); distance < best)
                nearest = static_cast<int>(i), best = distance;
    return nearest;
}

std::string TownFlow::Save() const
{
    if (!ready_)
        return {};
    nlohmann::json saved = SaveTown(def_, state_);
    saved["story"] = SaveStory(story_); // how far the plot is, beside the town it happened in
    return saved.dump();
}

bool TownFlow::Load(const std::string &text)
{
    if (!ready_)
        return false;
    std::vector<umbra::core::Diagnostic> diagnostics;
    TownState loaded;
    const nlohmann::json saved = nlohmann::json::parse(text, nullptr, false);
    const bool ok = !text.empty() && LoadTownState(def_, saved, loaded, diagnostics);
    for (const umbra::core::Diagnostic &diagnostic : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(diagnostic));
    if (ok)
    {
        state_ = std::move(loaded);
        // A save from before the plan (or from another plan): the plot starts from its first beat.
        if (!saved.contains("story") || !LoadStory(plan_, saved["story"], story_))
            story_ = NewStory(plan_);
    }
    else
        NewGame(); // an older save, or one from a changed town file: the town starts over
    lastHour_ = -1.0F;
    owed_ = 0.0F;
    generation_ = -1;
    if (mind_)
        mind_->Forget();
    return ok;
}

nlohmann::json TownFlow::UiData() const
{
    if (!ready_)
        return nullptr;
    char text[16];
    std::snprintf(text, sizeof text, "%02d:%02d", state_.Hour(), state_.Minute());
    nlohmann::json residents = nlohmann::json::array();
    for (std::size_t i = 0; i < def_.residents.size(); ++i)
    {
        const ResidentState &r = state_.residents[i];
        const std::string doing = r.gone == Gone::Left ? "left town" : r.gone == Gone::Dead ? "dead" : def_.activities[static_cast<std::size_t>(r.activity)].name;
        residents.push_back({{"name", def_.residents[i].name}, {"doing", doing + (r.gone == Gone::No && state_.tick < r.illUntil ? " (ill)" : "")},
                             {"place", def_.places[static_cast<std::size_t>(r.place)].name}, {"money", r.money}, {"level", r.level}});
    }
    // The chronicle, newest first (the screen shows the top of the list).
    std::vector<std::string> chronicle = Chronicle(def_, state_);
    std::reverse(chronicle.begin(), chronicle.end());
    if (chronicle.size() > 12)
        chronicle.resize(12);
    return {{"day", state_.Day()}, {"time", text}, {"residents", residents}, {"chronicle", chronicle}, {"noChronicle", chronicle.empty()}};
}

#endif
