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

private:
    const umbra::match::MatchDef &def_;
    const umbra::campaign::Cast &cast_;
    const umbra::campaign::Memories &memories_;
    umbra::campaign::Save &save_;
    std::string tab_ = "characters", selected_;
    bool readOnly_ = false;
};
