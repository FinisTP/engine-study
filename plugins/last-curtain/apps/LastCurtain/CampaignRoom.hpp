#pragma once

#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

// The campaign's café as a room (CampaignRoom.md): the active character walks; everyone else stands at
// their place; E talks to the nearest person, brews at the counter, opens the notebook or leaves by the
// door; Tab switches who walks. Drawn by CafeRoom; the café's screens are CafeHub's.
class CampaignRoom final
{
public:
    struct Person
    {
        std::string who;
        glm::vec2 at{0.0F};
    };
    // room: assets/cafe/room.json with "campaign" = assets/campaign/cafe-room.json. party: who can walk (the
    // player first); others: who else is in (Le Thanh).
    CampaignRoom(nlohmann::json room, std::vector<std::string> party, std::vector<std::string> others);

    void Move(glm::vec2 direction, float dt);
    void WalkTo(glm::vec2 at) { goal_ = at; }
    void Place(glm::vec2 at) { at_ = at, goal_ = {-1.0F, -1.0F}; }
    void Update(float dt);
    void Switch(); // the next in the party walks from here; the one who walked goes back to its place
    // What E does here: "talk:<who>", "brew", "notebook", "door" or "".
    [[nodiscard]] std::string Interact() const;
    [[nodiscard]] const std::string &Active() const { return party_[active_]; }
    [[nodiscard]] glm::vec2 Position() const { return at_; }
    [[nodiscard]] std::vector<Person> People() const; // everyone standing (not the one walking)
    [[nodiscard]] const nlohmann::json &Room() const { return room_; }

private:
    [[nodiscard]] bool Blocked(glm::vec2 p) const;
    [[nodiscard]] glm::vec2 PlaceOf(const std::string &who) const;
    nlohmann::json room_;
    std::vector<std::string> party_, others_;
    std::size_t active_ = 0;
    glm::vec2 at_{6.0F, 6.0F}, goal_{-1.0F, -1.0F};
};
