#pragma once

#if UMBRA_PLUGIN_LIVING_TOWN

#include "SceneRuntime.hpp"

#include <Storyteller.hpp>
#include <Town.hpp>
#include <TownMind.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

// The living town in the running game (S21, plugins/living-town Town.md "In the game"): keeps the
// town's clock in step with the world's time of day, shows the residents whose place is in the
// area the player is in, and puts the town into saves. The town's rules know nothing of this.
class TownFlow final
{
public:
    // Loads the project's town: the first `town/*.town.json` under `assets`. No file = no town.
    TownFlow(const std::filesystem::path &assets, const umbra::rpg::Content &content, SceneRuntime &scene);
    [[nodiscard]] bool Ready() const { return ready_; }
    [[nodiscard]] const std::vector<std::string> &Problems() const { return problems_; }

    // A new town with a seed of its own, lived up to the world's hour (a game starts at dusk).
    void NewGame();
    // Once per frame: the world's clock moved, so the town lives those minutes; residents here are shown.
    void Update();
    // The town as JSON text for the save, and back (false = not this town: a new one is started).
    [[nodiscard]] std::string Save() const;
    bool Load(const std::string &text);
    // The resident with a body within reach of `feet` (-1 = nobody), and talking to them (S22):
    // the line they say. Talking can ask or settle a favour and adds to the chronicle.
    [[nodiscard]] int Near(glm::vec3 feet) const;
    [[nodiscard]] std::string NameOf(int resident) const { return def_.residents[static_cast<std::size_t>(resident)].name; }
    std::string Talk(int resident) { return mind_ ? mind_->Talk(def_, state_, resident) : umbra::town::TalkTo(def_, state_, resident); }
    // Coming near someone: their next line is worded ahead by the model, if there is one (S23).
    void Approach(int resident)
    {
        if (mind_)
            mind_->Prepare(def_, state_, resident);
    }
    // For screens: {"day", "time", "residents": [{"name", "doing", "place", "money", "level"}],
    // "chronicle": [lines, newest first], "noChronicle"}.
    [[nodiscard]] nlohmann::json UiData() const;
    [[nodiscard]] const umbra::town::TownState &State() const { return state_; }
    [[nodiscard]] const umbra::town::StoryState &Story() const { return story_; }

private:
    void ShowResidents();

    SceneRuntime &scene_;
    umbra::town::TownDef def_;
    umbra::town::TownState state_;
    bool ready_ = false;
    umbra::town::PlanDef plan_; // the outcome plan and how far its plot is (S24); no plan file = no beats
    umbra::town::StoryState story_;
    bool hasPlan_ = false;
    std::unique_ptr<umbra::town::TownMind> mind_; // the built-in model, when its file is there (S23)
    std::vector<std::string> problems_;
    float lastHour_ = -1.0F; // the world's hour at the last update (-1 = none yet)
    float owed_ = 0.0F;      // parts of a tick not lived yet
    struct Shown final
    {
        std::size_t roamer = static_cast<std::size_t>(-1);
        int place = -1;
    };
    std::vector<Shown> shown_; // per resident: the body walking in this area, if any
    int generation_ = -1;      // the scene these bodies belong to
};

#endif
