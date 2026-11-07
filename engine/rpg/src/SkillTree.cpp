#include <SkillTree.hpp>

namespace umbra::rpg
{
    namespace
    {
        UnlockResult Check(const Content &content, const Character &character, const std::string &skill)
        {
            const SkillDef *def = content.FindSkill(skill);
            if (def == nullptr)
                return UnlockResult::UnknownSkill;
            if (character.skills.contains(skill))
                return UnlockResult::AlreadyUnlocked;
            for (const std::string &required : def->prerequisites)
            {
                if (!character.skills.contains(required))
                    return UnlockResult::MissingPrerequisite;
            }
            if (character.skillPoints < def->pointCost)
                return UnlockResult::NotEnoughPoints;
            return UnlockResult::Unlocked;
        }
    }

    UnlockResult Unlock(const Content &content, Character &character, const std::string &skill)
    {
        const UnlockResult result = Check(content, character, skill);
        if (result != UnlockResult::Unlocked)
            return result;
        // Both changes together, only after every check passed: points are spent exactly once.
        character.skillPoints -= content.FindSkill(skill)->pointCost;
        character.skills.insert(skill);
        return result;
    }

    bool CanUnlock(const Content &content, const Character &character, const std::string &skill)
    {
        return Check(content, character, skill) == UnlockResult::Unlocked;
    }

    const char *ToString(UnlockResult result)
    {
        switch (result)
        {
        case UnlockResult::Unlocked: return "unlocked";
        case UnlockResult::UnknownSkill: return "unknown skill";
        case UnlockResult::AlreadyUnlocked: return "already unlocked";
        case UnlockResult::MissingPrerequisite: return "a required skill is not unlocked yet";
        case UnlockResult::NotEnoughPoints: return "not enough skill points";
        }
        return "?";
    }
}
