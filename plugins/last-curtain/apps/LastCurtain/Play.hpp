#pragma once

#include <Night.hpp>

#include <Input.hpp>

#include <nlohmann/json.hpp>

#include <string>

// One night as the person plays it, without a window (tested): clicks, keys and screen actions in,
// screen data out. The player's guest and their allies wait for input; bots play in Update.
class Play final
{
public:
    Play(const umbra::night::NightDef &def, std::uint64_t seed, int player); // player -1: all bots
    Play(const umbra::night::NightDef &def, const umbra::night::NightState &state);

    [[nodiscard]] bool PlayerTurn() const; // the acting guest is the player's or an ally's
    void Click(int cell);                  // moves there if it is a step cell
    void Key(umbra::platform::InputKey key); // arrows move, Enter ends the turn, I opens the bag
    // An action from a screen; true when it did something.
    bool HandleUi(const std::string &screen, const std::string &action, const std::string &arg);
    [[nodiscard]] nlohmann::json UiData() const;
    [[nodiscard]] std::string Screen() const; // "", "actions" or "bag"
    void Update(float dt);                    // a bot's turn: one action
    [[nodiscard]] const umbra::night::NightState &State() const { return state_; }

private:
    bool Do(const umbra::night::Action &action); // applies a legal action on the player's turn
    [[nodiscard]] int Me() const;                // the guest the screens show
    [[nodiscard]] std::string Label(const umbra::night::Action &action) const;

    const umbra::night::NightDef &def_;
    umbra::night::NightState state_;
    bool bag_ = false;
};
