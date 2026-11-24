#pragma once

#include <MatchDef.hpp>

#include <algorithm>
#include <string>

// An item's picture: art/ui/campaign/item/<sprite>.png, the sprite from items.csv. Many items share
// one picture (weapon, armour, accessory, item) so new ones need no new art (the user, 2026-11-25).
inline std::string ItemPicture(const umbra::match::MatchDef &def, const std::string &id)
{
    const auto it = std::ranges::find(def.items, id, &umbra::match::ItemDef::id);
    return "art/ui/campaign/item/" + (it == def.items.end() || it->sprite.empty() ? id : it->sprite) + ".png";
}
