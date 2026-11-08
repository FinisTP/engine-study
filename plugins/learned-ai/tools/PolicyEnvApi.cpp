// UmbraPolicyEnv: a C interface over umbra::policy for the developer-only training scripts
// (tools/policy/*.py, loaded with Python's ctypes). The game never loads this library.
// Every function is safe to call with a null handle (it does nothing / returns -1).

#include <PolicyEnvironment.hpp>
#include <PolicyModel.hpp>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

#if defined(_WIN32)
#define UMBRA_API extern "C" __declspec(dllexport)
#else
#define UMBRA_API extern "C" __attribute__((visibility("default")))
#endif

namespace
{
    struct Env final
    {
        umbra::rpg::Content content;
        std::unique_ptr<umbra::policy::PolicyEnvironment> env;
        std::string text; // last string handed out
    };

    struct Model final
    {
        umbra::policy::PolicyModel model;
    };
}

UMBRA_API const char *umbra_observation_version() { return umbra::policy::observationVersion; }
UMBRA_API int umbra_observation_size() { return static_cast<int>(umbra::policy::observationSize); }

// Loads the RPG content and makes a fight environment; null on any error (printed to stderr).
UMBRA_API void *umbra_env_create(const char *contentPath, const char *learner, const char *opponent, float randomness, int randomStart,
                                 int maxRounds, int opponentLevel)
{
    auto e = std::make_unique<Env>();
    std::vector<umbra::core::Diagnostic> diagnostics;
    if (!umbra::rpg::LoadContent(contentPath, e->content, diagnostics))
    {
        for (const auto &d : diagnostics)
            std::fprintf(stderr, "%s\n", umbra::core::Format(d).c_str());
        return nullptr;
    }
    e->env = std::make_unique<umbra::policy::PolicyEnvironment>(e->content, umbra::policy::EnvironmentSettings{learner, opponent, maxRounds, randomness, randomStart != 0, opponentLevel});
    if (e->env->GetContract().actions.empty())
        return nullptr;
    return e.release();
}

UMBRA_API void umbra_env_destroy(void *handle) { delete static_cast<Env *>(handle); }
UMBRA_API int umbra_env_reset(void *handle, unsigned seed) { return handle && static_cast<Env *>(handle)->env->Reset(seed) ? 1 : 0; }
UMBRA_API int umbra_env_action_count(void *handle) { return handle ? static_cast<int>(static_cast<Env *>(handle)->env->GetContract().actions.size()) : -1; }

UMBRA_API void umbra_env_observe(void *handle, float *out)
{
    if (!handle)
        return;
    const std::vector<float> o = static_cast<Env *>(handle)->env->Observe();
    std::copy(o.begin(), o.end(), out);
}

UMBRA_API void umbra_env_mask(void *handle, unsigned char *out)
{
    if (!handle)
        return;
    const std::vector<std::uint8_t> m = static_cast<Env *>(handle)->env->Mask();
    std::copy(m.begin(), m.end(), out);
}

UMBRA_API int umbra_env_step(void *handle, int action, float *reward, int *done, int *truncated, int *winner)
{
    if (!handle || action < 0)
        return -1;
    const umbra::policy::EnvironmentStep s = static_cast<Env *>(handle)->env->Step(static_cast<std::size_t>(action));
    *reward = s.reward;
    *done = s.done;
    *truncated = s.truncated;
    *winner = s.winner;
    return 0;
}

UMBRA_API int umbra_env_conventional(void *handle) { return handle ? static_cast<Env *>(handle)->env->ConventionalAction() : -1; }
UMBRA_API int umbra_env_illegal(void *handle) { return handle ? static_cast<Env *>(handle)->env->IllegalActions() : -1; }

UMBRA_API int umbra_env_is_attack(void *handle, int slot)
{
    if (!handle)
        return -1;
    const Env &e = *static_cast<Env *>(handle);
    const auto &actions = e.env->GetContract().actions;
    return slot >= 0 && slot < static_cast<int>(actions.size()) && umbra::policy::IsAttack(e.content, actions[static_cast<std::size_t>(slot)]) ? 1 : 0;
}

// "wait", a skill id, or "item:<id>".
UMBRA_API const char *umbra_env_action_name(void *handle, int slot)
{
    if (!handle)
        return "";
    Env &e = *static_cast<Env *>(handle);
    const auto &actions = e.env->GetContract().actions;
    if (slot < 0 || slot >= static_cast<int>(actions.size()))
        return "";
    const auto &a = actions[static_cast<std::size_t>(slot)];
    e.text = !a.skill.empty() ? a.skill : !a.item.empty() ? "item:" + a.item : "wait";
    return e.text.c_str();
}

UMBRA_API const char *umbra_env_fingerprint(void *handle) { return handle ? static_cast<Env *>(handle)->env->GetContract().fingerprint.c_str() : ""; }

// --- The engine's own network code, for parity checks and native evaluation ---

UMBRA_API void *umbra_model_load(const char *modelPath, void *envHandle)
{
    auto m = std::make_unique<Model>();
    std::vector<umbra::core::Diagnostic> diagnostics;
    bool ok = umbra::policy::LoadPolicy(modelPath, m->model, diagnostics);
    if (ok && envHandle)
        ok = umbra::policy::CheckCompatible(m->model, static_cast<Env *>(envHandle)->content, modelPath, diagnostics);
    for (const auto &d : diagnostics)
        std::fprintf(stderr, "%s\n", umbra::core::Format(d).c_str());
    return ok ? m.release() : nullptr;
}

UMBRA_API void umbra_model_destroy(void *handle) { delete static_cast<Model *>(handle); }

UMBRA_API int umbra_model_logits(void *handle, const float *observation, float *out)
{
    if (!handle)
        return -1;
    const std::vector<float> o(observation, observation + umbra::policy::observationSize);
    const std::vector<float> scores = static_cast<Model *>(handle)->model.network.Forward(o);
    std::copy(scores.begin(), scores.end(), out);
    return static_cast<int>(scores.size());
}

UMBRA_API int umbra_model_choose(void *handle, const float *observation, const unsigned char *mask, int actions)
{
    if (!handle)
        return -1;
    const std::vector<float> o(observation, observation + umbra::policy::observationSize);
    const std::vector<std::uint8_t> m(mask, mask + actions);
    return static_cast<Model *>(handle)->model.network.Choose(o, m);
}
