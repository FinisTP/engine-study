#include <TurnEncounter.hpp>

#include <Inventory.hpp>

#include <algorithm>
#include <utility>

namespace umbra::turn
{
    using rpg::Rejection;

    TurnEncounter::TurnEncounter(const rpg::Content &content, rpg::Roster roster)
        : content_(content), roster_(std::move(roster))
    {
        winner_ = rpg::WinningTeam(roster_);
        if (!IsOver())
            StartRound();
    }

    std::optional<std::size_t> TurnEncounter::Current() const
    {
        if (IsOver() || position_ >= order_.size())
            return std::nullopt;
        return order_[position_];
    }

    Rejection TurnEncounter::Submit(const rpg::ActionIntent &intent)
    {
        // 1. Is it this fighter's turn, and is this a new decision?
        if (IsOver())
            return Rejection::EncounterOver;
        if (intent.actor != Current())
            return Rejection::NotYourTurn;
        const bool wait = intent.skill.empty() && intent.item.empty();
        const Rejection check = wait ? Rejection::None : rpg::CheckAction(content_, roster_, intent, rpg::Mode::Turn);
        if (check != Rejection::None)
            return check;
        if (!log_.First(intent.id))
            return Rejection::DuplicateIntent;

        // 2. Commit: pay, apply, then the turn is used up.
        if (!wait)
        {
            rpg::PayCost(content_, roster_, intent, events_);
            rpg::ApplyEffects(content_, roster_, intent, rpg::TurnTargets(content_, intent), rpg::Mode::Turn, events_);
        }
        winner_ = rpg::WinningTeam(roster_);
        NextTurn();
        return Rejection::None;
    }

    void TurnEncounter::StartRound()
    {
        // Fastest first; equal speed keeps roster order (stable sort), so the order is fixed.
        ++round_;
        order_.clear();
        for (std::size_t i = 0; i < roster_.size(); ++i)
        {
            if (roster_[i].character->IsAlive())
                order_.push_back(i);
        }
        std::stable_sort(order_.begin(), order_.end(), [&](std::size_t a, std::size_t b) {
            return rpg::Get(rpg::DerivedStats(content_, *roster_[a].character), rpg::Stat::Speed) >
                   rpg::Get(rpg::DerivedStats(content_, *roster_[b].character), rpg::Stat::Speed);
        });
        // The first fighter's turn starts now (statuses tick); NextTurn skips anyone who can't act.
        position_ = static_cast<std::size_t>(-1);
        NextTurn();
    }

    void TurnEncounter::NextTurn()
    {
        // Move to the next fighter who is alive when their turn starts. Statuses tick at the
        // start of a turn and can kill; then that fighter is skipped.
        while (!IsOver())
        {
            ++position_;
            if (position_ >= order_.size())
            {
                StartRound();
                return;
            }
            const std::size_t actor = order_[position_];
            if (!roster_[actor].character->IsAlive())
                continue;
            events_.push_back({rpg::EventKind::TurnStarted, actor, actor, round_, ""});
            rpg::TickStatusesTurn(content_, roster_, actor, events_);
            winner_ = rpg::WinningTeam(roster_);
            if (roster_[actor].character->IsAlive())
                return;
        }
    }

    std::vector<rpg::ActionIntent> TurnEncounter::LegalActions() const
    {
        std::vector<rpg::ActionIntent> actions;
        const std::optional<std::size_t> actor = Current();
        if (!actor)
            return actions;
        const rpg::Character &self = *roster_[*actor].character;
        const auto add = [&](rpg::ActionIntent intent) {
            if (rpg::CheckAction(content_, roster_, intent, rpg::Mode::Turn) == Rejection::None)
                actions.push_back(intent);
        };
        // Skills (sorted by id) x living enemies (roster order), then usable items, then wait.
        for (const std::string &skill : self.skills)
        {
            if (content_.FindSkill(skill)->target == rpg::TargetRule::Self)
                add({0, *actor, skill, "", *actor});
            else
            {
                for (std::size_t target = 0; target < roster_.size(); ++target)
                    add({0, *actor, skill, "", target});
            }
        }
        std::vector<std::string> seen;
        for (const rpg::ItemStack &stack : self.inventory)
        {
            if (std::find(seen.begin(), seen.end(), stack.item) == seen.end())
            {
                seen.push_back(stack.item);
                add({0, *actor, "", stack.item, *actor});
            }
        }
        actions.push_back({0, *actor, "", "", *actor}); // wait
        return actions;
    }

    rpg::ActionIntent ChooseTurnAction(const rpg::Content &content, const TurnEncounter &encounter)
    {
        const std::vector<rpg::ActionIntent> legal = encounter.LegalActions();
        if (legal.empty())
            return {};
        const rpg::Roster &roster = encounter.GetRoster();
        const rpg::Character &self = *roster[legal.front().actor].character;
        const rpg::StatBlock stats = rpg::DerivedStats(content, self);

        // 1. Low on health: drink something that heals.
        if (self.health * 100 < rpg::Get(stats, rpg::Stat::MaxHealth) * 35)
        {
            for (const rpg::ActionIntent &action : legal)
            {
                if (!action.item.empty() && content.FindItem(action.item)->use.front().kind == rpg::EffectKind::Heal)
                    return action;
            }
        }

        // 2. The attack with the most damage on the weakest enemy (first found wins ties).
        const auto damage = [&](const rpg::ActionIntent &action) {
            if (action.skill.empty() || content.FindSkill(action.skill)->target != rpg::TargetRule::Enemy)
                return 0;
            const int defense = rpg::Get(rpg::DerivedStats(content, *roster[action.target].character), rpg::Stat::Defense);
            int total = 0;
            for (const rpg::EffectDef &effect : content.FindSkill(action.skill)->effects)
            {
                if (effect.kind == rpg::EffectKind::Damage)
                    total += rpg::DamageAmount(effect.power, rpg::Get(stats, rpg::Stat::Attack), defense);
            }
            return total;
        };
        const rpg::ActionIntent *best = nullptr;
        for (const rpg::ActionIntent &action : legal)
        {
            if (damage(action) == 0)
                continue;
            if (best == nullptr)
            {
                best = &action;
                continue;
            }
            const int health = roster[action.target].character->health;
            const int bestHealth = roster[best->target].character->health;
            if (health < bestHealth || (health == bestHealth && damage(action) > damage(*best)))
                best = &action;
        }
        return best != nullptr ? *best : legal.back(); // nothing to attack with: wait
    }
}
