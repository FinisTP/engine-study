#include "StageApp.hpp"

#include "DialogApp.hpp"
#include "MatchApp.hpp"
#include "MatchPlay.hpp"

#include <CampaignCafe.hpp>

#include <Log.hpp>

#include <chrono>

int RunStage(GameContext &game, const umbra::assets::AssetFolders &assets, const std::string &id, int refill, std::uint64_t seed,
             const umbra::campaign::Speaker &player, const std::filesystem::path &capture, int frames,
             const umbra::campaign::Save *save, const std::vector<std::string> &party, umbra::campaign::BuiltStage *out)
{
    namespace campaign = umbra::campaign;
    // 1. The rules, the cast, the growth and the stage.
    umbra::match::MatchDef def;
    campaign::Cast cast;
    campaign::Growth growth;
    campaign::StageDef stage;
    std::vector<umbra::core::Diagnostic> d;
    const std::filesystem::path folder = assets.RootOf("campaign/cast.csv") / "campaign";
    const bool loaded = umbra::match::LoadMatch(assets.RootOf("match/rules.json") / "match", def, d) && campaign::LoadCast(folder / "cast.csv", cast, d) &&
                        campaign::LoadGrowth(folder / "growth.csv", growth, d) &&
                        campaign::LoadStage(folder / "stages" / (id + ".json"), cast, def, folder / "scenes", stage, d);
    for (const umbra::core::Diagnostic &e : d)
        umbra::diagnostics::Error(umbra::core::Format(e));
    if (!loaded)
        return 1;
    std::vector<campaign::PartyMember> members;
    for (const char *who : {"player", "bao", "mai"})
        if (static_cast<int>(members.size()) < stage.partyCap)
            members.push_back({who, stage.level + refill, {}}); // a refill comes after a story stage
    campaign::Recipes recipes;
    if (save)
    {
        members = campaign::PartyOf(*save, party);
        (void)campaign::LoadRecipes(folder / "recipes.json", def, recipes, d);
    }

    // 2. The hooks: a fresh fight, the enemy brain, the scenes between beats.
    campaign::BuiltStage built;
    campaign::Moments moments;
    campaign::Facts facts;
    std::uint64_t tries = seed ? seed : static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    MatchHooks hooks;
    // 3. Names from the cast (the player's own, else the nameless default); spawned units get theirs too.
    const auto names = [&] {
        std::vector<std::string> out;
        for (const std::string &who : built.who)
        {
            const campaign::CastMember *m = cast.Find(who == "player" && player.name.empty() ? "nameless" : who);
            out.push_back(who == "player" && !player.name.empty() ? player.name : m ? m->name : who);
        }
        return out;
    };
    const auto sprites = [&] {
        // Each unit's own figure: the player's by gender; a recruit still strung (on the other side) its bound one.
        std::vector<std::string> out;
        int ours = -1;
        for (std::size_t i = 0; i < built.who.size() && i < built.state.units.size(); ++i)
            if (built.who[i] == "player")
                ours = built.state.units[i].faction;
        for (std::size_t i = 0; i < built.who.size(); ++i)
        {
            const campaign::CastMember *m = cast.Find(built.who[i]);
            const bool strung = m && !m->bound.empty() && i < built.state.units.size() && built.state.units[i].faction != ours;
            out.push_back(built.who[i] == "player" ? (player.gender == "male" ? "player_male" : "player_female") : !m ? "" : strung ? m->bound : m->sprite);
        }
        return out;
    };
    hooks.start = [&] {
        built = campaign::BuildStage(def, cast, growth, stage, members, refill, save ? save->taken : std::set<std::string>{}, tries++);
        if (save)
            campaign::ApplySave(def, recipes, *save, built);
        moments = {};
        return built.state;
    };
    hooks.brain = [&](const umbra::match::MatchState &s) {
        built.state = s;
        campaign::Sync(built);
        return campaign::EnemyAction(def, built, s, def.options.bots);
    };
    bool ended = false; // the fight is over and its last scene has played
    hooks.done = [&] { return out != nullptr && ended; };
    hooks.beat = [&](MatchPlay &play) {
        if (!play.Quiet() || !capture.empty())
            return;
        built.state = play.State();
        campaign::Sync(built);
        play.SetCast(names(), sprites()), play.SetHeights(built.height);
        for (const std::string &scene : campaign::StageMoments(stage, built, play.Told(), moments))
            if (RunScene(game, assets, scene, stage.backdrop, player, facts) == 0)
                return;
        if (out && play.State().over && play.Quiet())
            *out = built, ended = true;
    };
    hooks.setup = [&](MatchPlay &play) { play.SetCast(names(), sprites()), play.SetHeights(built.height), play.SetStage(stage.title); };
    return RunMatch(game, assets, seed, false, capture, frames, 0.0F, nullptr, 0, {}, &hooks);
}
