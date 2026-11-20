#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>
#include <deque>

// The mirrored lane board (Board.md).
namespace umbra::match
{
    using namespace inner;

    namespace
    {
        constexpr int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0}; // N E S W

        int At(const Board &b, int x, int y) { return x < 0 || y < 0 || x >= b.width || y >= b.height ? -1 : y * b.width + x; }

        // Opens a door between two side-by-side cells.
        void Join(Board &b, int c, int dir)
        {
            const int n = At(b, c % b.width + dx[dir], c / b.width + dy[dir]);
            if (n < 0)
                return;
            b.cells[U(c)].doors |= static_cast<std::uint8_t>(1 << dir);
            b.cells[U(n)].doors |= static_cast<std::uint8_t>(1 << ((dir + 2) % 4));
        }

        void Set(Board &b, int x, int y, CellKind kind) { b.cells[U(At(b, x, y))].kind = kind; }

        // Steps from one cell to every cell (-1 unreachable).
        std::vector<int> Distances(const Board &b, int from)
        {
            std::vector<int> d(b.cells.size(), -1);
            std::deque<int> open{from};
            d[U(from)] = 0;
            while (!open.empty())
            {
                const int c = open.front();
                open.pop_front();
                for (int dir = 0; dir < 4; ++dir)
                    if (const int n = Neighbour(b, c, dir); n >= 0 && d[U(n)] < 0)
                        d[U(n)] = d[U(c)] + 1, open.push_back(n);
            }
            return d;
        }

        // Up to `count` cells from `pool` (shuffled), each passing `ok`.
        template <class F> std::vector<std::int16_t> Take(Rng &rng, std::vector<int> pool, int count, F ok)
        {
            for (std::size_t i = pool.size(); i > 1; --i)
                std::swap(pool[i - 1], pool[U(rng.Below(static_cast<int>(i)))]);
            std::vector<std::int16_t> out;
            for (int c : pool)
                if (static_cast<int>(out.size()) < count && ok(c, out))
                    out.push_back(static_cast<std::int16_t>(c));
            return out;
        }

