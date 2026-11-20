#include "CafePlay.hpp"

#include <MatchDef.hpp>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

// The café hub on screen (CafePlay.md).
namespace umbra::night
{
    namespace
    {
        std::size_t U(int i) { return static_cast<std::size_t>(i); }
        glm::vec2 At(const nlohmann::json &spot) { return {spot["at"][0].get<float>(), spot["at"][1].get<float>()}; }

        std::string Fill(std::string text, std::initializer_list<std::pair<std::string, std::string>> values)
        {
            for (const auto &[key, value] : values)
                for (std::size_t at = text.find("{" + key + "}"); at != std::string::npos; at = text.find("{" + key + "}", at + value.size()))
                    text.replace(at, key.size() + 2, value);
            return text;
        }
    }

    CafePlay::CafePlay(const cafe::CafeDef &def, const match::MatchDef &match, nlohmann::json room, nlohmann::json words, cafe::Career &career,
                       std::string walker, std::uint64_t seed)
        : def_(def), match_(match), room_(std::move(room)), words_(std::move(words)), career_(career), walker_(std::move(walker)), seed_(seed)
    {
        at_ = {room_["start"][0].get<float>(), room_["start"][1].get<float>()};
        shift_ = cafe::OpenShop(def_, seed_);
        shift_.over = false;
    }

    std::string CafePlay::Word(const std::string &group, const std::string &key) const
    {
        const nlohmann::json &g = group.empty() ? words_ : words_.value(group, nlohmann::json::object());
        return g.value(key, std::string{});
    }

    bool CafePlay::Blocked(glm::vec2 p) const
    {
        // 1. Inside the room's walls, outside every block (a little margin so the walker never touches them).
        const float margin = 0.25F;
        if (p.x < margin || p.y < margin || p.x > room_["size"][0].get<float>() - margin || p.y > room_["size"][1].get<float>() - margin)
            return true;
        for (const nlohmann::json &b : room_["blocks"])
            if (p.x > b["min"][0].get<float>() - margin && p.x < b["max"][0].get<float>() + margin && p.y > b["min"][1].get<float>() - margin &&
                p.y < b["max"][1].get<float>() + margin)
                return true;
        return false;
    }

    void CafePlay::Move(glm::vec2 direction, float dt)
    {
        if (glm::length(direction) < 0.001F || dt <= 0.0F)
            return;
        goal_ = {-1.0F, -1.0F};
        // A step along each axis on its own, so the walker slides along a counter instead of sticking to it.
        const glm::vec2 step = glm::normalize(direction) * room_.value("speed", 3.0F) * dt;
        if (!Blocked({at_.x + step.x, at_.y}))
            at_.x += step.x;
        if (!Blocked({at_.x, at_.y + step.y}))
            at_.y += step.y;
    }

    void CafePlay::WalkTo(glm::vec2 at) { goal_ = at; }

    int CafePlay::Near() const
    {
        int best = -1;
        float bestDistance = room_.value("reach", 1.0F);
        for (int i = 0; i < static_cast<int>(room_["spots"].size()); ++i)
            if (const float d = glm::length(At(room_["spots"][U(i)]) - at_); d <= bestDistance)
                best = i, bestDistance = d;
        return best;
    }

    void CafePlay::Update(float dt)
    {
        if (dt <= 0.0F)
            return;
        // 1. A clicked walk: toward the goal until there, or until it stops against something.
        if (goal_.x >= 0.0F)
        {
            const glm::vec2 to = goal_ - at_;
            const float d = glm::length(to), step = room_.value("speed", 3.0F) * dt;
            const glm::vec2 before = at_;
            if (d <= step)
                at_ = Blocked(goal_) ? at_ : goal_, goal_ = {-1.0F, -1.0F};
            else
            {
                const glm::vec2 keep = goal_;
                Move(to, dt), goal_ = keep;
                if (glm::length(at_ - before) < step * 0.1F)
                    goal_ = {-1.0F, -1.0F}; // stuck against a block
            }
        }
        // 2. The shift: its clock, guests leaving; at its end the takings land in the career once.
        if (open_)
        {
            const std::size_t logged = shift_.log.size();
            cafe::Tick(def_, shift_, dt);
            for (std::size_t i = logged; i < shift_.log.size(); ++i)
                if (shift_.log[i].kind == cafe::CafeEvent::Left)
                    note_ = Word("notes", "left"), noteLeft_ = 2.5F, sounds_.push_back("hurt");
                else if (shift_.log[i].kind == cafe::CafeEvent::Arrived)
                    sounds_.push_back("door");
            if (shift_.over)
                Finish();
        }
        // 3. The talk's lines, one after another.
        if (!lines_.empty() && (lineLeft_ -= dt) <= 0.0F)
        {
            lines_.erase(lines_.begin());
            lineLeft_ = lines_.empty() ? 0.0F : room_.value("lineSeconds", 3.0F);
        }
        noteLeft_ = std::max(0.0F, noteLeft_ - dt);
    }

