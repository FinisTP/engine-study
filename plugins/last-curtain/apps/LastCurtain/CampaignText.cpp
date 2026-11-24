#include "CampaignText.hpp"

bool CampaignText::Load(const std::filesystem::path &csv)
{
    std::string error;
    english = umbra::night::EnglishChosen();
    return table.Load(csv, error);
}

std::string CampaignText::Get(const std::string &key, const std::string &fallback) const
{
    return table.text.contains(key) ? table.Get(key, english) : fallback;
}

void CampaignText::Fill(umbra::campaign::StageDef &s) const
{
    const std::string at = "stage." + s.id + ".";
    s.title = Get(at + "title", s.title), s.titleEn = table.text.contains(at + "title") ? table.text.at(at + "title").second : s.titleEn;
    s.aimStart = Get(at + "aim", s.aimStart);
    for (std::size_t i = 0; i < s.twists.size(); ++i)
        s.twists[i].name = Get(at + "twist." + std::to_string(i), s.twists[i].name);
}

void CampaignText::Fill(std::vector<umbra::campaign::StageDef> &stages) const
{
    for (auto &s : stages)
        Fill(s);
}

void CampaignText::Fill(umbra::campaign::TheatreMap &map) const
{
    for (auto &t : map.tickets)
        t.name = Get("ticket." + t.id + ".name", t.name), t.lock = Get("ticket." + t.id + ".lock", t.lock);
}

void CampaignText::Fill(umbra::campaign::Cast &cast) const
{
    for (auto &m : cast.members)
        m.name = Get("cast." + m.id + ".name", m.name);
}

void CampaignText::Fill(umbra::campaign::Skills &skills) const
{
    // Every ability with a story in the table, whether or not skills.json names it.
    const std::string start = "lore.";
    for (auto it = table.text.lower_bound(start); it != table.text.end() && it->first.starts_with(start); ++it)
        skills.lore[it->first.substr(start.size())] = table.Get(it->first, english);
    for (auto &[who, life] : skills.timeline) // timeline.<who>.<n>, n from 0
        for (std::size_t n = 0; n < life.size(); ++n)
            life[n].text = table.Get("timeline." + who + "." + std::to_string(n), english);
}

void CampaignText::Fill(umbra::campaign::Recipes &recipes) const
{
    for (auto &r : recipes.list)
        r.name = Get("recipe." + r.id + ".name", r.name), r.text = Get("recipe." + r.id + ".text", r.text);
}

void CampaignText::Fill(umbra::campaign::Memories &memories) const
{
    for (auto &[who, list] : memories.byRecruit)
        for (auto &m : list)
            m.name = Get("memory." + m.id + ".name", m.name);
}
