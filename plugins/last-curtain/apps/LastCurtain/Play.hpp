#pragma once

#include <BoardView.hpp>
#include <Narrate.hpp>
#include <Night.hpp>
#include <Options.hpp>
#include <Words.hpp>

#include <Input.hpp>

#include <nlohmann/json.hpp>

#include <deque>
#include <functional>
#include <vector>
#include <optional>
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
    static constexpr float thinkSeconds = 0.4F; // the shortest "thinking..." (user)
    static constexpr float lineSeconds = 3.0F;  // the longest wait for the model's line (user)
    // How a bot decides; `seconds` is how long the decision took (it sets the thinking pause).
    using Decider = std::function<umbra::night::Action(const umbra::night::NightState &, int guest, float &seconds)>;

    [[nodiscard]] bool PlayerTurn() const; // the acting guest is the player's or an ally's
    // Whose turn the screen shows: the player's while they rest after a room, else the rules' (-1: over).
    [[nodiscard]] int Acting() const;
    // The banner that opens a turn: whose, and how far it has played (0 to 1); guest -1: none.
    struct Banner
    {
        int guest = -1;
        float t = 0.0F;
    };
    [[nodiscard]] Banner TurnBanner() const;
    // The night's scenes on the board (cutscenes.json; Play.md "Cutscenes"): the opening of a new
    // night, midnight, the ending. Shot: where the camera looks and from how far, turned how far.
    void SetCutscenes(nlohmann::json scenes);
    struct CameraShot
    {
        int cell = -1; // -1: no scene playing
        float distance = 0.0F, yaw = 0.0F, tilt = 50.0F;
        int index = -1;       // the shot's number in its scene (a new one starts its clip)
        std::string clip;     // what the shot's figure plays ("" none)
        int clipGuest = -1;
    };
    [[nodiscard]] CameraShot Shot() const;
    // Sounds to play since the last call, by cue (sounds.csv): step, dice, door, item, hit, death, bell, turn.
    [[nodiscard]] std::vector<std::string> TakeCues() { return std::exchange(cues_, {}); }
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
    void SetDecider(Decider decider) { decider_ = std::move(decider); }
    // The pace (options.csv); the defaults are plan 2c's.
    void SetOptions(const umbra::night::GameOptions &options) { options_ = options; }
    [[nodiscard]] bool WaitingForTap() const { return waitTap_; }
    // The model's lines (plan 3); null or no model: the written lines.
    void SetWords(umbra::night::Words *words) { words_ = words; }
    // The room at full size (plan 4b/4c): Look on the player's turn, or a battle in the player's light.
    struct Actor
    {
        int guest = -1, monsterKind = -1, side = 0; // side 1: the monsters or the other side of a battle
        bool taken = false;
    };
    // The room view (the user's switch, "look"; Table.md "Room view"), or a moment that shows its room.
    [[nodiscard]] bool RoomView() const { return roomView_ || StageCell() >= 0; }
    [[nodiscard]] std::vector<int> GoalCells() const; // opened rooms the player's room goal names (they shine)
    [[nodiscard]] bool CanView() const { return !state_.over && state_.player >= 0 && (PlayerTurn() || resting_); }
    [[nodiscard]] int StageCell() const; // a moment's room (an event, a deed in sight, a battle in light); -1: none
    [[nodiscard]] std::vector<Actor> StageActors() const; // who stands in StageCell, a battle's sides apart
    // Clicks on things in the room view: a floor item (state.floor index), a guest, a monster (state.monsters index).
    bool ClickItem(int floor);
    bool ClickGuest(int guest);
    bool ClickMonster(int monster);
    // The camera: the cell to look at (-1: nothing), and whether it follows (a drag stops it until Focus).
    [[nodiscard]] int CameraCell() const;
    // (guest, number) for every guest in a group of two or more: the player's group is 1, the
    // others 2, 3 ... in order of their lowest member.
    [[nodiscard]] std::vector<std::pair<int, int>> Rings() const;
    void Drag() { following_ = false; }
    [[nodiscard]] bool Following() const { return following_; }
    [[nodiscard]] float Thinking() const { return think_; }      // seconds of "thinking..." left
    [[nodiscard]] int ThinkingGuest() const { return think_ > 0.0F || lineWaiting_ ? pendingGuest_ : -1; }
    [[nodiscard]] const umbra::night::Beat &LastBeat() const { return beat_; }
    [[nodiscard]] int Beats() const { return beats_; } // bot actions so far
    [[nodiscard]] const umbra::night::NightState &State() const { return state_; }
    [[nodiscard]] umbra::night::BoardView View() const; // what the player sees (all of it with the light off)

    // Saves (plan 1's SaveNight); Restore refuses a damaged save or one from other data.
    [[nodiscard]] nlohmann::json Save() const;
    static bool Restore(const umbra::night::NightDef &def, const nlohmann::json &saved, umbra::night::NightState &out);
    // The start screen's data: the guests to pick, and whether Continue shows.
    static nlohmann::json StartData(const umbra::night::NightDef &def, bool canContinue, int chosen = -1);

