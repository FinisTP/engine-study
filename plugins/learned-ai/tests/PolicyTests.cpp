#include "RpgTestContent.hpp"
#include "assets/TestFiles.hpp"

#include <PolicyDecider.hpp>
#include <PolicyEnvironment.hpp>
#include <PolicyModel.hpp>

#include <Inventory.hpp>

#include <catch2/catch_approx.hpp>
#include <nlohmann/json.hpp>

#include <cmath>
#include <limits>

using namespace umbra;
using Catch::Approx;

namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }

    // A tiny hand-made network: 12 inputs -> 2 tanh -> 6 scores (one per bandit leader action).
    // h0 = tanh(self.health + 0.5), h1 = tanh(-0.5); rows of `out` are (h0, h1) weights per action.
    policy::Network TinyNetwork()
    {
        policy::Layer hidden{policy::observationSize, 2, true, std::vector<float>(2 * policy::observationSize, 0.0F), {0.5F, -0.5F}};
        hidden.weights[0] = 1.0F; // hidden 0 follows self.health
        policy::Layer out{2, 6, false, {}, {0, 0, 0, 0, 0, 0}};
        out.weights = {0, 0, 1, 0, 2, 0, 0, 1, 0, -1, 0, 0}; // score[a] = w[a][0]*h0 + w[a][1]*h1
        return policy::Network({hidden, out});
    }

    std::string ModelJson(const policy::Contract &contract)
    {
        nlohmann::json actions = nlohmann::json::array();
        for (const policy::ActionSlot &a : contract.actions)
            actions.push_back(a.item.empty() ? nlohmann::json{{"skill", a.skill}} : nlohmann::json{{"item", a.item}});
        const policy::Network net = TinyNetwork();
        nlohmann::json layers = nlohmann::json::array();
        for (const policy::Layer &l : net.Layers())
            layers.push_back({{"in", l.in}, {"out", l.out}, {"activation", l.tanh ? "tanh" : "linear"}, {"weights", l.weights}, {"bias", l.bias}});
        return nlohmann::json{{"format", "umbra.policy"}, {"version", 1}, {"learner", contract.learner}, {"opponent", contract.opponent},
                              {"observation", policy::observationVersion}, {"fingerprint", contract.fingerprint}, {"actions", actions}, {"layers", layers}}
            .dump();
    }
}

TEST_CASE("The action catalogue and fingerprint follow the content")
{
    const policy::Contract contract = policy::MakeContract(C(), "bandit_leader", "hero");
    REQUIRE(contract.actions.size() == 6);
    REQUIRE(contract.actions[0] == policy::ActionSlot{}); // wait first
    REQUIRE(contract.actions[1].skill == "strike");
    REQUIRE(contract.actions[5].item == "health_potion");
    REQUIRE(contract.fingerprint.size() == 16);
    REQUIRE(policy::IsAttack(C(), contract.actions[2]));  // heavy_blow
    REQUIRE_FALSE(policy::IsAttack(C(), contract.actions[3])); // guard

    // A rename changes nothing the actions do; a stronger heavy blow does (T27).
    rpg::Content renamed = C();
    renamed.skills["heavy_blow"].name = "Mighty Blow";
    REQUIRE(policy::MakeContract(renamed, "bandit_leader", "hero").fingerprint == contract.fingerprint);
    rpg::Content stronger = C();
    stronger.skills["heavy_blow"].effects[0].power += 1;
    REQUIRE(policy::MakeContract(stronger, "bandit_leader", "hero").fingerprint != contract.fingerprint);
    REQUIRE(policy::MakeContract(C(), "nobody", "hero").actions.empty());
}

TEST_CASE("The environment runs real fights, repeatably, and sees only what an enemy may see")
{
    policy::PolicyEnvironment env(C(), {});
    REQUIRE(env.Reset(42));
    const std::vector<float> first = env.Observe();
    REQUIRE(first.size() == policy::observationSize);
    for (float v : first)
        REQUIRE((v >= 0.0F && v <= 1.0F));
    const std::vector<std::uint8_t> mask = env.Mask();
    REQUIRE(mask[0] == 1);

    // Same seed, same actions -> same fight (T23: no state leaks between resets).
    const auto play = [&](std::uint32_t seed) {
        env.Reset(seed);
        std::vector<float> trace;
        for (int i = 0; i < 200; ++i)
        {
            const int conventional = env.ConventionalAction();
            const policy::EnvironmentStep step = env.Step(conventional < 0 ? 0 : static_cast<std::size_t>(conventional));
            trace.push_back(step.reward);
            if (step.done || step.truncated)
                break;
        }
        return trace;
    };
    const std::vector<float> a = play(7);
    play(99);
    REQUIRE(play(7) == a);
    REQUIRE(!a.empty());

    // The opponent's bag is hidden: its potions don't change what the learner sees.
    env.Reset(5);
    const std::vector<float> seen = env.Observe();
    rpg::Character &hero = const_cast<rpg::Character &>(*env.Encounter()->GetRoster()[0].character);
    rpg::RemoveItem(hero, "health_potion", rpg::CountItem(hero, "health_potion"));
    REQUIRE(env.Observe() == seen);

    // An illegal choice is counted and played as waiting, never as a forbidden action.
    env.Reset(5);
    env.Step(99);
    REQUIRE(env.IllegalActions() == 1);
}

