#pragma once

#include <Character.hpp>

namespace umbra::rpg
{
    // Inventory and equipment transactions. Each one either fully succeeds or changes nothing.

    // Tops up existing stacks, then fills free slots. If everything doesn't fit, nothing is
    // added (user decision: a full inventory rejects the pickup and the item stays where it was).
    bool AddItem(const Content &content, Character &character, const std::string &item, int count);
}
