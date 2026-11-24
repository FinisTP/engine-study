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

void Notebook::Select(const std::string &id) { selected_ = id, skill_.clear(), picked_.clear(), bagTarget_ = skillTarget_ = -1; }

int Notebook::Level(const std::string &who) const
{
    const auto it = std::ranges::find(save_.roster, who, &campaign::Member::id);
    return it == save_.roster.end() ? 1 : it->level;
}

std::vector<std::string> Notebook::Known() const
{
    std::vector<std::string> out;
    if (!growth_ || !cast_.Find(selected_))
        return out;
    for (const std::int8_t a : campaign::MakeUnit(def_, cast_, *growth_, selected_, Level(selected_), 0, false).abilities)
        if (a >= 0)
            out.push_back(def_.abilities[static_cast<std::size_t>(a)].id);
    return out;
}

std::vector<std::string> Notebook::Carried() const
{
    const std::vector<std::string> known = Known();
    if (skills_)
        return campaign::Loadout(*skills_, save_, selected_, known, Level(selected_));
    return {known.begin(), known.begin() + static_cast<std::ptrdiff_t>(std::min(known.size(), campaign::maxCarried))};
}

int Notebook::Free(const std::string &item) const
{
    int left = save_.storage.contains(item) ? save_.storage.at(item) : 0;
    for (const auto &[who, slots] : save_.slots)
        left -= static_cast<int>(std::ranges::count(slots, item));
    return std::max(0, left);
}

std::string Notebook::SkillText(const umbra::match::AbilityDef &a) const
{
    // Where it works, what it does in numbers, how far, what it costs: each only when it says something.
    const auto fill = [&](const std::string &key, const std::string &fallback, int n) {
        std::string t = Word(key, fallback);
        if (const auto at = t.find("{n}"); at != std::string::npos)
            t.replace(at, 3, std::to_string(n));
        return t;
    };
    using umbra::match::AbilityClass;
    const char *where = a.use == umbra::match::Use::Board ? "skillBoard" : a.use == umbra::match::Use::Battle ? "skillBattle" : "skillPassive";
    std::string text = Word(where, a.use == umbra::match::Use::Board ? "On the board." : a.use == umbra::match::Use::Battle ? "In battle." : "Always on.");
    if (a.power > 0 && (a.kind == AbilityClass::Strike || a.kind == AbilityClass::Hit))
        text += " " + fill("skillPower", "Power {n} plus Attack.", a.power);
    else if (a.power > 0 && a.kind == AbilityClass::Heal)
        text += " " + fill("skillHeal", "Heals {n} HP.", a.power);
    if (a.range > 1)
        text += " " + fill("skillRange", "Reach {n} squares.", a.range);
    if (a.hpCost > 0)
        text += " " + fill("skillCost", "Costs {n} HP.", a.hpCost);
    return text;
}

void Notebook::Pick(const std::string &what)
{
    // An empty slot picked first is where the next Take goes.
    picked_ = what;
    // A skill's card also shows its upgrade's versions (the page's "skill").
    if (what.starts_with("learned:") || what.starts_with("locked:"))
        skill_ = what.substr(what.find(':') + 1);
    else if (what.starts_with("skill:"))
    {
        const std::vector<std::string> carried = Carried();
        const std::size_t k = static_cast<std::size_t>(std::atoi(what.c_str() + 6));
        skill_ = k < carried.size() ? carried[k] : "";
    }
    else
        skill_.clear();
    const auto index = [&](const char *prefix) { return what.starts_with(prefix) ? std::atoi(what.c_str() + std::char_traits<char>::length(prefix)) : -1; };
    if (const int b = index("bag:"); b >= 0)
        bagTarget_ = save_.slots.contains(selected_) && !save_.slots.at(selected_)[static_cast<std::size_t>(std::min(b, 3))].empty() ? -1 : b;
    if (const int k = index("skill:"); k >= 0)
        skillTarget_ = k < static_cast<int>(Carried().size()) ? -1 : k;
}

