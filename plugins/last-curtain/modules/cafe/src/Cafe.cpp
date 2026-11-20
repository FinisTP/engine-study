#include <Cafe.hpp>

#include <algorithm>
#include <cmath>

// A shift at the café (Cafe.md "A shift").
namespace umbra::cafe
{
    namespace
    {
        std::size_t U(int i) { return static_cast<std::size_t>(i); }

        // splitmix64, as the match's: a shift follows from its seed.
        std::uint64_t Next(std::uint64_t &s)
        {
            std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }
        float Unit01(std::uint64_t &s) { return static_cast<float>(Next(s) >> 40) / static_cast<float>(1ULL << 24); }

        // A guest by weight, and one of the drinks it likes.
        void SeatGuest(const CafeDef &def, Shift &s, int seat)
        {
            int total = 0;
            for (const Guest &g : def.guests)
                total += g.weight;
            int pick = static_cast<int>(Next(s.rng) % static_cast<std::uint64_t>(std::max(total, 1))), guest = 0;
            while (guest + 1 < static_cast<int>(def.guests.size()) && pick >= def.guests[U(guest)].weight)
                pick -= def.guests[U(guest++)].weight;
            const Guest &g = def.guests[U(guest)];
            const int recipe = g.likes[U(static_cast<int>(Next(s.rng) % g.likes.size()))];
            s.seats[U(seat)] = {guest, recipe, g.patience};
            s.log.push_back({CafeEvent::Arrived, seat, guest, recipe, 0});
        }

        void Leave(Shift &s, int seat, CafeEvent why, int money)
        {
            Seat &st = s.seats[U(seat)];
            s.log.push_back({why, seat, st.guest, st.recipe, money});
            st = {};
        }
    }

    Shift OpenShop(const CafeDef &def, std::uint64_t seed)
    {
        Shift s;
        s.rng = seed;
        s.seats.assign(U(def.rules.seats), {});
        s.nextArrival = def.rules.firstArrival;
        return s;
    }

    void Tick(const CafeDef &def, Shift &s, float dt)
    {
        if (s.over || dt <= 0.0F)
            return;
        s.clock += dt;
        // 1. Patience runs down; at nothing the guest leaves unserved.
        for (int seat = 0; seat < static_cast<int>(s.seats.size()); ++seat)
            if (Seat &st = s.seats[U(seat)]; st.guest >= 0 && (st.patience -= dt) <= 0.0F)
                ++s.lost, Leave(s, seat, CafeEvent::Left, 0);
        // 2. Before closing time someone comes now and then, if a seat is free.
        if (s.clock < def.rules.shiftSeconds && s.clock >= s.nextArrival)
        {
            const auto free = std::ranges::find_if(s.seats, [](const Seat &st) { return st.guest < 0; });
            if (free != s.seats.end())
                SeatGuest(def, s, static_cast<int>(free - s.seats.begin()));
            s.nextArrival = s.clock + def.rules.arrivalMin + (def.rules.arrivalMax - def.rules.arrivalMin) * Unit01(s.rng);
        }
        // 3. After closing time, the last guest gone ends the shift.
        if (s.clock >= def.rules.shiftSeconds && std::ranges::all_of(s.seats, [](const Seat &st) { return st.guest < 0; }))
            s.over = true, s.cup.clear(), s.log.push_back({CafeEvent::Closed, -1, -1, -1, s.takings + s.tips});
    }

    bool UseStation(const CafeDef &def, Shift &s, int station)
    {
        if (s.over || station < 0 || station >= static_cast<int>(def.stations.size()) || static_cast<int>(s.cup.size()) >= def.rules.maxSteps)
            return false;
        s.cup.push_back(station);
        return true;
    }

    void Trash(Shift &s) { s.cup.clear(); }

    int MatchingRecipe(const CafeDef &def, const std::vector<int> &cup)
    {
        for (int r = 0; r < static_cast<int>(def.recipes.size()); ++r)
            if (def.recipes[U(r)].steps == cup)
                return r;
        return -1;
    }

    bool Serve(const CafeDef &def, Shift &s, int seat)
    {
        if (s.over || seat < 0 || seat >= static_cast<int>(s.seats.size()) || s.seats[U(seat)].guest < 0 || s.cup.empty())
            return false;
        const Seat st = s.seats[U(seat)];
        // 1. The right drink pays its price and a tip by the patience left; a wrong one sends the guest away.
        if (def.recipes[U(st.recipe)].steps == s.cup)
        {
            const float left = st.patience / def.guests[U(st.guest)].patience;
            const int price = def.recipes[U(st.recipe)].price, tip = static_cast<int>(std::ceil(left * static_cast<float>(def.rules.tipMax)));
            s.takings += price, s.tips += tip, ++s.served;
            Leave(s, seat, CafeEvent::Served, price + tip);
        }
        else
            ++s.lost, Leave(s, seat, CafeEvent::Wrong, 0);
        s.cup.clear();
        return true;
    }

    void CloseShop(Shift &s)
    {
        for (int seat = 0; seat < static_cast<int>(s.seats.size()); ++seat)
            if (s.seats[U(seat)].guest >= 0)
                Leave(s, seat, CafeEvent::Left, 0);
        if (!s.over)
            s.over = true, s.cup.clear(), s.log.push_back({CafeEvent::Closed, -1, -1, -1, s.takings + s.tips});
    }
}