        // One try: the bottom half from the seed, then mirrored. False if a count cannot be met.
        bool Try(const MatchDef &def, std::uint64_t seed, Board &b)
        {
            const auto &r = def.rules.board;
            Rng rng{seed};
            const int W = r.width, H = r.height, mid = W / 2, ym = H / 2;
            b = Board{};
            b.width = W;
            b.height = H;
            b.cells.assign(U(W * H), Cell{});

            // 1. The hub, its door square, and the cross hall that joins the lanes.
            std::vector<int> lanes = r.lanes == 1 ? std::vector<int>{mid} : r.lanes == 2 ? std::vector<int>{1, W - 2} : std::vector<int>{1, mid, W - 2};
            Set(b, mid, H - 1, CellKind::Hub);
            for (int x = lanes.front(); x <= lanes.back(); ++x)
            {
                Set(b, x, H - 2, CellKind::Hall);
                if (x > lanes.front())
                    Join(b, At(b, x, H - 2), 3);
            }
            Join(b, At(b, mid, H - 1), 0);
            // 2. A hall each side of the hub, so the guardian on the door square never walls its own side in.
            for (int x : {mid - 1, mid + 1})
            {
                Set(b, x, H - 1, CellKind::Hall);
                Join(b, At(b, x, H - 1), 0);
                Join(b, At(b, mid, H - 1), x < mid ? 3 : 1);
            }

            // 3. The lanes, from the cross hall up to the centre.
            for (int x : lanes)
                for (int y = H - 3; y >= ym + 2; --y)
                {
                    Set(b, x, y, CellKind::Hall);
                    Join(b, At(b, x, y), 2);
                    if (y == ym + 2)
                        Join(b, At(b, x, y), 0);
                }

            // 4. The centre: open rooms joined on every side.
            for (int y = ym - 1; y <= ym + 1; ++y)
                for (int x = 1; x <= W - 2; ++x)
                {
                    Set(b, x, y, CellKind::Centre);
                    if (x > 1)
                        Join(b, At(b, x, y), 3);
                    if (y > ym - 1)
                        Join(b, At(b, x, y), 0);
                }

            // 5. Now and then a cross link between two neighbouring lanes, for other routes.
            for (std::size_t l = 0; l + 1 < lanes.size(); ++l)
                if (ym + 3 <= H - 4 && rng.Below(2) == 0)
                {
                    const int y = ym + 3 + rng.Below(H - 4 - (ym + 3) + 1);
                    for (int x = lanes[l] + 1; x <= lanes[l + 1]; ++x)
                    {
                        Set(b, x, y, b.cells[U(At(b, x, y))].kind == CellKind::Wall ? CellKind::Hall : b.cells[U(At(b, x, y))].kind);
                        Join(b, At(b, x, y), 3);
                    }
                }

            // 6. Rooms hung off the halls, each with one door, about seven in ten of the places.
            std::vector<int> rooms;
            for (int y = ym + 2; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const int c = At(b, x, y);
                    if (b.cells[U(c)].kind != CellKind::Wall || rng.Below(10) >= 7)
                        continue;
                    std::vector<int> halls;
                    for (int dir = 0; dir < 4; ++dir)
                        if (const int n = At(b, x + dx[dir], y + dy[dir]); n >= 0 && b.cells[U(n)].kind == CellKind::Hall && n / W > ym + 1)
                            halls.push_back(dir);
                    if (halls.empty())
                        continue;
                    b.cells[U(c)].kind = CellKind::Room;
                    Join(b, c, halls[U(rng.Below(static_cast<int>(halls.size())))]);
                    rooms.push_back(c);
                }

            // 7. Dens, events and items by their steps from the hub.
            b.hub[0] = static_cast<std::int16_t>(At(b, mid, H - 1));
            b.door[0] = static_cast<std::int16_t>(At(b, mid, H - 2));
            const std::vector<int> d = Distances(b, b.hub[0]);
            auto band = [&](int lo, int hi) { return [&d, lo, hi](int c, const auto &) { return d[U(c)] >= lo && d[U(c)] <= hi; }; };
            b.weakDens = Take(rng, rooms, r.weakPerHalf, band(r.weakMin, r.weakMax));
            auto free = [&](int c) {
                return std::find(b.weakDens.begin(), b.weakDens.end(), c) == b.weakDens.end() &&
                       std::find(b.strongDens.begin(), b.strongDens.end(), c) == b.strongDens.end();
            };
            b.strongDens = Take(rng, rooms, r.strongPerHalf, [&](int c, const auto &out) { return band(r.strongMin, r.strongMax)(c, out) && free(c); });
            const std::vector<std::int16_t> events = Take(rng, rooms, r.eventsPerHalf, [&](int c, const auto &out) {
                return free(c) && std::all_of(out.begin(), out.end(), [&](int e) { return Steps(b, c, e) >= r.eventGap; });
            });
            if (static_cast<int>(b.weakDens.size()) < r.weakPerHalf || static_cast<int>(b.strongDens.size()) < r.strongPerHalf ||
                static_cast<int>(events.size()) < r.eventsPerHalf || (def.events.empty() && r.eventsPerHalf > 0))
                return false;
            for (int e : events)
                b.cells[U(e)].event = static_cast<std::int8_t>(rng.Below(static_cast<int>(def.events.size())));
            b.itemRooms = Take(rng, rooms, r.itemsPerHalf, [&](int c, const auto &) { return free(c) && b.cells[U(c)].event < 0; });
            if (static_cast<int>(b.itemRooms.size()) < r.itemsPerHalf)
                return false;

            // 8. Mirror the bottom onto the top, north and south doors swapped.
            for (int c = 0; c < W * ym; ++c)
            {
                if (c / W == ym)
                    continue;
                Cell m = b.cells[U(Mirror(b, c))];
                m.doors = static_cast<std::uint8_t>((m.doors & 0xA) | ((m.doors & 1) << 2) | ((m.doors & 4) >> 2));
                b.cells[U(c)] = m;
            }
            for (auto *list : {&b.weakDens, &b.strongDens, &b.itemRooms})
                for (std::size_t i = 0, n = list->size(); i < n; ++i)
                    list->push_back(static_cast<std::int16_t>(Mirror(b, (*list)[i])));
            b.hub[1] = static_cast<std::int16_t>(Mirror(b, b.hub[0]));
            b.door[1] = static_cast<std::int16_t>(Mirror(b, b.door[0]));

            // 9. Stage rooms: the middle row's open rooms, as far from one hub as from the other.
            for (int x = 0; x < W; ++x)
                if (const int c = At(b, x, ym); b.cells[U(c)].kind == CellKind::Centre && b.cells[U(c)].doors == 0xF)
                    b.stageRooms.push_back(static_cast<std::int16_t>(c));
            // 10. Centre spawn squares: on the middle row, spawn.centre steps either side of the stage.
            for (int x : {mid - def.rules.spawn.centre, mid + def.rules.spawn.centre})
                if (const int c = At(b, x, ym); x != mid && c >= 0 && b.cells[U(c)].kind == CellKind::Centre)
                    b.centreSpawns.push_back(static_cast<std::int16_t>(c));
            return b.stageRooms.size() >= 2;
        }