bool Notebook::Take()
{
    if (readOnly_)
        return false;
    // 1. A skill in stock: carried (Equip appends; the slot order follows the loadout).
    if (picked_.starts_with("learned:") && skills_)
    {
        const bool ok = campaign::Equip(*skills_, save_, selected_, Known(), Level(selected_), picked_.substr(8));
        if (ok)
            skillTarget_ = -1;
        return ok;
    }
    // 2. An item from storage: into the empty slot picked first, else the first empty one.
    if (picked_.starts_with("store:"))
    {
        const std::string item = picked_.substr(6);
        if (Free(item) <= 0)
            return false;
        auto &slots = save_.slots[selected_];
        int slot = bagTarget_ >= 0 && bagTarget_ < 4 && slots[static_cast<std::size_t>(bagTarget_)].empty() ? bagTarget_ : -1;
        for (int i = 0; i < 4 && slot < 0; ++i)
            if (slots[static_cast<std::size_t>(i)].empty())
                slot = i;
        if (slot < 0 || !SetSlot(selected_, slot, item))
            return false;
        bagTarget_ = -1;
        return true;
    }
    return false;
}

bool Notebook::Drop()
{
    if (readOnly_)
        return false;
    if (picked_.starts_with("skill:") && skills_)
    {
        const std::vector<std::string> carried = Carried();
        const std::size_t k = static_cast<std::size_t>(std::atoi(picked_.c_str() + 6));
        return k < carried.size() && campaign::Unequip(*skills_, save_, selected_, Known(), Level(selected_), carried[k]);
    }
    if (picked_.starts_with("bag:"))
    {
        const int k = std::atoi(picked_.c_str() + 4);
        return save_.slots.contains(selected_) && !save_.slots.at(selected_)[static_cast<std::size_t>(std::clamp(k, 0, 3))].empty() && SetSlot(selected_, k, "");
    }
    return false;
}

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
    const int level = Level(selected_);
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
    // 2b. Pick then place: four skill slots, the skills in stock and those still locked, the items storage can spare.
    const auto name = [&](const std::string &id) {
        const auto a = std::ranges::find(def_.abilities, campaign::AbilityFor(save_, id), &umbra::match::AbilityDef::id);
        return a == def_.abilities.end() ? id : a->name;
    };
    const std::vector<std::string> carried = Carried();
    p["loadout"] = nlohmann::json::array();
    for (std::size_t i = 0; i < campaign::maxCarried; ++i)
        p["loadout"].push_back(i < carried.size() ? nlohmann::json{{"index", i}, {"id", carried[i]}, {"name", name(carried[i])}, {"empty", false}, {"picked", picked_ == "skill:" + std::to_string(i)}}
                                                  : nlohmann::json{{"index", i}, {"id", ""}, {"name", Word("emptySlot", "(empty)")}, {"empty", true}, {"picked", picked_ == "skill:" + std::to_string(i)}});
    p["learned"] = nlohmann::json::array(), p["locked"] = nlohmann::json::array();
    for (const std::string &id : Known())
        if (const int at = skills_ ? campaign::UnlockLevel(*skills_, id) : 1; at > level)
        {
            std::string when = Word("unlockAt", "opens at level {n}");
            if (const auto k = when.find("{n}"); k != std::string::npos)
                when.replace(k, 3, std::to_string(at));
            p["locked"].push_back({{"id", id}, {"name", name(id)}, {"level", at}, {"when", when}, {"picked", picked_ == "locked:" + id}});
        }
        else if (std::ranges::find(carried, id) == carried.end())
            p["learned"].push_back({{"id", id}, {"name", name(id)}, {"picked", picked_ == "learned:" + id}});
    p["store"] = nlohmann::json::array();
    for (const auto &[item, count] : save_.storage)
        if (count > 0 && campaign::SlotOk(def_, item))
            p["store"].push_back({{"id", item}, {"count", Free(item)}, {"picked", picked_ == "store:" + item}});
    // 2c. The card: what is picked, what it does, and whether it can be carried or put back.
    const auto bag = save_.slots.contains(selected_) ? save_.slots.at(selected_) : std::array<std::string, 4>{};
    const auto skillCard = [&](const std::string &id, bool take, bool drop, const std::string &hint) {
        const auto a = std::ranges::find(def_.abilities, campaign::AbilityFor(save_, id), &umbra::match::AbilityDef::id);
        if (a == def_.abilities.end())
            return;
        const std::string lore = skills_ && skills_->lore.contains(id) ? skills_->lore.at(id) : "";
        p["card"] = {{"kind", "skill"}, {"name", a->name}, {"lore", lore}, {"text", SkillText(*a)}, {"take", take && !readOnly_}, {"drop", drop && !readOnly_}, {"hint", hint}};
    };
    const auto itemCard = [&](const std::string &id, bool take, bool drop) {
        const auto it = std::ranges::find(def_.items, id, &umbra::match::ItemDef::id);
        p["card"] = {{"kind", "item"}, {"id", id}, {"name", it == def_.items.end() ? id : it->name}, {"lore", ""}, {"text", it == def_.items.end() ? "" : it->text},
                     {"take", take && !readOnly_}, {"drop", drop && !readOnly_}, {"hint", ""}};
    };
    const int full = static_cast<int>(carried.size()) >= static_cast<int>(campaign::maxCarried);
    if (picked_.starts_with("learned:"))
        skillCard(picked_.substr(8), !full, false, full ? Word("skillsFull", "Four skills already: put one back first.") : "");
    else if (picked_.starts_with("locked:"))
        skillCard(picked_.substr(7), false, false, "");
    else if (picked_.starts_with("skill:"))
    {
        const std::size_t k = static_cast<std::size_t>(std::atoi(picked_.c_str() + 6));
        if (k < carried.size())
            skillCard(carried[k], false, carried.size() > 1, "");
        else
            p["card"] = {{"kind", "hint"}, {"name", ""}, {"lore", ""}, {"text", Word("pickSkill", "Pick a skill on the right to carry.")}, {"take", false}, {"drop", false}, {"hint", ""}};
    }
    else if (picked_.starts_with("store:"))
        itemCard(picked_.substr(6), Free(picked_.substr(6)) > 0 && std::ranges::any_of(bag, [](const std::string &x) { return x.empty(); }), false);
    else if (picked_.starts_with("bag:"))
    {
        const std::string held = bag[static_cast<std::size_t>(std::clamp(std::atoi(picked_.c_str() + 4), 0, 3))];
        if (!held.empty())
            itemCard(held, false, true);
        else
            p["card"] = {{"kind", "hint"}, {"name", ""}, {"lore", ""}, {"text", Word("pickItem", "Pick an item from storage on the right.")}, {"take", false}, {"drop", false}, {"hint", ""}};
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
        // 1. The player, then whoever is in the roster or has joined; no one not met yet (note 18).
        const auto met = [&](const std::string &who) {
            return who == "player" || save_.joined.contains(who) || std::ranges::any_of(save_.roster, [&](const campaign::Member &m) { return m.id == who; });
        };
        for (const char *who : {"player", "bao", "mai"})
            if (met(who))
                entry(who, cast_.Find(who) ? campaign::FillTokens(cast_.Find(who)->name, save_.player, cast_) : who, true); // the player by name
        for (const auto &[recruit, list] : memories_.byRecruit)
            if (met(recruit))
                entry(recruit, cast_.Find(recruit) ? cast_.Find(recruit)->name : recruit, true);
        if (!selected_.empty())
        {
            nlohmann::json memories = nlohmann::json::array();
            if (memories_.byRecruit.contains(selected_))
                for (const campaign::Memory &m : memories_.byRecruit.at(selected_))
                    memories.push_back({{"name", save_.memories.contains(m.id) ? m.name : "?"}});
            nlohmann::json slots = nlohmann::json::array();
            const auto it = save_.slots.find(selected_);
            for (int i = 0; i < 4; ++i)
                slots.push_back({{"slot", i}, {"item", it == save_.slots.end() ? "" : it->second[static_cast<std::size_t>(i)]}, {"picked", picked_ == "bag:" + std::to_string(i)}});
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