private:
    // A roll the player made, shown on the dice screen.
    struct Dice
    {
        int die = 0, statValue = 0, target = 0;
        std::string stat, result;
        bool passed = false;
        int hp = 0; // the HP it took or gave, for the HUD's flash
        std::string reply, replyBy; // a chat answer: the asker's reply (the model's words when ready)
        int replyTicket = 0;
        std::string who; // a bot's roll: its name (empty: the player's)
    };

    bool Handle(const std::string &screen, const std::string &action, const std::string &arg); // HandleUi's work
    float tapClock_ = 1.0e9F; // seconds since the last tap went on
    float banner_ = 0.0F;     // seconds the turn's banner still shows
    std::vector<std::string> cues_; // sounds waiting for Main
    nlohmann::json scenes_;            // cutscenes.json
    std::string scene_;                // the scene playing ("" none)
    std::vector<std::string> queued_;  // scenes waiting their turn
    int shot_ = 0;
    float shotT_ = 0.0F;
    bool opening_ = false; // a new night: its opening plays once scenes are given
    void StartScene(const std::string &kind);
    void NextScene();
    [[nodiscard]] std::string SceneLine() const;
    int bannerKey_ = -1;      // round * 8 + turn of the last banner
    bool Do(const umbra::night::Action &action); // applies a legal action on the player's turn
    bool Step(int to);                           // the legal Move into a neighbouring cell, with its slide
    void WalkOn();                               // the next step of the walk, on the player's turn
    [[nodiscard]] int Me() const;                // the guest the screens show
    [[nodiscard]] std::string Label(const umbra::night::Action &action) const;
    [[nodiscard]] nlohmann::json BattleData() const;
    [[nodiscard]] nlohmann::json TurnData() const;
    [[nodiscard]] nlohmann::json EndData() const;
    [[nodiscard]] nlohmann::json InspectData() const;
    void Notice(std::uint32_t since); // the Turn in new log entries, popups, the Taken's fall
    // The Taken's fall: where, the screen's title and cause, who dealt it (-1 the house).
    struct Falling
    {
        int cell = -1;
        std::string title, cause;
        int killer = -1, guest = -1; // who struck, who fell
    };
    [[nodiscard]] Falling Fall(const std::vector<umbra::night::LogEntry> &entries, const umbra::night::LogEntry &died) const;
    [[nodiscard]] bool SawFall(int cell) const;
    [[nodiscard]] bool KillerNear() const; // the player can hear the killer // the room is in the player's light
    Falling fall_;
    bool fallSeen_ = false;
    // The model's lines: a bot's coming beat (caption, goal line), the Turn, the chat question.
    void AskLines(int acting);
    void AskTurn();
    void ResolveLines();
    [[nodiscard]] bool LineWaiting(int ticket, float askedAt) const;
    [[nodiscard]] bool ChatWaiting() const;
    [[nodiscard]] int ChatKey() const;
    [[nodiscard]] std::uint32_t LineSeed(int kind) const;

    const umbra::night::NightDef &def_;
    umbra::night::NightState state_;
    bool bag_ = false;
    umbra::night::Captions captions_;
    umbra::night::Beat beat_;
    float hold_ = 0.0F; // seconds the last beat still shows
    bool fast_ = false; // Space: short beats until the player's next turn
    int beats_ = 0;
    int event_ = -1, eventCell_ = -1, eventStat_ = 0; // the event to show before its dice (events.csv row)
    // A room walked into, step by step (Play.md "The room's flow"): arrival, the card or the find, the dice.
    struct Arrival
    {
        int cell = -1;
        std::string kind, line;
    };
    Arrival arrive_;
    int found_ = -1;           // an item found in an item room (items.csv row)
    std::string lasting_;      // the card's note: a lasting event still armed
    std::string cleared_;      // the card's note: its condition met, no roll
    float diceClock_ = 0.0F;   // seconds the front die has shown; it tumbles for the first diceSeconds
    float flashClock_ = 0.0F;  // the HUD shows `flash_` (an HP change) while this runs
    std::string flash_;
    int beatEvent_ = -1; // the event a bot's seen beat faced, for the caption
    [[nodiscard]] int Affordable(int guest, const std::vector<int> &path) const;
    [[nodiscard]] int RoomEvent(const umbra::night::NightState &before, const umbra::night::NightState &after, const umbra::night::Action &action,
                                int guest, int &entered) const;
    struct Popup
    {
        int guest = -1;
        std::string text;
        float until = 0.0F; // clock_ when it goes
    };
    std::vector<Popup> popups_; // damage over struck guests (Main draws them as labels)
    bool preview_ = false;          // the caption on show is a deed's first step: what the bot faces
    std::string heldResult_;        // ... its result, shown after the tap
    std::optional<Dice> botDice_;   // ... and the bot's roll, shown between
    [[nodiscard]] float DiceSettle() const;
    [[nodiscard]] bool Presenting() const; // the last deed is still on screen
    [[nodiscard]] std::string RollResult(const umbra::night::EventDef &ev, bool passed, int guest) const;
    int shown_ = -1;              // the guest whose deed is on screen (Acting stays on them)
    std::deque<Dice> dice_;
    bool turn_ = false; // the Turn happened and its screen waits
    Mode mode_ = Mode::Menu;
    std::vector<int> walk_; // cells still to walk, in order
    bool walking_ = false;  // a walk is under way (the menu returns when it ends)
    int hover_ = -1;
    bool pause_ = false; // the save menu; the night waits
    bool roomView_ = false; // the player's choice of view; the night goes on in either
    umbra::night::GameOptions options_;
    bool waitTap_ = false;  // a bot's beat where something happened waits for a tap (options)
    bool moment_ = false;   // ... and is shown at full size (options)
    int rester_ = -1;       // ... who rests (the player or an ally)
    bool resting_ = false;  // the player resolved a room: the rules ended the turn, the screen holds (Look, End turn)
    bool skipping_ = false; // Skip: no taps, pauses or line waits until the player's turn
    [[nodiscard]] bool Notable(const std::vector<umbra::night::LogEntry> &entries, const umbra::night::Action &action) const;
    [[nodiscard]] bool NotableBeat(const std::vector<umbra::night::LogEntry> &entries, const umbra::night::Action &action) const;
    bool goalOpen_ = true; // the HUD's goal panel, folded or open (the user)
    int inspect_ = -1;   // the cell being inspected; the night waits
    Slide slide_;
    Decider decider_;
    bool pending_ = false; // a bot's decision waits for its thinking pause
    umbra::night::Action pendingAction_;
    int pendingGuest_ = -1;
    float think_ = 0.0F;
    int thoughtTurn_ = -1;
    bool following_ = true;
    int followTurn_ = -1; // round * 8 + turn when the camera last began following
    void Follow(); // round * 8 + turn of the last bot turn that thought
    umbra::night::Words *words_ = nullptr;
    float clock_ = 0.0F;                      // seconds of Update, for the line waits
    float askedAt_ = 0.0F, turnAskedAt_ = 0.0F, chatAskedAt_ = 0.0F;
    int captionTicket_ = 0, saidTicket_ = 0, turnTicket_ = 0, chatTicket_ = 0;
    int saidGuest_ = -1, chatFor_ = -1;       // who says the goal line; the chat question the line is for
    std::string said_, turnWords_, chatWords_; // worded lines ("" = none: the written ones show)
    bool lineWaiting_ = false;
};
