#pragma once

#include <Cafe.hpp>
#include <Career.hpp>

#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace umbra::match { struct MatchDef; }

// The café hub on screen, headless (CafePlay.md): one character walks the room, E does what the
// nearest spot offers, the shop opens for a shift, talks are told, the day's takings are kept.
namespace umbra::night
{
    class CafePlay final
    {
    public:
        CafePlay(const cafe::CafeDef &def, const match::MatchDef &match, nlohmann::json room, nlohmann::json words, cafe::Career &career,
                 std::string walker, std::uint64_t seed);

        void Move(glm::vec2 direction, float dt); // a step along the floor, stopped by blocks and walls
        void WalkTo(glm::vec2 at);                 // a click: walk there on its own
        void Update(float dt);                     // the walk, the shift's clock, the talk's lines
        bool Interact();                           // E: the nearest spot's action
        bool OpenShop();                           // the sign's opening (once a day)
        [[nodiscard]] int Near() const;            // the nearest spot in reach, -1 none
        [[nodiscard]] glm::vec2 Position() const { return at_; }
        [[nodiscard]] const cafe::Shift &Shift() const { return shift_; }
        [[nodiscard]] bool Open() const { return open_; }
        [[nodiscard]] const std::string &Walker() const { return walker_; }
        [[nodiscard]] std::string Screen() const;  // "", "board" (the characters), "takings" (the day's end)
        bool HandleUi(const std::string &screen, const std::string &action, const std::string &arg);
        [[nodiscard]] nlohmann::json UiData() const;
        [[nodiscard]] bool Leaving() const { return leaving_; } // the stage door: on to the next match
        [[nodiscard]] const nlohmann::json &Room() const { return room_; }
        std::vector<std::string> TakeSounds() { return std::exchange(sounds_, {}); }

    private:
        struct Line
        {
            int seat = -1; // -1: the walker says it
            std::string text;
        };
        [[nodiscard]] bool Blocked(glm::vec2 p) const;
        [[nodiscard]] std::string Word(const std::string &group, const std::string &key) const;
        void Talk(int guest, int seat);
        void Finish();

        const cafe::CafeDef &def_;
        const match::MatchDef &match_;
        nlohmann::json room_, words_;
        cafe::Career &career_;
        std::string walker_;
        std::uint64_t seed_;
        cafe::Shift shift_;
        bool open_ = false, done_ = false, paid_ = false, seen_ = false, board_ = false, leaving_ = false;
        glm::vec2 at_{0.0F}, goal_{-1.0F};
        std::vector<Line> lines_; // the talk being told, first now
        float lineLeft_ = 0.0F;
        std::string note_;        // a short line under the prompt ("Closed for today.")
        float noteLeft_ = 0.0F;
        std::vector<std::string> sounds_;
    };
}
