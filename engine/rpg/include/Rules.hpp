#pragma once

#include <Character.hpp>

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace umbra::rpg
{
    enum class Mode
    {
        Turn,
        Realtime
    };

    // One fighter in an encounter. Characters are owned outside (the party, the area), so
    // health, items and unlocked skills carry over from one encounter to the next.
    struct Combatant final
    {
        Character *character = nullptr;
        int team = 0;
    };
    using Roster = std::vector<Combatant>;

    // What the player, a conventional enemy or a learned policy wants to do.
    // Exactly one of `skill` / `item` is set.
    struct ActionIntent final
    {
        std::uint64_t id = 0;   // unique per decision; a repeated id is ignored (no double effects)
        std::size_t actor = 0;  // index in the roster
        std::string skill;
        std::string item;       // use one of this item on yourself
        std::size_t target = 0; // roster index, for enemy skills in turn combat
    };

    enum class Rejection
    {
        None,
        DuplicateIntent,
        UnknownActor,
        ActorDead,
        UnknownSkill,
        SkillLocked,
        WrongMode,
        NotEnoughEnergy,
        NoSuchItem,
        ItemNotUsable,
        BadTarget,
        TargetDead,
        NotYourTurn, // turn controller
        Busy,        // real-time controller: already attacking
        OnCooldown,  // real-time controller
        Stale,       // the encounter this intent was made for has ended
        EncounterOver
    };
    [[nodiscard]] const char *ToString(Rejection rejection);

    enum class EventKind
    {
        CostPaid,
        ItemUsed,
        Damage,
        Heal,
        EnergyRestored,
        StatusApplied,
        StatusTick,
        StatusExpired,
        Death,
        Interrupted, // real-time: wind-up cancelled
        TurnStarted
    };

    // Something that happened, for UI, audio, quests and tests. Never an input to the rules.
    struct Event final
    {
        EventKind kind = EventKind::Damage;
        std::size_t actor = 0;
        std::size_t target = 0;
        int amount = 0;
        std::string what; // skill, item or status id
    };
    using Events = std::vector<Event>;

    // Remembers decision ids so a repeated delivery is rejected instead of applied twice.
    class IntentLog final
    {
    public:
        // True the first time an id is seen.
        bool First(std::uint64_t id) { return seen_.insert(id).second; }
        void Clear() { seen_.clear(); }

    private:
        std::set<std::uint64_t> seen_;
    };

    // --- The shared rules (formulas in Rpg.md). Both combat controllers call these. ---

    // Is this action legal right now? Never changes anything.
    [[nodiscard]] Rejection CheckAction(const Content &content, const Roster &roster, const ActionIntent &intent, Mode mode);

    // Pays the energy (skill) or consumes the item. Call only after CheckAction passed.
    void PayCost(const Content &content, const Roster &roster, const ActionIntent &intent, Events &events);

    // The effects of the intent's skill or item, applied to `targets` in the given order;
    // dead targets are skipped. Deaths are reported after all effects, in target order.
    void ApplyEffects(const Content &content, const Roster &roster, const ActionIntent &intent,
                      const std::vector<std::size_t> &targets, Mode mode, Events &events);

    // Who the intent affects in turn combat: the chosen target, or the actor for self skills/items.
    [[nodiscard]] std::vector<std::size_t> TurnTargets(const Content &content, const ActionIntent &intent);

    // Status clocks: one turn passed for this combatant / some seconds passed for everyone.
    void TickStatusesTurn(const Content &content, const Roster &roster, std::size_t index, Events &events);
    void TickStatusesSeconds(const Content &content, const Roster &roster, float seconds, Events &events);

    // Damage before clamping: max(1, power + attack - defense).
    [[nodiscard]] int DamageAmount(int power, int attack, int defense);

    // The team still standing, or -1 while two or more teams have someone alive.
    [[nodiscard]] int WinningTeam(const Roster &roster);

    // Statuses end with the encounter (user decision 2026-11-07).
    void EndEncounter(Character &character);
}
