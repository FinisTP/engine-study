#include <RealtimeEncounter.hpp>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace umbra::realtime
{
    using rpg::Rejection;

    namespace
    {
        glm::vec2 Forward(float yaw) { return {-std::sin(yaw), -std::cos(yaw)}; }
    }

    bool InReach(glm::vec2 from, float yaw, glm::vec2 target, const rpg::RealtimeTiming &timing)
    {
        const glm::vec2 offset = target - from;
        const float distance = glm::length(offset);
        if (distance > timing.range)
            return false;
        if (distance < 1e-4F || timing.arcDegrees >= 360.0F)
            return true;
        // Inside the cone when the angle to the target is at most half the arc.
        return glm::dot(offset / distance, Forward(yaw)) >= std::cos(glm::radians(timing.arcDegrees * 0.5F));
    }

    RealtimeEncounter::RealtimeEncounter(const rpg::Content &content, rpg::Roster roster)
        : content_(content), roster_(std::move(roster)), fighters_(roster_.size())
    {
        winner_ = rpg::WinningTeam(roster_);
    }

    void RealtimeEncounter::SetPlacement(std::size_t index, glm::vec2 position, float yaw)
    {
        fighters_[index].position = position;
        fighters_[index].yaw = yaw;
    }

    float RealtimeEncounter::CooldownLeft(std::size_t index, const std::string &skill) const
    {
        const auto it = fighters_[index].cooldowns.find(skill);
        return it == fighters_[index].cooldowns.end() ? 0.0F : it->second;
    }

    const rpg::RealtimeTiming &RealtimeEncounter::TimingOf(const rpg::ActionIntent &intent) const
    {
        return intent.skill.empty() ? itemTiming : content_.FindSkill(intent.skill)->timing;
    }

    Rejection RealtimeEncounter::StartAction(const rpg::ActionIntent &intent)
    {
        if (IsOver())
            return Rejection::EncounterOver;
        if (intent.actor >= fighters_.size())
            return Rejection::UnknownActor;
        if (fighters_[intent.actor].phase != Phase::Idle)
            return Rejection::Busy;
        if (!intent.skill.empty() && CooldownLeft(intent.actor, intent.skill) > 0.0F)
            return Rejection::OnCooldown;
        const Rejection check = rpg::CheckAction(content_, roster_, intent, rpg::Mode::Realtime);
        if (check != Rejection::None)
            return check;
        if (!log_.First(intent.id))
            return Rejection::DuplicateIntent;

        Fighter &f = fighters_[intent.actor];
        f.phase = Phase::Windup;
        f.phaseLeft = TimingOf(intent).windup;
        f.action = intent;
        f.hit.clear();
        return Rejection::None;
    }

    void RealtimeEncounter::Update(float seconds)
    {
        if (IsOver() || seconds <= 0.0F)
            return;
        // 1. Clocks: statuses tick per second; cooldowns run down.
        rpg::TickStatusesSeconds(content_, roster_, seconds, events_);
        for (Fighter &f : fighters_)
        {
            for (auto &[skill, left] : f.cooldowns)
                left = std::max(0.0F, left - seconds);
        }
        // 2. Every fighter's action, in roster order (a fixed order for simultaneous hits).
        for (std::size_t i = 0; i < fighters_.size() && !IsOver(); ++i)
        {
            if (!roster_[i].character->IsAlive())
                Cancel(i); // the dead stop acting; a committed cost is not refunded
            else
                Advance(i, seconds);
            winner_ = rpg::WinningTeam(roster_);
        }
    }

    void RealtimeEncounter::Advance(std::size_t index, float seconds)
    {
        Fighter &f = fighters_[index];
        // Time left over after a phase ends carries into the next one, so results don't depend
        // on the frame rate. The active phase always gets at least one hit check.
        while (f.phase != Phase::Idle)
        {
            if (f.phase == Phase::Active)
                Hit(index);
            if (f.phase == Phase::Idle || f.phaseLeft > seconds)
            {
                f.phaseLeft -= seconds;
                return;
            }
            seconds -= f.phaseLeft;
            const rpg::RealtimeTiming &timing = TimingOf(f.action);
            switch (f.phase)
            {
            case Phase::Windup:
                // Commit (user decision 2026-11-07): the cost is paid now, and the cooldown starts.
                if (rpg::CheckAction(content_, roster_, f.action, rpg::Mode::Realtime) != Rejection::None)
                {
                    Cancel(index); // e.g. the energy was spent meanwhile: nothing paid
                    return;
                }
                rpg::PayCost(content_, roster_, f.action, events_);
                if (!f.action.skill.empty())
                    f.cooldowns[f.action.skill] = timing.cooldown;
                f.phase = Phase::Active;
                f.phaseLeft = timing.active;
                break;
            case Phase::Active:
                f.phase = Phase::Recovery;
                f.phaseLeft = timing.recovery;
                break;
            default:
                f.phase = Phase::Idle;
                break;
            }
        }
    }

    void RealtimeEncounter::Hit(std::size_t index)
    {
        Fighter &f = fighters_[index];
        const bool self = f.action.skill.empty() || content_.FindSkill(f.action.skill)->target == rpg::TargetRule::Self;

        // 1. Who is hit now: yourself once, or living enemies in reach not hit before by this swing.
        std::vector<std::size_t> targets;
        if (self && !f.hit.contains(index))
            targets.push_back(index);
        for (std::size_t t = 0; t < roster_.size() && !self; ++t)
        {
            if (roster_[t].team != roster_[index].team && roster_[t].character->IsAlive() && !f.hit.contains(t) &&
                InReach(f.position, f.yaw, fighters_[t].position, TimingOf(f.action)))
                targets.push_back(t);
        }
        if (targets.empty())
            return;
        f.hit.insert(targets.begin(), targets.end());

        // 2. Shared effects; damage during someone's wind-up interrupts it (free: nothing paid yet).
        const std::size_t first = events_.size();
        rpg::ApplyEffects(content_, roster_, f.action, targets, rpg::Mode::Realtime, events_);
        for (std::size_t e = first; e < events_.size(); ++e)
        {
            const rpg::Event event = events_[e]; // copy: Cancel may add events
            if (event.kind == rpg::EventKind::Damage && event.amount > 0 && fighters_[event.target].phase == Phase::Windup)
            {
                events_.push_back({rpg::EventKind::Interrupted, event.target, event.target, 0, fighters_[event.target].action.skill});
                Cancel(event.target);
            }
        }
    }

    void RealtimeEncounter::Cancel(std::size_t index)
    {
        fighters_[index].phase = Phase::Idle;
        fighters_[index].phaseLeft = 0.0F;
        fighters_[index].hit.clear();
    }

    Decision ChooseRealtimeAction(const rpg::Content &content, const RealtimeEncounter &encounter, std::size_t actor)
    {
        Decision decision;
        const rpg::Roster &roster = encounter.GetRoster();
        const rpg::Character &self = *roster[actor].character;
        decision.yaw = encounter.GetYaw(actor);
        if (!self.IsAlive() || encounter.IsOver())
            return decision;

        // 1. The nearest living enemy: face it.
        const glm::vec2 from = encounter.GetPosition(actor);
        std::optional<std::size_t> enemy;
        float nearest = std::numeric_limits<float>::max();
        for (std::size_t t = 0; t < roster.size(); ++t)
        {
            const float distance = glm::length(encounter.GetPosition(t) - from);
            if (roster[t].team != roster[actor].team && roster[t].character->IsAlive() && distance < nearest)
            {
                enemy = t;
                nearest = distance;
            }
        }
        if (!enemy)
            return decision;
        const glm::vec2 offset = encounter.GetPosition(*enemy) - from;
        if (glm::length(offset) > 1e-4F)
            decision.yaw = std::atan2(-offset.x, -offset.y);

        // 2. Low health: drink a healing item.
        const bool idle = encounter.GetPhase(actor) == Phase::Idle;
        const rpg::StatBlock stats = rpg::DerivedStats(content, self);
        if (idle && self.health * 100 < rpg::Get(stats, rpg::Stat::MaxHealth) * 35)
        {
            for (const rpg::ItemStack &stack : self.inventory)
            {
                const rpg::ItemDef *item = content.FindItem(stack.item);
                if (!item->use.empty() && item->use.front().kind == rpg::EffectKind::Heal)
                {
                    decision.act = rpg::ActionIntent{0, actor, "", stack.item, actor};
                    return decision;
                }
            }
        }

        // 3. The strongest ready, affordable enemy skill that reaches; otherwise walk closer.
        const rpg::SkillDef *best = nullptr;
        float longest = 0.0F;
        for (const std::string &id : self.skills)
        {
            const rpg::SkillDef *skill = content.FindSkill(id);
            if (!skill->realtime || skill->target != rpg::TargetRule::Enemy || self.energy < skill->energyCost)
                continue;
            longest = std::max(longest, skill->timing.range);
            const bool ready = encounter.CooldownLeft(actor, id) <= 0.0F && nearest <= skill->timing.range * 0.9F;
            const auto power = [](const rpg::SkillDef *s) {
                int total = 0;
                for (const rpg::EffectDef &e : s->effects)
                    total += e.kind == rpg::EffectKind::Damage ? e.power : 0;
                return total;
            };
            if (ready && (best == nullptr || power(skill) > power(best)))
                best = skill;
        }
        if (idle && best != nullptr)
            decision.act = rpg::ActionIntent{0, actor, best->id, "", *enemy};
        if (nearest > longest * 0.8F && idle)
            decision.move = glm::normalize(offset);
        return decision;
    }
}
