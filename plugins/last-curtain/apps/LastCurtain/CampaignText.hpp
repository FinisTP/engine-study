#pragma once

#include <Campaign.hpp>
#include <CampaignCafe.hpp>
#include <Localization.hpp>

// The campaign's display text from localization.csv (Localization.md "Data"): stage titles, aims and
// twists, the map's names and locks, cast names, skill lore, recipes and memories. The data files keep
// only ids and numbers; each loader's result is filled here in the chosen language.
struct CampaignText
{
    umbra::night::Localization table;
    bool english = false;

    // Reads assets/localization.csv in the settings' language; false if it can't be read.
    bool Load(const std::filesystem::path &csv);
    [[nodiscard]] std::string Get(const std::string &key, const std::string &fallback = "") const;
    void Fill(umbra::campaign::StageDef &stage) const;
    void Fill(std::vector<umbra::campaign::StageDef> &stages) const;
    void Fill(umbra::campaign::TheatreMap &map) const;
    void Fill(umbra::campaign::Cast &cast) const;
    void Fill(umbra::campaign::Skills &skills) const;
    void Fill(umbra::campaign::Recipes &recipes) const;
    void Fill(umbra::campaign::Memories &memories) const;
    void Fill(umbra::campaign::Quests &quests) const; // quest.<id>
};
