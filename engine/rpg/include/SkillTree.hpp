#pragma once

#include <Character.hpp>

#include <string>

namespace umbra::rpg
{
    enum class UnlockResult
    {
        Unlocked,
        UnknownSkill,
        AlreadyUnlocked,
        MissingPrerequisite,
        NotEnoughPoints
    };

    // Spends the skill's points and unlocks it. Anything but Unlocked changes nothing.
    UnlockResult Unlock(const Content &content, Character &character, const std::string &skill);
    [[nodiscard]] bool CanUnlock(const Content &content, const Character &character, const std::string &skill);
    [[nodiscard]] const char *ToString(UnlockResult result);
}
