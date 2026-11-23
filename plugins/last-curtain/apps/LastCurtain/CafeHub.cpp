#include "CafeHub.hpp"

#include <algorithm>

namespace campaign = umbra::campaign;

CafeHub::CafeHub(const umbra::match::MatchDef &def, const campaign::Cast &cast, const campaign::Recipes &recipes, const campaign::Memories &memories,
                 const campaign::Talks &talks, std::vector<campaign::StageDef> stages, campaign::Save &save, int boostCap)
    : def_(def), cast_(cast), recipes_(recipes), memories_(memories), talks_(talks), stages_(std::move(stages)), save_(save), cap_(boostCap)
{
}

nlohmann::json CafeHub::Data() const
{
    nlohmann::json d;
    // 1. People in the cafe: the allies, Le Thanh, the recruits who joined; a mark when they have a new talk.
    const std::vector<const campaign::Talk *> open = campaign::OpenTalks(talks_, save_, 99);
    std::vector<std::string> present{"lethanh", "bao", "mai"};
    for (const std::string &r : save_.joined)
        present.push_back(r);
    d["people"] = nlohmann::json::array();
    for (const std::string &who : present)
    {
        const campaign::CastMember *m = cast_.Find(who);
        const bool talk = std::ranges::any_of(open, [&](const campaign::Talk *t) { return t->who == who; });
        d["people"].push_back({{"who", who}, {"name", m ? m->name : who}, {"talk", talk}});
    }
    // 2. Recipes known: memory drinks first, then general; what each lacks; a memory's two versions.
    d["recipes"] = nlohmann::json::array();
    bool any = false;
    for (const char *kind : {"memory", "general"})
        for (const campaign::Recipe &r : recipes_.list)
        {
            if (r.kind != kind || !save_.recipes.contains(r.id))
                continue;
            nlohmann::json row{{"id", r.id}, {"name", r.name}, {"text", r.text}, {"kind", r.kind}, {"for", r.forWho}, {"lacks", nlohmann::json::array()},
                               {"versions", nlohmann::json::array()}};
            for (const campaign::Missing &m : campaign::Lacks(save_, r, stages_))
                row["lacks"].push_back({{"item", m.item}, {"need", m.need}, {"have", m.have}, {"where", m.where}});
            if (const campaign::Memory *mem = r.kind == "memory" ? campaign::NextMemory(memories_, save_, r.forWho, "recipe", r.id) : nullptr)
                for (const auto &[answer, version] : mem->versions)
                {
                    const auto it = std::ranges::find(def_.abilities, version, &umbra::match::AbilityDef::id);
                    row["versions"].push_back({{"answer", answer}, {"ability", version}, {"name", it == def_.abilities.end() ? version : it->name}});
                }
            row["ok"] = row["lacks"].empty() && (r.kind == "general" || !row["versions"].empty());
            any |= row["ok"].get<bool>();
            d["recipes"].push_back(row);
        }
    d["canBrew"] = any;
    // 3. Keepsakes in storage that would bring a joined recruit's memory back.
    d["keepsakes"] = nlohmann::json::array();
    for (const auto &[item, count] : save_.storage)
        for (const std::string &r : save_.joined)
            if (count > 0 && campaign::NextMemory(memories_, save_, r, "keepsake", item))
            {
                const auto it = std::ranges::find(def_.items, item, &umbra::match::ItemDef::id);
                d["keepsakes"].push_back({{"item", item}, {"name", it == def_.items.end() ? item : it->name}, {"for", r}});
            }
    d["toasts"] = nlohmann::json::array();
    for (const Toasted &t : toasts_)
        d["toasts"].push_back({{"text", t.text}});
    return d;
}

std::string CafeHub::Talk(const std::string &who)
{
    for (const campaign::Talk *t : campaign::OpenTalks(talks_, save_, 99))
        if (t->who == who)
        {
            campaign::Talked(save_, *t);
            if (!t->memory.empty())
                for (const auto &[recruit, list] : memories_.byRecruit)
                    for (const campaign::Memory &m : list)
                        if (m.id == t->memory)
                            pending_ = &m;
            return t->scene;
        }
    return {};
}

std::string CafeHub::Brew(const std::string &recipe, const std::string &forWho)
{
    const campaign::Recipe *r = recipes_.Find(recipe);
    if (!r || !save_.recipes.contains(recipe))
        return {};
    if (r->kind == "general")
    {
        if (campaign::Boost(save_, recipes_, *r, forWho, cap_))
            Toast(r->name + " · " + (cast_.Find(forWho) ? cast_.Find(forWho)->name : forWho));
        return {};
    }
    const campaign::Memory *m = campaign::NextMemory(memories_, save_, r->forWho, "recipe", r->id);
    if (!m || !campaign::Brew(save_, *r))
        return {};
    pending_ = m;
    return m->scene;
}

std::string CafeHub::Give(const std::string &keepsake)
{
    for (const std::string &r : save_.joined)
        if (const campaign::Memory *m = campaign::NextMemory(memories_, save_, r, "keepsake", keepsake); m && save_.storage[keepsake] > 0)
            return pending_ = m, m->scene;
    return {};
}

void CafeHub::SceneDone(const campaign::Facts &chosen)
{
    for (const auto &[key, value] : chosen)
        save_.facts[key] = value;
    if (pending_ && campaign::Remember(save_, *pending_, chosen))
        Toast(pending_->name);
    pending_ = nullptr;
}

void CafeHub::Update(float dt)
{
    for (Toasted &t : toasts_)
        t.left -= dt;
    std::erase_if(toasts_, [](const Toasted &t) { return t.left <= 0.0F; });
}

void CafeHub::Toast(std::string text)
{
    toasts_.push_back({std::move(text), 3.0F});
    if (toasts_.size() > 3)
        toasts_.erase(toasts_.begin());
}
