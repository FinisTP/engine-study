#pragma once

#if UMBRA_PLUGIN_LIVING_TOWN

#include "SceneRuntime.hpp"

#include <Town.hpp>

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
    // For screens: {"day", "time", "residents": [{"name", "doing", "place", "money", "level"}]}.
    [[nodiscard]] nlohmann::json UiData() const;
    [[nodiscard]] const umbra::town::TownState &State() const { return state_; }

private:
    void ShowResidents();

    SceneRuntime &scene_;
    umbra::town::TownDef def_;
    umbra::town::TownState state_;
    bool ready_ = false;
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
