#include "NotebookView.hpp"

#include <algorithm>

namespace campaign = umbra::campaign;

Notebook::Notebook(const umbra::match::MatchDef &def, const campaign::Cast &cast, const campaign::Memories &memories, campaign::Save &save)
    : def_(def), cast_(cast), memories_(memories), save_(save)
{
}

void Notebook::Tab(const std::string &tab)
{
    if (tab == "characters" || tab == "ingredients" || tab == "keepsakes")
        tab_ = tab, selected_.clear();
}

void Notebook::Select(const std::string &id) { selected_ = id, skill_.clear(); }

bool Notebook::ToggleFight(const std::string &character)
{
    if (readOnly_ || character == "player")
        return false;
    if (const auto at = std::ranges::find(save_.party, character); at != save_.party.end())
        save_.party.erase(at);
    else
        save_.party.push_back(character);
    return true;
}

nlohmann::json Notebook::Page() const
{
    // 1. Who: the portrait, the level in the roster, the stats at that level.
    nlohmann::json p = nlohmann::json::object();
    const campaign::CastMember *m = cast_.Find(selected_);
    const std::string sprite = selected_ == "player" ? (save_.player.gender == "male" ? "player_male" : "player_female") : m ? m->sprite : "";
    p["portrait"] = "art/sprites/" + sprite + "/face_calm.png";
    int level = 1;
    for (const campaign::Member &r : save_.roster)
        if (r.id == selected_)
            level = r.level;
    p["level"] = level;
    p["fights"] = selected_ == "player" || std::ranges::find(save_.party, selected_) != save_.party.end();
    p["canToggle"] = selected_ != "player" && !readOnly_;
    p["locked"] = !p["canToggle"].get<bool>();
    p["skills"] = nlohmann::json::array();
    if (!growth_ || !m)
        return p;
    const umbra::match::Unit u = campaign::MakeUnit(def_, cast_, *growth_, selected_, level, 0, false);
    p["stats"] = {{"hp", u.maxHp}, {"attack", u.attack}, {"defence", u.defence}, {"speed", u.speed}, {"move", u.move}};
    // 2. Skills: each by its id (its chosen upgrade's name when one was chosen); which have upgrades.
    for (const std::int8_t a : u.abilities)
    {
        if (a < 0)
            continue;
        const umbra::match::AbilityDef &base = def_.abilities[static_cast<std::size_t>(a)];
        const std::string now = campaign::AbilityFor(save_, base.id);
        const auto shown = std::ranges::find(def_.abilities, now, &umbra::match::AbilityDef::id);
        bool upgradable = false;
        for (const auto &[recruit, list] : memories_.byRecruit)
            for (const campaign::Memory &mem : list)
                upgradable |= mem.upgrade == base.id;
        p["skills"].push_back({{"id", base.id}, {"name", shown == def_.abilities.end() ? base.name : shown->name}, {"upgradable", upgradable}, {"selected", base.id == skill_}});
    }
    // 3. The chosen skill: its line (range, power, HP cost) and its upgrade's two versions.
    const auto it = std::ranges::find(def_.abilities, skill_, &umbra::match::AbilityDef::id);
    if (it == def_.abilities.end())
        return p;
    std::string line = words_.value("skillLine", std::string("range {range}, power {power}, HP {hp}"));
    for (const auto &[key, value] : std::map<std::string, int>{{"{range}", it->range}, {"{power}", it->power}, {"{hp}", it->hpCost}})
        if (const auto at = line.find(key); at != std::string::npos)
            line.replace(at, key.size(), std::to_string(value));
    nlohmann::json versions = nlohmann::json::array();
    for (const auto &[recruit, list] : memories_.byRecruit)
        for (const campaign::Memory &mem : list)
            if (mem.upgrade == it->id)
                for (const auto &[answer, version] : mem.versions)
                {
                    const auto v = std::ranges::find(def_.abilities, version, &umbra::match::AbilityDef::id);
                    versions.push_back({{"name", v == def_.abilities.end() ? version : v->name}, {"chosen", campaign::AbilityFor(save_, it->id) == version},
                                        {"known", save_.memories.contains(mem.id)}, {"unknown", !save_.memories.contains(mem.id)}});
                }
    p["skill"] = {{"name", it->name}, {"line", line}, {"versions", versions}};
    return p;
}

nlohmann::json Notebook::Data() const
{
    nlohmann::json d{{"tab", tab_}, {"selected", selected_}, {"readOnly", readOnly_}, {"entries", nlohmann::json::array()}};
    const auto entry = [&](const std::string &id, const std::string &name, bool known) {
        d["entries"].push_back({{"id", id}, {"name", known ? name : "?"}, {"known", known}, {"selected", id == selected_}});
        if (id == selected_)
            d["selectedName"] = known ? name : "?";
    };
    if (tab_ == "characters")
    {
        // 1. The party, then each recruit with memories (a ? until joined), with memories back.
        for (const char *who : {"player", "bao", "mai"})
            entry(who, cast_.Find(who) ? campaign::FillTokens(cast_.Find(who)->name, save_.player, cast_) : who, true); // the player by name
        for (const auto &[recruit, list] : memories_.byRecruit)
            entry(recruit, cast_.Find(recruit) ? cast_.Find(recruit)->name : recruit, save_.joined.contains(recruit));
        if (!selected_.empty())
        {
            nlohmann::json memories = nlohmann::json::array();
            if (memories_.byRecruit.contains(selected_))
                for (const campaign::Memory &m : memories_.byRecruit.at(selected_))
                    memories.push_back({{"name", save_.memories.contains(m.id) ? m.name : "?"}});
            nlohmann::json slots = nlohmann::json::array();
            const auto it = save_.slots.find(selected_);
            for (int i = 0; i < 4; ++i)
                slots.push_back({{"slot", i}, {"item", it == save_.slots.end() ? "" : it->second[static_cast<std::size_t>(i)]}});
            d["detail"] = Page();
            d["detail"]["memories"] = memories, d["detail"]["slots"] = slots;
            d["detail"]["memoryCount"] = std::to_string(std::ranges::count_if(memories, [](const nlohmann::json &x) { return x["name"] != "?"; })) + "/" + std::to_string(memories.size());
        }
        return d;
    }
    // 2. Ingredients or keepsakes: known once in storage (even used up).
    const umbra::match::ItemFamily family = tab_ == "ingredients" ? umbra::match::ItemFamily::Ingredient : umbra::match::ItemFamily::Keepsake;
    for (const umbra::match::ItemDef &item : def_.items)
        if (item.family == family)
            entry(item.id, item.name, save_.storage.contains(item.id));
    if (!selected_.empty() && save_.storage.contains(selected_))
        d["detail"] = {{"count", save_.storage.at(selected_)}};
    return d;
}

bool Notebook::SetSlot(const std::string &character, int slot, const std::string &item)
{
    if (readOnly_ || slot < 0 || slot > 3 || (!item.empty() && !campaign::SlotOk(def_, item)))
        return false;
    save_.slots[character][static_cast<std::size_t>(slot)] = item;
    return true;
}
