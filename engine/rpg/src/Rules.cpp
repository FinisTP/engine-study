#include <Rules.hpp>
#include <Inventory.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::rpg
{
    namespace
    {
        const std::vector<EffectDef> &EffectsOf(const Content &content, const ActionIntent &intent)
        {
            return intent.skill.empty() ? content.FindItem(intent.item)->use : content.FindSkill(intent.skill)->effects;
        }

        bool TargetsSelf(const Content &content, const ActionIntent &intent)
        {
            return intent.skill.empty() || content.FindSkill(intent.skill)->target == TargetRule::Self;
        }

        // Lowers health; reports damage only (death is reported by the caller after all effects).
        void Hurt(Character &target, int amount, std::size_t actor, std::size_t index, const std::string &what,
                  EventKind kind, Events &events)
        {
            const int dealt = std::min(target.health, amount);
            target.health -= dealt;
            events.push_back({kind, actor, index, dealt, what});
        }

        void ReportDeaths(const Roster &roster, const std::vector<std::size_t> &indices, const std::vector<bool> &wasAlive,
                          std::size_t actor, Events &events)
        {
            for (std::size_t i = 0; i < indices.size(); ++i)
            {
                Character &target = *roster[indices[i]].character;
                if (wasAlive[i] && !target.IsAlive())
                {
                    target.statuses.clear();
                    events.push_back({EventKind::Death, actor, indices[i], 0, target.id});
                }
            }
        }

        // Applies one status tick (damage or heal per tick) to a living combatant.
        void Tick(const Content &content, const Roster &roster, std::size_t index, const ActiveStatus &status, Events &events)
        {
            Character &c = *roster[index].character;
            const int perTick = content.FindStatus(status.status)->damagePerTick;
            if (perTick > 0)
                Hurt(c, perTick, index, index, status.status, EventKind::StatusTick, events);
            else if (perTick < 0)
            {
                const int max = Get(DerivedStats(content, c), Stat::MaxHealth);
                const int healed = std::min(-perTick, max - c.health);
                c.health += healed;
                events.push_back({EventKind::StatusTick, index, index, -healed, status.status});
            }
        }
    }

    Rejection CheckAction(const Content &content, const Roster &roster, const ActionIntent &intent, Mode mode)
    {
        // 1. The actor.
        if (intent.actor >= roster.size())
            return Rejection::UnknownActor;
        const Character &actor = *roster[intent.actor].character;
        if (!actor.IsAlive())
            return Rejection::ActorDead;

        // 2. The skill or item, and its cost.
        if (intent.skill.empty() == intent.item.empty())
            return Rejection::UnknownSkill;
        if (!intent.skill.empty())
        {
            const SkillDef *skill = content.FindSkill(intent.skill);
            if (skill == nullptr)
                return Rejection::UnknownSkill;
            if (!actor.skills.contains(intent.skill))
                return Rejection::SkillLocked;
            if ((mode == Mode::Turn && !skill->turn) || (mode == Mode::Realtime && !skill->realtime))
                return Rejection::WrongMode;
            if (actor.energy < skill->energyCost)
                return Rejection::NotEnoughEnergy;
        }
        else
        {
            const ItemDef *item = content.FindItem(intent.item);
            if (item == nullptr || CountItem(actor, intent.item) == 0)
                return Rejection::NoSuchItem;
            if (item->use.empty())
                return Rejection::ItemNotUsable;
        }

        // 3. The target: turn combat names one enemy; real-time finds targets when the attack lands.
        if (mode == Mode::Turn && !TargetsSelf(content, intent))
        {
            if (intent.target >= roster.size() || roster[intent.target].team == roster[intent.actor].team)
                return Rejection::BadTarget;
            if (!roster[intent.target].character->IsAlive())
                return Rejection::TargetDead;
        }
        return Rejection::None;
    }

    void PayCost(const Content &content, const Roster &roster, const ActionIntent &intent, Events &events)
    {
        Character &actor = *roster[intent.actor].character;
        if (!intent.skill.empty())
        {
            const int cost = content.FindSkill(intent.skill)->energyCost;
            actor.energy -= cost;
            events.push_back({EventKind::CostPaid, intent.actor, intent.actor, cost, intent.skill});
        }
        else
        {
            RemoveItem(actor, intent.item, 1);
            events.push_back({EventKind::ItemUsed, intent.actor, intent.actor, 1, intent.item});
        }
    }

    std::vector<std::size_t> TurnTargets(const Content &content, const ActionIntent &intent)
    {
        return {TargetsSelf(content, intent) ? intent.actor : intent.target};
    }

    void ApplyEffects(const Content &content, const Roster &roster, const ActionIntent &intent,
                      const std::vector<std::size_t> &targets, Mode mode, Events &events)
    {
        const int attack = Get(DerivedStats(content, *roster[intent.actor].character), Stat::Attack);
        const std::string &what = intent.skill.empty() ? intent.item : intent.skill;
        std::vector<bool> wasAlive;

        // 1. Every effect on every target, in the given order.
        for (const std::size_t index : targets)
        {
            Character &target = *roster[index].character;
            wasAlive.push_back(target.IsAlive());
            for (const EffectDef &effect : EffectsOf(content, intent))
            {
                if (!target.IsAlive())
                    break; // no effects on the dead (roadmap T07)
                const StatBlock stats = DerivedStats(content, target);
                switch (effect.kind)
                {
                case EffectKind::Damage:
                    Hurt(target, DamageAmount(effect.power, attack, Get(stats, Stat::Defense)), intent.actor, index, what,
                         EventKind::Damage, events);
                    break;
                case EffectKind::Heal: {
                    const int healed = std::min(effect.power, Get(stats, Stat::MaxHealth) - target.health);
                    target.health += healed;
                    events.push_back({EventKind::Heal, intent.actor, index, healed, what});
                    break;
                }
                case EffectKind::RestoreEnergy: {
                    const int restored = std::min(effect.power, Get(stats, Stat::MaxEnergy) - target.energy);
                    target.energy += restored;
                    events.push_back({EventKind::EnergyRestored, intent.actor, index, restored, what});
                    break;
                }
                case EffectKind::ApplyStatus: {
                    // Re-applying refreshes the duration; the same status never stacks.
                    const StatusDef &def = *content.FindStatus(effect.status);
                    ActiveStatus fresh{effect.status, mode == Mode::Turn ? *def.turns : 0,
                                       mode == Mode::Realtime ? *def.seconds : 0.0F, 1.0F};
                    std::erase_if(target.statuses, [&](const ActiveStatus &s) { return s.status == effect.status; });
                    target.statuses.push_back(fresh);
                    ClampResources(content, target); // a status may lower a maximum
                    events.push_back({EventKind::StatusApplied, intent.actor, index, 0, effect.status});
                    break;
                }
                }
            }
        }

        // 2. Deaths after all effects, in target order (roadmap T10).
        ReportDeaths(roster, targets, wasAlive, intent.actor, events);
    }

    void TickStatusesTurn(const Content &content, const Roster &roster, std::size_t index, Events &events)
    {
        Character &c = *roster[index].character;
        if (!c.IsAlive())
            return;
        // 1. Each status ticks once and loses a turn; 2. expired ones are removed afterwards.
        for (ActiveStatus &status : c.statuses)
        {
            if (c.IsAlive())
                Tick(content, roster, index, status, events);
            --status.turnsLeft;
        }
        for (const ActiveStatus &status : c.statuses)
        {
            if (status.turnsLeft <= 0)
                events.push_back({EventKind::StatusExpired, index, index, 0, status.status});
        }
        std::erase_if(c.statuses, [](const ActiveStatus &s) { return s.turnsLeft <= 0; });
        ClampResources(content, c);
        ReportDeaths(roster, {index}, {true}, index, events);
    }

    void TickStatusesSeconds(const Content &content, const Roster &roster, float seconds, Events &events)
    {
        for (std::size_t index = 0; index < roster.size(); ++index)
        {
            Character &c = *roster[index].character;
            if (!c.IsAlive())
                continue;
            // A status ticks once per full second it has been active, then expires when its time runs out.
            for (ActiveStatus &status : c.statuses)
            {
                status.secondsLeft -= seconds;
                status.tickTimer -= seconds;
                while (status.tickTimer <= 0.0F && c.IsAlive())
                {
                    Tick(content, roster, index, status, events);
                    status.tickTimer += 1.0F;
                }
            }
            for (const ActiveStatus &status : c.statuses)
            {
                if (status.secondsLeft <= 0.0F)
                    events.push_back({EventKind::StatusExpired, index, index, 0, status.status});
            }
            std::erase_if(c.statuses, [](const ActiveStatus &s) { return s.secondsLeft <= 0.0F; });
            ClampResources(content, c);
            ReportDeaths(roster, {index}, {true}, index, events);
        }
    }

    int DamageAmount(int power, int attack, int defense) { return std::max(1, power + attack - defense); }

    int WinningTeam(const Roster &roster)
    {
        int alive = -1;
        for (const Combatant &c : roster)
        {
            if (!c.character->IsAlive())
                continue;
            if (alive != -1 && alive != c.team)
                return -1;
            alive = c.team;
        }
        return alive;
    }

    void EndEncounter(Character &character) { character.statuses.clear(); }

    const char *ToString(Rejection rejection)
    {
        switch (rejection)
        {
        case Rejection::None: return "ok";
        case Rejection::DuplicateIntent: return "this decision was already handled";
        case Rejection::UnknownActor: return "unknown actor";
        case Rejection::ActorDead: return "the actor is dead";
        case Rejection::UnknownSkill: return "unknown skill";
        case Rejection::SkillLocked: return "skill not unlocked";
        case Rejection::WrongMode: return "skill not usable in this combat mode";
        case Rejection::NotEnoughEnergy: return "not enough energy";
        case Rejection::NoSuchItem: return "item not in the inventory";
        case Rejection::ItemNotUsable: return "item cannot be used";
        case Rejection::BadTarget: return "invalid target";
        case Rejection::TargetDead: return "the target is dead";
        case Rejection::NotYourTurn: return "not this actor's turn";
        case Rejection::Busy: return "already acting";
        case Rejection::OnCooldown: return "skill on cooldown";
        case Rejection::Stale: return "the encounter this decision was for has ended";
        case Rejection::EncounterOver: return "the encounter is over";
        }
        return "?";
    }
}
