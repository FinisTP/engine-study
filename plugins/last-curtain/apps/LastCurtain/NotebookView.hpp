#pragma once

#include <CampaignCafe.hpp>

#include <nlohmann/json.hpp>

#include <string>

// The notebook, Sổ tay (campaign plan 6, ux/notebook.md): tabs characters, ingredients, keepsakes; a list
// with "?" for what is not known yet; the selected entry's detail; the 4 slots of a character, edited only
// outside battle. It remembers its tab and entry.
class Notebook final
{
public:
    Notebook(const umbra::match::MatchDef &def, const umbra::campaign::Cast &cast, const umbra::campaign::Memories &memories, umbra::campaign::Save &save);

    [[nodiscard]] nlohmann::json Data() const;
    void Tab(const std::string &tab); // characters, ingredients or keepsakes
    void Select(const std::string &id);
    void SetReadOnly(bool readOnly) { readOnly_ = readOnly; }
    bool SetSlot(const std::string &character, int slot, const std::string &item); // "" empties it
    // The character page (the mockup's middle and right columns): stats need the growth table; words give the
    // skill line ("skillLine": "{range} {power} {hp}" filled in).
    void SetGrowth(const umbra::campaign::Growth *growth) { growth_ = growth; }
    void SetWords(nlohmann::json words) { words_ = std::move(words); }
    void Skill(const std::string &ability) { skill_ = ability; } // the skill shown on the right
    bool ToggleFight(const std::string &character);           // in or out of the next fight; the player always fights

private:
    const umbra::match::MatchDef &def_;
    const umbra::campaign::Cast &cast_;
    const umbra::campaign::Memories &memories_;
    umbra::campaign::Save &save_;
    std::string tab_ = "characters", selected_, skill_;
    const umbra::campaign::Growth *growth_ = nullptr;
    nlohmann::json words_ = nlohmann::json::object();
    [[nodiscard]] nlohmann::json Page() const; // the selected character's page
    bool readOnly_ = false;
};
