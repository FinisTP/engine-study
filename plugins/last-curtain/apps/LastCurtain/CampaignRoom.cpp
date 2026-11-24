#include "CampaignRoom.hpp"

#include <glm/geometric.hpp>

#include <algorithm>

namespace
{
    glm::vec2 Vec(const nlohmann::json &j) { return {j[0].get<float>(), j[1].get<float>()}; }
}

CampaignRoom::CampaignRoom(nlohmann::json room, std::vector<std::string> party, std::vector<std::string> others)
    : room_(std::move(room)), party_(std::move(party)), others_(std::move(others))
{
    if (party_.empty())
        party_.push_back("player");
    at_ = room_.contains("start") ? Vec(room_["start"]) : glm::vec2{6.0F, 6.0F};
}

bool CampaignRoom::Blocked(glm::vec2 p) const
{
    // Inside the walls, outside every block, with a little margin (as the career café).
    const float margin = 0.25F;
    if (p.x < margin || p.y < margin || p.x > room_["size"][0].get<float>() - margin || p.y > room_["size"][1].get<float>() - margin)
        return true;
    for (const nlohmann::json &b : room_["blocks"])
        if (p.x > b["min"][0].get<float>() - margin && p.x < b["max"][0].get<float>() + margin && p.y > b["min"][1].get<float>() - margin &&
            p.y < b["max"][1].get<float>() + margin)
            return true;
    return false;
}

void CampaignRoom::Move(glm::vec2 direction, float dt)
{
    if (glm::length(direction) < 0.001F || dt <= 0.0F)
        return;
    goal_ = {-1.0F, -1.0F};
    // One axis at a time, so the walker slides along the counter.
    const glm::vec2 step = glm::normalize(direction) * room_.value("speed", 3.0F) * dt;
    if (!Blocked({at_.x + step.x, at_.y}))
        at_.x += step.x;
    if (!Blocked({at_.x, at_.y + step.y}))
        at_.y += step.y;
}

void CampaignRoom::Update(float dt)
{
    // A clicked walk: toward the goal until there or stopped.
    if (goal_.x < 0.0F || dt <= 0.0F)
        return;
    const glm::vec2 to = goal_ - at_;
    if (glm::length(to) < 0.08F)
        return void(goal_ = {-1.0F, -1.0F});
    const glm::vec2 before = at_, goal = goal_;
    Move(glm::length(to) > room_.value("speed", 3.0F) * dt ? to : to / dt / room_.value("speed", 3.0F), dt);
    goal_ = glm::length(at_ - before) < 0.0001F ? glm::vec2{-1.0F, -1.0F} : goal; // Move clears it; keep walking
}

glm::vec2 CampaignRoom::PlaceOf(const std::string &who) const
{
    const nlohmann::json &people = room_["campaign"]["people"];
    return people.contains(who) ? Vec(people[who]) : glm::vec2{6.0F, 5.5F};
}

void CampaignRoom::Switch()
{
    if (party_.size() > 1)
        active_ = (active_ + 1) % party_.size(); // the newcomer walks on from here
}

std::vector<CampaignRoom::Person> CampaignRoom::People() const
{
    std::vector<Person> out;
    for (const std::string &who : others_)
        out.push_back({who, PlaceOf(who)});
    for (std::size_t i = 0; i < party_.size(); ++i)
        if (i != active_)
            out.push_back({party_[i], PlaceOf(party_[i])});
    return out;
}

std::string CampaignRoom::Interact() const
{
    // The nearest person or spot within reach (a person behind the counter is reached across it).
    const float reach = room_.value("reach", 1.1F) + 0.4F;
    std::string best;
    float bestDistance = reach;
    for (const Person &p : People())
        if (const float d = glm::length(p.at - at_); d <= bestDistance)
            best = "talk:" + p.who, bestDistance = d;
    for (const nlohmann::json &s : room_["campaign"]["spots"])
        if (const float d = glm::length(Vec(s["at"]) - at_); d <= bestDistance)
            best = s["kind"].get<std::string>(), bestDistance = d;
    return best;
}
