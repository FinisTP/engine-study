#pragma once

#include <Diagnostic.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// The café between matches (Cafe.md): a timed shift of ghosts ordering drinks made at stations.
namespace umbra::cafe
{
    struct Station
    {
        std::string id, name; // a step a drink can take: phin, milk, ice, tea
    };

    struct Recipe
    {
        std::string id, name;
        std::vector<int> steps; // stations, in order
        int price = 0;
    };

    struct Guest
    {
        std::string id, name, sprite; // a ghost kind (its monsters.csv id)
        std::vector<int> likes;      // recipes it orders
        float patience = 30.0F;      // seconds it waits once seated
        int weight = 1;              // how often it comes
    };

    struct Rules
    {
        float shiftSeconds = 150.0F, firstArrival = 3.0F, arrivalMin = 6.0F, arrivalMax = 12.0F;
        int seats = 4, tipMax = 3, maxSteps = 3;
        int moneyPerEssence = 1; // a match's performed essence becomes this much money
    };

    struct CafeDef
    {
        std::vector<Station> stations;
        std::vector<Recipe> recipes;
        std::vector<Guest> guests;
        Rules rules;
        nlohmann::json talk; // cafe-talk.json: lines by ghost and character
    };

    // Reads stations.csv, recipes.csv, guests.csv, cafe.json and talk.json; problems name file, row and column.
    bool LoadCafe(const std::filesystem::path &folder, CafeDef &def, std::vector<core::Diagnostic> &diagnostics);

    struct Seat
    {
        int guest = -1, recipe = -1; // -1: empty
        float patience = 0.0F;       // seconds left
    };

    enum class CafeEvent : std::uint8_t { Arrived, Served, Wrong, Left, Closed };

    struct CafeLog
    {
        CafeEvent kind = CafeEvent::Arrived;
        int seat = -1, guest = -1, recipe = -1, money = 0;
    };

    // One shift. Only these functions change it.
    struct Shift
    {
        bool over = false;
        float clock = 0.0F, nextArrival = 0.0F;
        std::uint64_t rng = 0;
        std::vector<Seat> seats;
        std::vector<int> cup; // the steps in the drink being made
        int takings = 0, tips = 0, served = 0, lost = 0;
        std::vector<CafeLog> log;
    };

    Shift OpenShop(const CafeDef &def, std::uint64_t seed);
    void Tick(const CafeDef &def, Shift &s, float dt);           // arrivals, patience, closing time
    bool UseStation(const CafeDef &def, Shift &s, int station);  // the station's step into the cup; false when full or over
    void Trash(Shift &s);                                         // empties the cup
    bool Serve(const CafeDef &def, Shift &s, int seat);           // the cup to a seated guest: paid and tipped if right
    void CloseShop(Shift &s);                                     // now: the seated leave unserved, nothing lost
    int MatchingRecipe(const CafeDef &def, const std::vector<int> &cup); // the recipe the cup makes, -1 none
}