        // The plan's squares become tiles: square (x, y) is tile (2x, 2y); between two squares a tile that is open
        // when a door joins them, a wall otherwise; a corner tile is open only inside a block of four joined squares.
        Board Expand(const Board &plan)
        {
            Board b;
            b.width = plan.width * 2 - 1;
            b.height = plan.height * 2 - 1;
            b.cells.assign(U(b.width * b.height), Cell{});
            const auto tile = [&](int c) { return static_cast<std::int16_t>((c / plan.width) * 2 * b.width + (c % plan.width) * 2); };
            const auto open = [&](int c, int dir) { return Neighbour(plan, c, dir) >= 0; };
            for (int c = 0; c < static_cast<int>(plan.cells.size()); ++c)
            {
                const Cell &p = plan.cells[U(c)];
                const int t = tile(c), x = c % plan.width, y = c / plan.width;
                b.cells[U(t)].kind = p.kind;
                b.cells[U(t)].event = p.event;
                // 1. East and south of each square: a doorway of the shared kind (a hall between two kinds), or a wall.
                for (int dir : {1, 2})
                    if (open(c, dir))
                    {
                        const CellKind other = plan.cells[U(Neighbour(plan, c, dir))].kind;
                        b.cells[U(t + (dir == 1 ? 1 : b.width))].kind = other == p.kind ? p.kind : CellKind::Hall;
                    }
                // 2. The corner to the south-east: open when the four squares around it are joined all round.
                if (x + 1 < plan.width && y + 1 < plan.height && open(c, 1) && open(c, 2))
                {
                    const int e = c + 1, s = c + plan.width;
                    if (open(e, 2) && open(s, 1) && plan.cells[U(e)].kind == p.kind && plan.cells[U(s)].kind == p.kind &&
                        plan.cells[U(s + 1)].kind == p.kind)
                        b.cells[U(t + b.width + 1)].kind = p.kind;
                }
            }
            // 3. Every two open tiles side by side are joined: the walls are tiles now, not missing doors.
            for (int t = 0; t < static_cast<int>(b.cells.size()); ++t)
                for (int dir : {1, 2})
                    if (const int n = At(b, t % b.width + dx[dir], t / b.width + dy[dir]);
                        n >= 0 && b.cells[U(t)].kind != CellKind::Wall && b.cells[U(n)].kind != CellKind::Wall)
                        Join(b, t, dir);
            // 4. The plan's places, as tiles.
            for (int side : {0, 1})
                b.hub[side] = tile(plan.hub[side]), b.door[side] = tile(plan.door[side]);
            for (auto [from, to] : {std::pair{&plan.weakDens, &b.weakDens}, {&plan.strongDens, &b.strongDens}, {&plan.itemRooms, &b.itemRooms},
                                    {&plan.stageRooms, &b.stageRooms}, {&plan.centreSpawns, &b.centreSpawns}})
                for (int c : *from)
                    to->push_back(tile(c));
            return b;
        }
    }

    std::vector<int> Squares(const Board &b, const Unit &u)
    {
        std::vector<int> out;
        if (u.cell < 0 || u.hp <= 0)
            return out;
        const int x0 = u.cell % b.width, y0 = u.cell / b.width;
        for (int y = y0; y < y0 + u.size && y < b.height; ++y)
            for (int x = x0; x < x0 + u.size && x < b.width; ++x)
                out.push_back(y * b.width + x);
        return out;
    }

    bool Covers(const Board &b, const Unit &u, int cell)
    {
        if (u.cell < 0 || u.hp <= 0 || cell < 0)
            return false;
        const int dx = cell % b.width - u.cell % b.width, dy = cell / b.width - u.cell / b.width;
        return dx >= 0 && dy >= 0 && dx < u.size && dy < u.size;
    }

    int Mirror(const Board &b, int cell) { return (b.height - 1 - cell / b.width) * b.width + cell % b.width; }

    int Neighbour(const Board &b, int cell, int dir)
    {
        if (cell < 0 || !(b.cells[U(cell)].doors & (1 << dir)))
            return -1;
        return At(b, cell % b.width + dx[dir], cell / b.width + dy[dir]);
    }

    int Steps(const Board &b, int from, int to)
    {
        if (from < 0 || to < 0)
            return -1;
        return Distances(b, from)[U(to)];
    }

    bool Fair(const Board &b)
    {
        // 1. Every square matches its mirror: kind, doors (north and south swapped), event, distance.
        const std::vector<int> d0 = Distances(b, b.hub[0]), d1 = Distances(b, b.hub[1]);
        for (int c = 0; c < static_cast<int>(b.cells.size()); ++c)
        {
            const Cell &a = b.cells[U(c)], &m = b.cells[U(Mirror(b, c))];
            const int flipped = (m.doors & 0xA) | ((m.doors & 1) << 2) | ((m.doors & 4) >> 2);
            if (a.kind != m.kind || a.doors != flipped || a.event != m.event || d0[U(c)] != d1[U(Mirror(b, c))])
                return false;
            if (a.kind != CellKind::Wall && d0[U(c)] < 0)
                return false; // every square can be reached
        }
        // 2. Every den and item room has its mirror in the same list.
        for (const auto *list : {&b.weakDens, &b.strongDens, &b.itemRooms, &b.centreSpawns})
            for (int c : *list)
                if (std::find(list->begin(), list->end(), Mirror(b, c)) == list->end())
                    return false;
        return b.stageRooms.size() >= 2;
    }

    Board BuildBoard(const MatchDef &def, std::uint64_t seed)
    {
        // 1. Up to 64 tries from the seed; the last one is returned even if unfair (Fair says so).
        Board plan, b;
        for (std::uint64_t t = 0; t < 64; ++t)
            if (Try(def, seed * 0x100000001B3ULL + t, plan) && Fair(b = Expand(plan)))
                return b;
        return b;
    }
}
