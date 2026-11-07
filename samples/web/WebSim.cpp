// The turn-combat rules built for WebAssembly (W0 experiment). The page (or Node) passes the
// content JSON in, then resets and steps a scenario. Everything returns plain numbers or a JSON
// string, so the JavaScript side needs no knowledge of C++ objects. See README.md.
#include <Inventory.hpp>
#include <TurnEnvironment.hpp>

#include <cstdlib>
#include <memory>
#include <string>

#define EXPORT(name) extern "C" __attribute__((export_name(name)))

namespace
{
    using namespace umbra;

    rpg::Content content;
    std::unique_ptr<turn::TurnEnvironment> environment;
    std::string text; // last string handed to JavaScript

    void Append(std::string &out, const char *key, int value, bool comma = true)
    {
        out += std::string("\"") + key + "\":" + std::to_string(value) + (comma ? "," : "");
    }
}

// Memory the page writes the content text into.
EXPORT("umbra_alloc") char *Alloc(int size) { return static_cast<char *>(std::malloc(static_cast<std::size_t>(size))); }
EXPORT("umbra_free") void Free(char *p) { std::free(p); }

// 1. Load content (umbra.rpg JSON). Returns the number of diagnostics; -1 if it failed.
EXPORT("umbra_load") int Load(const char *json, int length)
{
    std::vector<core::Diagnostic> diagnostics;
    if (!rpg::ParseContent(std::string(json, static_cast<std::size_t>(length)), "web", content, diagnostics))
        return -1;
    return static_cast<int>(diagnostics.size());
}

// 2. The reference fight: hero against a wolf and a bandit. Returns 1 on success.
EXPORT("umbra_reset") int Reset()
{
    environment = std::make_unique<turn::TurnEnvironment>(
        content, std::vector<turn::ScenarioSlot>{{"hero", "hero", 0}, {"wolf", "wolf_1", 1}, {"bandit", "bandit_1", 1}});
    return environment->Reset() ? 1 : 0;
}

// 3. One decision by the conventional AI for whoever's turn it is. Returns 1 while the fight goes on.
EXPORT("umbra_step_ai") int StepAi()
{
    if (!environment || !environment->Current())
        return 0;
    const turn::StepResult result = environment->Step(turn::ChooseTurnAction(content, *environment->Encounter()));
    return result.rejection == rpg::Rejection::None && !result.done ? 1 : 0;
}

// 4. The state as JSON: {"round":..,"steps":..,"winner":..,"fighters":[{"id":..,"health":..,"energy":..}]}
EXPORT("umbra_state") const char *State()
{
    text = "{";
    if (environment)
    {
        const turn::TurnEncounter *encounter = environment->Encounter();
        Append(text, "round", environment->Round());
        Append(text, "steps", environment->Steps());
        Append(text, "winner", encounter->Winner());
        text += "\"fighters\":[";
        for (const rpg::Character &c : environment->Characters())
        {
            text += "{\"id\":\"" + c.id + "\",";
            Append(text, "health", c.health);
            Append(text, "energy", c.energy);
            Append(text, "potions", rpg::CountItem(c, "health_potion"), false);
            text += "},";
        }
        if (text.back() == ',')
            text.pop_back();
        text += "]";
    }
    text += "}";
    return text.c_str();
}
