#include "NightInner.hpp"

#include <algorithm>
#include <array>

// The wandering killer (Night.md "The killer"): it wakes at midnight in the under-stage, crosses a few
// rooms after each round's start toward the nearest living guest it senses, and kills one in a room
// it enters. It cannot be fought; ghosts are the house's own and never harmed by it.
namespace umbra::night
{
    using namespace inner;

    int KillerMoves(const NightDef &def, const NightState &state)
    {
        // speed, +1 every speedPerHours hours since it woke; water halves it (at least 1).
        const auto &k = def.rules.killer;
        const int hours = (state.round - state.killer.wokeRound) * def.rules.clock.minutesPerRound / 60;
        const int moves = k.speed + hours / k.speedPerHours;
        return Flooded(state, state.killer.cell) ? std::max(1, moves / 2) : moves;
    }

    int MonsterChance(const NightDef &def, const NightState &state)
    {
        const int hours = (state.round - 1) * def.rules.clock.minutesPerRound / 60;
        return std::min(100, def.rules.monsters.chancePercent + hours * def.rules.monsters.chancePerHour);
    }

    int MonsterMax(const NightDef &def, const NightState &state)
    {
        const int hours = (state.round - 1) * def.rules.clock.minutesPerRound / 60;
        return def.rules.monsters.max + hours / def.rules.monsters.maxPerHours;
    }

    namespace
    {
        // Kills the weakest living guest in the room (lowest HP; ties by index). True if one died.
        bool Strike(const NightDef &def, NightState &state, int cell)
        {
            int victim = -1;
            for (int g = 0; g < state.count; ++g)
                if (Living(state, g) && state.guests[U(g)].cell == cell && (victim < 0 || state.guests[U(g)].hp < state.guests[U(victim)].hp))
                    victim = g;
            if (victim < 0)
                return false;
            Hurt(def, state, victim, 9999, -1);
            return true;
        }

        // The next room toward the nearest living guest within senseSteps, else a random passage.
        int NextStep(const NightDef &def, NightState &state)
        {
            const int at = state.killer.cell;
            int target = -1, best = def.rules.killer.senseSteps + 1;
            for (int g = 0; g < state.count; ++g)
                if (const int n = Living(state, g) ? Steps(def, state, at, state.guests[U(g)].cell) : -1; n >= 0 && n < best)
                    target = state.guests[U(g)].cell, best = n;
            std::array<int, 4> ways{};
            int count = 0;
            for (int d = 0; d < 4; ++d)
                if (Passage(def, state, at, d))
                {
                    const int next = Neighbour(def, at, d);
                    if (target >= 0 && Steps(def, state, next, target) == best - 1)
                        return next;
                    ways[U(count++)] = next;
                }
            return count == 0 ? -1 : ways[U(Below(state, count))];
        }
    }

    void inner::KillerRound(const NightDef &def, NightState &state)
    {
        // 1. Asleep: it wakes at wakeRound in the under-stage (and does not move that round).
        if (state.killer.cell < 0)
        {
            if (state.round == def.rules.killer.wakeRound && state.understage >= 0)
            {
                state.killer = {state.understage, I8(state.round)};
                Log(state, LogKind::Killer, -1, -1, state.understage, 0);
            }
            return;
        }
        // 2. Awake: room by room; it stops in a room where it kills.
        for (int n = KillerMoves(def, state); n > 0 && !state.over; --n)
        {
            const int next = NextStep(def, state);
            if (next < 0)
                return;
            state.killer.cell = I8(next);
            Log(state, LogKind::Killer, -1, -1, next, 1);
            if (Strike(def, state, next))
                return;
        }
    }

    bool inner::IntoKiller(const NightDef &def, NightState &state, int g)
    {
        // A living guest who walks into the killer's room dies there.
        if (!Living(state, g) || state.killer.cell < 0 || state.guests[U(g)].cell != state.killer.cell)
            return false;
        Hurt(def, state, g, 9999, -1);
        return true;
    }
}
