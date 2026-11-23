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

void Notebook::Select(const std::string &id) { selected_ = id; }

nlohmann::json Notebook::Data() const
{
    nlohmann::json d{{"tab", tab_}, {"selected", selected_}, {"readOnly", readOnly_}, {"entries", nlohmann::json::array()}};
    const auto entry = [&](const std::string &id, const std::string &name, bool known) {
        d["entries"].push_back({{"id", id}, {"name", known ? name : "?"}, {"known", known}, {"selected", id == selected_}});
    };
    if (tab_ == "characters")
    {
        // 1. The party, then each recruit with memories (a ? until joined), with memories back.
        for (const char *who : {"player", "bao", "mai"})
            entry(who, cast_.Find(who) ? cast_.Find(who)->name : who, true);
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
            d["detail"] = {{"memories", memories}, {"slots", slots}};
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