TEST_CASE("Expensive actions are masked when energy runs out")
{
    policy::PolicyEnvironment env(C(), {"bandit_leader", "hero", 30, 0.0F, false});
    REQUIRE(env.Reset(1));
    const std::size_t heavy = 2;
    for (int i = 0; i < 20 && env.Mask()[heavy] == 1; ++i)
        if (env.Step(heavy).done)
            break;
    if (!env.Encounter()->IsOver())
    {
        const rpg::Character &self = *env.Encounter()->GetRoster()[1].character;
        REQUIRE(self.energy < C().FindSkill("heavy_blow")->energyCost);
        REQUIRE(env.Mask()[heavy] == 0);
    }
}

TEST_CASE("The network scores actions and only picks legal ones")
{
    const policy::Network net = TinyNetwork();
    std::vector<float> obs(policy::observationSize, 0.0F);
    obs[0] = 1.0F; // h0 = tanh(1 + 0.5), h1 = tanh(-0.5)
    const std::vector<float> scores = net.Forward(obs);
    REQUIRE(scores.size() == 6);
    REQUIRE(scores[1] == Approx(std::tanh(1.5F)));
    REQUIRE(scores[2] == Approx(2.0F * std::tanh(1.5F)));
    REQUIRE(scores[3] == Approx(std::tanh(-0.5F)));
    REQUIRE(net.Choose(obs, {1, 1, 1, 1, 1, 1}) == 2); // 2 * tanh(1.5) is the largest score
    REQUIRE(net.Choose(obs, {1, 1, 0, 1, 1, 1}) == 1); // without it: tanh(1.5)
    REQUIRE(net.Choose(obs, {0, 0, 0, 0, 0, 0}) == -1); // T25: nothing legal
    REQUIRE(net.Forward(std::vector<float>(5)).empty());
    std::vector<float> broken = obs;
    broken[0] = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(net.Choose(broken, {1, 1, 1, 1, 1, 1}) == -1); // non-finite output -> fallback
}

TEST_CASE("Model files are checked on their own and against the content")
{
    const auto dir = test::TempDir("policy-model");
    const policy::Contract contract = policy::MakeContract(C(), "bandit_leader", "hero");
    test::WriteText(dir / "ok.policy.json", ModelJson(contract));
    policy::PolicyModel model;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(policy::LoadPolicy(dir / "ok.policy.json", model, diagnostics));
    REQUIRE(policy::CheckCompatible(model, C(), "ok", diagnostics));

    // Content changed after training (T26/T27): refused with a reason.
    rpg::Content stronger = C();
    stronger.skills["guard"].energyCost = 1;
    REQUIRE_FALSE(policy::CheckCompatible(model, stronger, "ok", diagnostics));
    REQUIRE(diagnostics.back().code == "policy.fingerprint");
    rpg::Content fewer = C();
    fewer.actors["bandit_leader"].skills.pop_back();
    REQUIRE_FALSE(policy::CheckCompatible(model, fewer, "ok", diagnostics));
    REQUIRE(diagnostics.back().code == "policy.actions");

    // Broken files never load.
    nlohmann::json bad = nlohmann::json::parse(ModelJson(contract));
    bad["layers"][0]["in"] = 11;
    test::WriteText(dir / "shape.policy.json", bad.dump());
    REQUIRE_FALSE(policy::LoadPolicy(dir / "shape.policy.json", model, diagnostics));
    REQUIRE(diagnostics.back().code == "policy.shape");
    test::WriteText(dir / "junk.policy.json", "{ not json");
    REQUIRE_FALSE(policy::LoadPolicy(dir / "junk.policy.json", model, diagnostics));
    REQUIRE_FALSE(policy::LoadPolicy(dir / "missing.policy.json", model, diagnostics));
}