    void CafePlay::Finish()
    {
        // The day is done: its takings land in the career, once.
        open_ = false, done_ = true;
        if (!paid_)
            career_.money += shift_.takings + shift_.tips, paid_ = true, sounds_.push_back("level");
    }

    void CafePlay::Talk(int guest, int seat)
    {
        // 1. The first talk this ghost has with the walker not yet heard, else one with anyone.
        const std::string &ghost = def_.guests[U(guest)].id;
        if (!def_.talk.contains(ghost))
            return;
        for (const std::string who : {walker_, std::string{"any"}})
        {
            const nlohmann::json talks = def_.talk[ghost].value(who, nlohmann::json::array());
            for (std::size_t n = 0; n < talks.size(); ++n)
            {
                const std::string key = ghost + ":" + who + ":" + std::to_string(n);
                if (std::ranges::find(career_.heard, key) != career_.heard.end())
                    continue;
                career_.heard.push_back(key);
                lines_.clear();
                for (const nlohmann::json &line : talks[n])
                    lines_.push_back({line["who"] == "ghost" ? seat : -1, line["text"].get<std::string>()});
                lineLeft_ = room_.value("lineSeconds", 3.0F);
                return;
            }
        }
    }

    bool CafePlay::Interact()
    {
        const int near = Near();
        if (near < 0 || !Screen().empty())
            return false;
        const nlohmann::json &spot = room_["spots"][U(near)];
        const std::string kind = spot["kind"];
        // 1. Each kind of spot does one thing.
        if (kind == "sign")
        {
            if (done_)
                return note_ = Word("prompt", "signDone"), noteLeft_ = 2.0F, true;
            if (open_)
                cafe::CloseShop(shift_), Finish(), sounds_.push_back("turn");
            else
                shift_ = cafe::OpenShop(def_, seed_ + static_cast<std::uint64_t>(career_.day)), open_ = true, sounds_.push_back("turn");
            return true;
        }
        if (kind == "station")
        {
            int station = 0;
            while (station < static_cast<int>(def_.stations.size()) && def_.stations[U(station)].id != spot["station"].get<std::string>())
                ++station;
            if (!open_)
                return note_ = Word("prompt", "stationClosed"), noteLeft_ = 2.0F, true;
            const bool added = cafe::UseStation(def_, shift_, station);
            sounds_.push_back(added ? "pickup" : "deny");
            return true;
        }
        if (kind == "trash")
            return cafe::Trash(shift_), sounds_.push_back("pickup"), true;
        if (kind == "seat")
        {
            const int seat = spot["seat"];
            const int guest = U(seat) < shift_.seats.size() ? shift_.seats[U(seat)].guest : -1;
            if (!open_ || guest < 0 || !cafe::Serve(def_, shift_, seat))
                return false;
            const cafe::CafeLog &e = shift_.log.back();
            if (e.kind == cafe::CafeEvent::Served)
                note_ = Fill(Word("notes", "served"), {{"n", std::to_string(e.money)}}), sounds_.push_back("essence"), Talk(guest, seat);
            else
                note_ = Word("notes", "wrong"), sounds_.push_back("deny");
            noteLeft_ = 2.5F;
            return true;
        }
        if (kind == "board")
            return board_ = true, true;
        if (kind == "door")
        {
            if (open_)
                return note_ = Word("prompt", "doorOpen"), noteLeft_ = 2.0F, true;
            return leaving_ = true, true;
        }
        return false;
    }

    std::string CafePlay::Screen() const
    {
        if (done_ && paid_ && !seen_)
            return "takings";
        return board_ ? "board" : "";
    }

    bool CafePlay::HandleUi(const std::string &screen, const std::string &action, const std::string &arg)
    {
        if (screen == "takings" && action == "ok")
            return seen_ = true, true;
        if (screen == "board" && action == "walk" && match::MatchDef::Find(match_.characters, arg) >= 0)
            return walker_ = arg, true;
        if (screen == "board" && action == "close")
            return board_ = false, true;
        return false;
    }

