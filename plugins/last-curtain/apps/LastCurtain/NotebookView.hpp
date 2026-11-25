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
    [[nodiscard]] const std::string &CurrentTab() const { return tab_; }
    void Select(const std::string &id);
    void SetReadOnly(bool readOnly) { readOnly_ = readOnly; }
    bool SetSlot(const std::string &character, int slot, const std::string &item); // "" empties it
    // The character page (the mockup's middle and right columns): stats need the growth table; words give the
    // skill line ("skillLine": "{range} {power} {hp}" filled in).
    void SetGrowth(const umbra::campaign::Growth *growth) { growth_ = growth; }
    void SetWords(nlohmann::json words) { words_ = std::move(words); }
    void Skill(const std::string &ability) { skill_ = ability; } // the skill shown on the right
    bool ToggleFight(const std::string &character);           // in or out of the next fight; the player always fights
    // Pick then place (feedback 2026-11-24 15:10, notes 16, 17): pick a slot ("skill:<i>", "bag:<i>") or something in
    // stock ("learned:<id>", "locked:<id>", "store:<item>"); its card says what it is; Take carries the picked stock
    // (into the empty slot picked before it, else the first empty one), Drop puts a picked slot's back.
    void SetSkills(const umbra::campaign::Skills *skills) { skills_ = skills; }
    nlohmann::json Life(const std::string &who) const; // Ky uc: [{year, skill, known, text}] (??? until known)
    void Pick(const std::string &what);
    bool Take();
    bool Drop();

private:
    const umbra::match::MatchDef &def_;
    const umbra::campaign::Cast &cast_;
    const umbra::campaign::Memories &memories_;
    umbra::campaign::Save &save_;
    std::string tab_ = "characters", selected_, skill_;
    const umbra::campaign::Growth *growth_ = nullptr;
    const umbra::campaign::Skills *skills_ = nullptr;
    std::string picked_;    // what the card shows
    int bagTarget_ = -1, skillTarget_ = -1; // an empty slot picked first: where the next Take goes
    [[nodiscard]] int Level(const std::string &who) const;
    [[nodiscard]] std::vector<std::string> Known() const;   // the selected one's abilities by id, kit order
    [[nodiscard]] std::vector<std::string> Carried() const; // its loadout (all it knows without skills.json)
    [[nodiscard]] int Free(const std::string &item) const;  // in storage and not yet in anyone's slot
    [[nodiscard]] std::string SkillText(const umbra::match::AbilityDef &a) const; // where, power, reach, cost in words
    [[nodiscard]] std::string Word(const std::string &key, const std::string &fallback) const { return words_.value(key, fallback); }
    nlohmann::json words_ = nlohmann::json::object();
    [[nodiscard]] nlohmann::json Page() const; // the selected character's page
    bool readOnly_ = false;
};