TEST_CASE("Decisions are tagged: late, stale and overlapping requests are controlled")
{
    using namespace std::chrono_literals;
    std::vector<float> obs(policy::observationSize, 0.0F);
    obs[0] = 1.0F;
    const std::vector<std::uint8_t> all{1, 1, 1, 1, 1, 1};

    // 1. A normal answer arrives for its own id only.
    {
        policy::PolicyDecider decider(TinyNetwork());
        REQUIRE(decider.Request(1, obs, all));
        std::optional<int> answer;
        for (int i = 0; i < 500 && !answer; ++i, std::this_thread::sleep_for(1ms))
            answer = decider.Take(1);
        REQUIRE(answer == 2);
    }
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    // 2. Browser build without threads (P10): the answer is there at once; a stale id is still dropped.
    {
        policy::PolicyDecider decider(TinyNetwork());
        REQUIRE(decider.Request(1, obs, all));
        REQUIRE_FALSE(decider.Take(2));
        REQUIRE_FALSE(decider.Take(1)); // gone
        REQUIRE(decider.Request(3, obs, {0, 0, 0, 0, 0, 0}));
        REQUIRE(decider.Take(3) == -1);
    }
#else
    // 2. A slow model (T28): nothing before the deadline; busy while working; the late answer
    //    for decision 1 is never taken as the answer to decision 2.
    {
        policy::PolicyDecider decider(TinyNetwork(), 50ms);
        REQUIRE(decider.Request(1, obs, all));
        REQUIRE_FALSE(decider.Request(2, obs, all)); // overload: one job at a time
        REQUIRE_FALSE(decider.Take(1));              // not ready yet: the caller falls back
        std::this_thread::sleep_for(120ms);
        REQUIRE_FALSE(decider.Take(2)); // stale answer (for 1) is dropped
        REQUIRE_FALSE(decider.Take(1)); // and gone
        REQUIRE(decider.Request(3, obs, {0, 0, 0, 0, 0, 0}));
        std::this_thread::sleep_for(120ms);
        REQUIRE(decider.Take(3) == -1); // nothing legal: the caller falls back
    }
    // 3. Shutdown while a job runs is clean (destructor joins).
    {
        policy::PolicyDecider decider(TinyNetwork(), 30ms);
        REQUIRE(decider.Request(1, obs, all));
    }
#endif
}

TEST_CASE("The shipped model matches its training scores and plays complete legal fights")
{
    // T29: the engine's C++ network agrees with a NumPy forward pass of the same file.
    const std::filesystem::path assets{UMBRA_TEST_ASSET_DIRECTORY};
    policy::PolicyModel model;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(policy::LoadPolicy(assets / "policies/bandit_leader.policy.json", model, diagnostics));
    REQUIRE(policy::CheckCompatible(model, C(), "shipped", diagnostics));

    const nlohmann::json parity = nlohmann::json::parse(test::ReadText(std::filesystem::path{UMBRA_TEST_DATA_DIRECTORY} / "parity.json"));
    REQUIRE(parity["cases"].size() >= 100);
    for (const nlohmann::json &c : parity["cases"])
    {
        const std::vector<float> obs = c["observation"].get<std::vector<float>>();
        const std::vector<float> expected = c["scores"].get<std::vector<float>>();
        const std::vector<std::uint8_t> mask = c["mask"].get<std::vector<std::uint8_t>>();
        const std::vector<float> scores = model.network.Forward(obs);
        REQUIRE(scores.size() == expected.size());
        int best = -1;
        for (std::size_t a = 0; a < scores.size(); ++a)
        {
            REQUIRE(scores[a] == Approx(expected[a]).margin(1e-4));
            if (mask[a] && (best < 0 || expected[a] > expected[static_cast<std::size_t>(best)]))
                best = static_cast<int>(a);
        }
        REQUIRE(model.network.Choose(obs, mask) == best);
    }

    // Native encounters: the model alone, through the real rules, never picks an illegal action.
    policy::PolicyEnvironment env(C(), {"bandit_leader", "hero", 30, 0.0F, true, 5});
    for (std::uint32_t seed = 0; seed < 200; ++seed)
    {
        REQUIRE(env.Reset(20'000'000 + seed));
        bool over = false;
        for (int turn = 0; turn < 100 && !over; ++turn)
        {
            const int action = model.network.Choose(env.Observe(), env.Mask());
            REQUIRE(action >= 0);
            const policy::EnvironmentStep step = env.Step(static_cast<std::size_t>(action));
            over = step.done || step.truncated;
        }
        REQUIRE(over);
        REQUIRE(env.IllegalActions() == 0);
    }
}
