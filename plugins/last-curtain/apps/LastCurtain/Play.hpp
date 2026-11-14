#pragma once

#include <Narrate.hpp>
#include <Night.hpp>

#include <Input.hpp>

#include <nlohmann/json.hpp>

#include <deque>
#include <vector>
#include <string>

// One night as the person plays it, without a window (tested): clicks, keys and screen actions in,
// screen data out. The player's guest and their allies wait for input; bots play in Update.
class Play final
{
public:
    Play(const umbra::night::NightDef &def, std::uint64_t seed, int player); // player -1: all bots
    Play(const umbra::night::NightDef &def, const umbra::night::NightState &state);

    // The player's turn: the menu, picking a square (and walking to it), or Stay/Back at a door.
    enum class Mode { Menu, Picking, Confirm };
    // A pawn sliding from one cell to the next; t runs 0 to 1. guest -1: none.
    struct Slide
    {
        int guest = -1, from = -1, to = -1;
        float t = 0.0F;
    };
    // The walk to the square under the mouse while picking; steps past `affordable` show red.
    struct Preview
    {
        std::vector<int> path;
        int affordable = 0;
    };
    static constexpr float slideSeconds = 0.25F;

    [[nodiscard]] bool PlayerTurn() const; // the acting guest is the player's or an ally's
    [[nodiscard]] Mode TurnMode() const { return mode_; }
    // Left: walk to a lit square, one step at a time (on a bot's turn: inspect); right: inspect.
    void Click(int cell, bool right = false);
    void Hover(int cell) { hover_ = cell; }
    [[nodiscard]] Slide Sliding() const { return slide_; }
    [[nodiscard]] Preview Previewed() const;
    void Key(umbra::platform::InputKey key); // arrows step, Enter ends the turn, I opens the bag
    // An action from a screen; true when it did something.
    bool HandleUi(const std::string &screen, const std::string &action, const std::string &arg);
    [[nodiscard]] nlohmann::json UiData() const;
    // "", "end", "pause", "inspect", "caption", "event", "dice", "turn", "chat", "battle", "menu",
    // "picking", "confirm" or "bag"
    [[nodiscard]] std::string Screen() const;
    // Bots act one action at a time: a beat the player sees holds 1.5 s (0.3 s after Space), an
    // unseen one 0.2 s. dt 0 (a minimised window) changes nothing.
    void Update(float dt);
    void SetCaptions(umbra::night::Captions captions) { captions_ = std::move(captions); }
    [[nodiscard]] const umbra::night::Beat &LastBeat() const { return beat_; }
    [[nodiscard]] int Beats() const { return beats_; } // bot actions so far
    [[nodiscard]] const umbra::night::NightState &State() const { return state_; }

    // Saves (plan 1's SaveNight); Restore refuses a damaged save or one from other data.
    [[nodiscard]] nlohmann::json Save() const;
    static bool Restore(const umbra::night::NightDef &def, const nlohmann::json &saved, umbra::night::NightState &out);
    // The start screen's data: the guests to pick, and whether Continue shows.
    static nlohmann::json StartData(const umbra::night::NightDef &def, bool canContinue);

private:
    // A roll the player made, shown on the dice screen.
    struct Dice
    {
        int die = 0, statValue = 0, target = 0;
        std::string stat, result;
        bool passed = false;
    };

    bool Do(const umbra::night::Action &action); // applies a legal action on the player's turn
    bool Step(int to);                           // the legal Move into a neighbouring cell, with its slide
    void WalkOn();                               // the next step of the walk, on the player's turn
    [[nodiscard]] int Me() const;                // the guest the screens show
    [[nodiscard]] std::string Label(const umbra::night::Action &action) const;
    [[nodiscard]] nlohmann::json BattleData() const;
    [[nodiscard]] nlohmann::json TurnData() const;
    [[nodiscard]] nlohmann::json EndData() const;
    [[nodiscard]] nlohmann::json InspectData() const;
    void Notice(std::uint32_t since); // the Turn in new log entries

    const umbra::night::NightDef &def_;
    umbra::night::NightState state_;
    bool bag_ = false;
    umbra::night::Captions captions_;
    umbra::night::Beat beat_;
    float hold_ = 0.0F; // seconds the last beat still shows
    bool fast_ = false; // Space: short beats until the player's next turn
    int beats_ = 0;
    int event_ = -1, eventCell_ = -1, eventStat_ = 0; // the event to show before its dice (events.csv row)
    std::deque<Dice> dice_;
    bool turn_ = false; // the Turn happened and its screen waits
    Mode mode_ = Mode::Menu;
    std::vector<int> walk_; // cells still to walk, in order
    bool walking_ = false;  // a walk is under way (the menu returns when it ends)
    int hover_ = -1;
    bool pause_ = false; // the save menu; the night waits
    int inspect_ = -1;   // the cell being inspected; the night waits
    Slide slide_;
};