    nlohmann::json CafePlay::UiData() const
    {
        nlohmann::json j;
        j["money"] = career_.money + (paid_ ? 0 : shift_.takings + shift_.tips);
        j["day"] = career_.day;
        j["open"] = open_;
        // 1. The clock, the cup, what E would do, and what to do next.
        const int left = static_cast<int>(std::ceil(std::max(0.0F, def_.rules.shiftSeconds - shift_.clock)));
        j["clock"] = !open_ ? "" : left > 0 ? Fill(Word("", "clock"), {{"m", std::to_string(left / 60)}, {"s", (left % 60 < 10 ? "0" : "") + std::to_string(left % 60)}})
                                            : Word("", "closing");
        nlohmann::json cup = nlohmann::json::array();
        for (int step : shift_.cup)
            cup.push_back({{"id", def_.stations[U(step)].id}, {"name", def_.stations[U(step)].name}, {"icon", "art/ui/cafe/" + def_.stations[U(step)].id + ".png"}});
        j["cup"] = cup;
        j["cupEmpty"] = cup.empty() ? Word("", "cupEmpty") : "";
        if (const int n = cafe::MatchingRecipe(def_, shift_.cup); n >= 0)
            j["cupMakes"] = def_.recipes[U(n)].name;
        std::string prompt;
        if (const int near = Near(); near >= 0)
        {
            const nlohmann::json &spot = room_["spots"][U(near)];
            const std::string kind = spot["kind"];
            if (kind == "sign")
                prompt = Word("prompt", done_ ? "signDone" : open_ ? "signClose" : "signOpen");
            else if (kind == "station")
                prompt = Fill(Word("prompt", "station"), {{"name", spot.value("label", std::string{})}});
            else if (kind == "seat" && U(spot["seat"].get<int>()) < shift_.seats.size() && shift_.seats[U(spot["seat"].get<int>())].guest >= 0)
                prompt = Fill(Word("prompt", "seat"), {{"drink", def_.recipes[U(shift_.seats[U(spot["seat"].get<int>())].recipe)].name}});
            else if (kind != "seat")
                prompt = Word("prompt", kind);
        }
        j["prompt"] = prompt;
        j["note"] = noteLeft_ > 0.0F ? note_ : "";
        j["hint"] = Word("hint", open_ ? "open" : done_ ? "done" : "closed");
        // 2. The orders at the tables, the talk's line, the takings, the company.
        nlohmann::json orders = nlohmann::json::array();
        for (int seat = 0; seat < static_cast<int>(shift_.seats.size()); ++seat)
            if (const cafe::Seat &st = shift_.seats[U(seat)]; st.guest >= 0)
                orders.push_back({{"seat", seat}, {"guest", def_.guests[U(st.guest)].name}, {"sprite", def_.guests[U(st.guest)].sprite}, {"drink", def_.recipes[U(st.recipe)].name},
                                  {"patience", st.patience}, {"maxPatience", def_.guests[U(st.guest)].patience}});
        j["orders"] = orders;
        nlohmann::json bubbles = nlohmann::json::array();
        if (!lines_.empty())
            bubbles.push_back({{"seat", lines_.front().seat}, {"text", lines_.front().text}});
        j["bubbles"] = bubbles;
        if (done_)
            j["takings"] = {{"title", Fill(Word("takings", "title"), {{"day", std::to_string(career_.day)}})},
                            {"served", Fill(Word("takings", "served"), {{"n", std::to_string(shift_.served)}})},
                            {"lost", Fill(Word("takings", "lost"), {{"n", std::to_string(shift_.lost)}})},
                            {"money", Fill(Word("takings", "money"), {{"takings", std::to_string(shift_.takings)}, {"tips", std::to_string(shift_.tips)}})},
                            {"total", Fill(Word("takings", "total"), {{"n", std::to_string(shift_.takings + shift_.tips)}})}};
        nlohmann::json board = nlohmann::json::array();
        for (const match::CharacterDef &c : match_.characters)
        {
            const cafe::CharacterProgress *p = career_.Find(c.id);
            std::string abilities;
            if (p && !p->abilities.empty())
                for (const std::string &a : p->abilities)
                    abilities += (abilities.empty() ? "" : ", ") + match_.abilities[U(match::MatchDef::Find(match_.abilities, a))].name;
            else // the role's level 1 abilities
                for (const match::JobLevel &j : match_.jobs)
                    if (j.role == c.role && j.level == 1 && j.ability >= 0)
                        abilities += (abilities.empty() ? "" : ", ") + match_.abilities[U(j.ability)].name;
            board.push_back({{"id", c.id}, {"name", c.name}, {"portrait", "art/sprites/" + c.sprite + "/face_calm.png"},
                             {"level", Fill(Word("board", "level"), {{"n", std::to_string(p ? p->level : 1)}})}, {"abilities", abilities},
                             {"walking", c.id == walker_}, {"walk", Fill(Word("board", "walk"), {{"name", c.name.substr(c.name.rfind(' ') + 1)}})}});
        }
        j["board"] = board;
        j["boardTitle"] = Word("board", "title");
        return j;
    }
}
