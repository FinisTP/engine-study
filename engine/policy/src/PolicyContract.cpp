#include <PolicyContract.hpp>

#include <Character.hpp>

#include <algorithm>
#include <cstdio>
#include <set>

namespace umbra::policy
{
    namespace
    {
        // --- Fingerprint: every number that changes what the actions do, as one canonical text ---

        void AddEffects(std::string &text, std::set<std::string> &statuses, const std::vector<rpg::EffectDef> &effects)
        {
            for (const rpg::EffectDef &e : effects)
            {
                text += "e" + std::to_string(static_cast<int>(e.kind)) + ":" + std::to_string(e.power) + ":" + e.status + ";";
                if (!e.status.empty())
                    statuses.insert(e.status);
            }
        }

        void AddModifiers(std::string &text, const std::vector<rpg::Modifier> &modifiers)
        {
            for (const rpg::Modifier &m : modifiers)
                text += "m" + std::to_string(static_cast<int>(m.stat)) + ":" + std::to_string(m.add) + ":" + std::to_string(m.percent) + ";";
        }

        void AddActor(std::string &text, std::set<std::string> &items, const rpg::ActorDef &actor)
        {
            text += "actor " + actor.id + ":";
            for (int stat : actor.base)
                text += std::to_string(stat) + ",";
            for (const std::string &skill : actor.skills)
                text += "s" + skill + ",";
            for (const std::string &item : actor.equipment)
                text += "q" + item + ",", items.insert(item);
            for (const rpg::ItemCount &stack : actor.inventory)
                text += "i" + stack.item + "x" + std::to_string(stack.count) + ",", items.insert(stack.item);
            text += "|";
        }

        // FNV-1a 64-bit, printed as 16 hex digits.
        std::string Hash(const std::string &text)
        {
            std::uint64_t hash = 14695981039346656037ULL;
            for (unsigned char c : text)
                hash = (hash ^ c) * 1099511628211ULL;
            char out[17];
            std::snprintf(out, sizeof(out), "%016llx", static_cast<unsigned long long>(hash));
            return out;
        }

        int StatusTurns(const rpg::Character &c, const char *status)
        {
            for (const rpg::ActiveStatus &s : c.statuses)
                if (s.status == status)
                    return s.turnsLeft;
            return 0;
        }

        float Fraction(int value, int max) { return max > 0 ? std::clamp(static_cast<float>(value) / static_cast<float>(max), 0.0F, 1.0F) : 0.0F; }
    }

    Contract MakeContract(const rpg::Content &content, const std::string &learner, const std::string &opponent)
    {
        Contract contract{learner, opponent, {}, {}};
        const rpg::ActorDef *self = content.FindActor(learner);
        const rpg::ActorDef *other = content.FindActor(opponent);
        if (self == nullptr || other == nullptr)
            return contract;

        // 1. Catalogue: wait, the learner's turn skills, its usable starting items.
        contract.actions.push_back({});
        for (const std::string &id : self->skills)
            if (const rpg::SkillDef *skill = content.FindSkill(id); skill && skill->turn)
                contract.actions.push_back({id, ""});
        for (const rpg::ItemCount &stack : self->inventory)
        {
            const rpg::ItemDef *item = content.FindItem(stack.item);
            const ActionSlot slot{"", stack.item};
            if (item && !item->use.empty() && std::find(contract.actions.begin(), contract.actions.end(), slot) == contract.actions.end())
                contract.actions.push_back(slot);
        }

        // 2. Fingerprint over both actors, every skill/item they can use or wear, and the statuses
        //    those apply. Names and looks are left out: renaming a skill changes nothing it does.
        std::string text = std::string{observationVersion} + "|";
        std::set<std::string> items, statuses, skills(self->skills.begin(), self->skills.end());
        skills.insert(other->skills.begin(), other->skills.end());
        AddActor(text, items, *self);
        AddActor(text, items, *other);
        for (const std::string &id : skills)
            if (const rpg::SkillDef *s = content.FindSkill(id))
            {
                text += "skill " + id + ":" + std::to_string(s->energyCost) + ":" + std::to_string(static_cast<int>(s->target)) + ":" +
                        std::to_string(s->turn) + ":";
                AddEffects(text, statuses, s->effects);
            }
        for (const std::string &id : items)
            if (const rpg::ItemDef *i = content.FindItem(id))
            {
                text += "item " + id + ":" + std::to_string(static_cast<int>(i->slot)) + ":";
                AddEffects(text, statuses, i->use);
                AddModifiers(text, i->modifiers);
            }
        for (const std::string &id : statuses)
            if (const rpg::StatusDef *s = content.FindStatus(id))
            {
                text += "status " + id + ":" + std::to_string(s->turns.value_or(0)) + ":" + std::to_string(s->damagePerTick) + ":";
                AddModifiers(text, s->modifiers);
            }
        contract.fingerprint = Hash(text);
        return contract;
    }

    std::vector<float> Observe(const rpg::Content &content, const turn::TurnEncounter &encounter, std::size_t self,
                               std::size_t opponent, int maxRounds)
    {
        // Only what the enemy can see: both health/energy bars, visible statuses, its own bag.
        std::vector<float> o;
        o.reserve(observationSize);
        for (const std::size_t index : {self, opponent})
        {
            const rpg::Character &c = *encounter.GetRoster()[index].character;
            const rpg::StatBlock stats = rpg::DerivedStats(content, c);
            o.push_back(Fraction(c.health, rpg::Get(stats, rpg::Stat::MaxHealth)));
            o.push_back(Fraction(c.energy, rpg::Get(stats, rpg::Stat::MaxEnergy)));
            o.push_back(Fraction(StatusTurns(c, "guarded"), 2));
            o.push_back(Fraction(StatusTurns(c, "weakened"), 3));
            o.push_back(Fraction(StatusTurns(c, "poison"), 3));
            if (index == self)
            {
                int heals = 0;
                for (const rpg::ItemStack &stack : c.inventory)
                    if (const rpg::ItemDef *item = content.FindItem(stack.item); item && !item->use.empty() && item->use.front().kind == rpg::EffectKind::Heal)
                        heals += stack.count;
                o.push_back(Fraction(heals, 3));
            }
        }
        o.push_back(Fraction(encounter.Round(), maxRounds));
        return o;
    }

    std::vector<std::uint8_t> Mask(const turn::TurnEncounter &encounter, const Contract &contract, std::size_t self, std::size_t opponent)
    {
        // A slot is legal if the encounter lists that skill on the opponent (or self), or that item.
        std::vector<std::uint8_t> mask(contract.actions.size(), 0);
        if (encounter.Current() != self)
            return mask;
        mask[0] = 1; // waiting is always legal
        for (const rpg::ActionIntent &intent : encounter.LegalActions())
            for (std::size_t i = 1; i < contract.actions.size(); ++i)
            {
                const ActionSlot &slot = contract.actions[i];
                const bool sameSkill = !slot.skill.empty() && slot.skill == intent.skill && (intent.target == opponent || intent.target == self);
                if (sameSkill || (!slot.item.empty() && slot.item == intent.item))
                    mask[i] = 1;
            }
        return mask;
    }

    rpg::ActionIntent ToIntent(const rpg::Content &content, const Contract &contract, std::size_t slot, std::size_t self, std::size_t opponent)
    {
        rpg::ActionIntent intent{0, self, "", "", self};
        if (slot == 0 || slot >= contract.actions.size())
            return intent; // wait
        intent.skill = contract.actions[slot].skill;
        intent.item = contract.actions[slot].item;
        if (const rpg::SkillDef *skill = content.FindSkill(intent.skill); skill && skill->target == rpg::TargetRule::Enemy)
            intent.target = opponent;
        return intent;
    }

    int ToSlot(const Contract &contract, const rpg::ActionIntent &intent)
    {
        for (std::size_t i = 0; i < contract.actions.size(); ++i)
            if (contract.actions[i].skill == intent.skill && contract.actions[i].item == intent.item)
                return static_cast<int>(i);
        return -1;
    }

    bool IsAttack(const rpg::Content &content, const ActionSlot &slot)
    {
        const rpg::SkillDef *skill = content.FindSkill(slot.skill);
        if (skill == nullptr || skill->target != rpg::TargetRule::Enemy)
            return false;
        return std::any_of(skill->effects.begin(), skill->effects.end(), [](const rpg::EffectDef &e) { return e.kind == rpg::EffectKind::Damage; });
    }
}
