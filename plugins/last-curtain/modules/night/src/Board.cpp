#include "NightInner.hpp"

#include <algorithm>
#include <cstdlib>

namespace umbra::night
{
    using namespace inner;

    namespace
    {
        bool Has(std::uint8_t doors, int direction) { return (doors >> direction) & 1u; }
        int Opposite(int direction) { return (direction + 2) % 4; }
        std::uint8_t Rotate(std::uint8_t doors, int turns)
        {
            return static_cast<std::uint8_t>(((doors << turns) | (doors >> (4 - turns))) & 15u);
        }

        bool Sealed(const NightDef &def, const NightState &state)
        {
            if (state.placed >= def.rules.board.rooms)
                return false;
            for (int cell = 0; cell < maxCells; ++cell)
                for (int d = 0; d < 4; ++d)
                    if (Doorway(def, state, cell, d))
                        return false;
            return true;
        }

        void Place(NightState &state, int cell, int room, std::uint8_t doors)
        {
            state.cells[U(cell)] = {I8(room), doors, true};
            ++state.placed;
        }
    }

    void inner::BuildBoard(const NightDef &def, NightState &state)
    {
        // 1. The deck: every room but the foyer and the under-stage, `count` times, shuffled.
        state.deckSize = 0;
        int foyerRoom = 0;
        for (std::size_t r = 0; r < def.rooms.size(); ++r)
        {
            const RoomDef &room = def.rooms[r];
            if (room.kind == RoomKind::Foyer)
                foyerRoom = static_cast<int>(r);
            else if (room.kind != RoomKind::UnderStage)
                for (int n = 0; n < room.count && state.deckSize < 64; ++n)
                    state.deck[U(state.deckSize++)] = I8(r);
        }
        for (int i = state.deckSize - 1; i > 0; --i)
            std::swap(state.deck[U(i)], state.deck[U(Below(state, i + 1))]);

        // 2. The foyer, bottom centre, as drawn.
        state.foyer = I8(CellAt(def, def.rules.board.width / 2, def.rules.board.height - 1));
        Place(state, state.foyer, foyerRoom, def.rooms[U(foyerRoom)].doors);
    }

    int CellAt(const NightDef &def, int x, int y)
    {
        const auto &b = def.rules.board;
        return x < 0 || y < 0 || x >= b.width || y >= b.height ? -1 : y * b.width + x;
    }

    int Neighbour(const NightDef &def, int cell, int direction)
    {
        const int w = def.rules.board.width, x = cell % w, y = cell / w;
        static constexpr int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
        return CellAt(def, x + dx[direction], y + dy[direction]);
    }

    bool Passage(const NightDef &def, const NightState &state, int from, int direction)
    {
        const int to = Neighbour(def, from, direction);
        if (from < 0 || to < 0)
            return false;
        const Cell &a = state.cells[U(from)], &b = state.cells[U(to)];
        return a.opened && b.opened && Has(a.doors, direction) && Has(b.doors, Opposite(direction));
    }

    bool Doorway(const NightDef &def, const NightState &state, int from, int direction)
    {
        const int to = Neighbour(def, from, direction);
        return to >= 0 && state.placed < def.rules.board.rooms && state.cells[U(from)].opened &&
               Has(state.cells[U(from)].doors, direction) && !state.cells[U(to)].opened;
    }

    int Open(const NightDef &def, NightState &state, int from, int direction)
    {
        if (!Doorway(def, state, from, direction))
            return -1;
        const int target = Neighbour(def, from, direction), back = Opposite(direction);
        const bool last = state.placed + 1 == def.rules.board.rooms;

        // 1. The under-stage, once the target is 3 grid squares from the foyer (no later shortcut
        //    can bring it nearer), or as the last room.
        const int w = def.rules.board.width;
        const int far = std::abs(target % w - state.foyer % w) + std::abs(target / w - state.foyer / w);
        if (state.understage < 0 && (last || far >= 3))
        {
            const int room = static_cast<int>(std::find_if(def.rooms.begin(), def.rooms.end(),
                                                           [](const RoomDef &r) { return r.kind == RoomKind::UnderStage; }) - def.rooms.begin());
            for (int turns = 0; turns < 4; ++turns)
            {
                const std::uint8_t doors = Rotate(def.rooms[U(room)].doors, turns);
                if (!Has(doors, back))
                    continue;
                Place(state, target, room, doors);
                if (!Sealed(def, state) || turns == 3)
                    break;
                state.cells[U(target)] = {};
                --state.placed;
            }
            if (!state.cells[U(target)].opened) // no rotation faces back: open a door for it
                Place(state, target, room, static_cast<std::uint8_t>(def.rooms[U(room)].doors | (1u << back)));
            state.understage = I8(target);
        }
        else
        {
            // 2. The first deck entry and rotation that faces back and leaves the board open.
            for (int i = state.deckNext; i < state.deckSize && !state.cells[U(target)].opened; ++i)
                for (int turns = 0; turns < 4; ++turns)
                {
                    const int room = state.deck[U(i)];
                    const std::uint8_t doors = Rotate(def.rooms[U(room)].doors, turns);
                    if (!Has(doors, back))
                        continue;
                    Place(state, target, room, doors);
                    if (!Sealed(def, state))
                    {
                        // Skipped entries stay in the deck, in order.
                        std::rotate(state.deck.begin() + state.deckNext, state.deck.begin() + i, state.deck.begin() + i + 1);
                        ++state.deckNext;
                        break;
                    }
                    state.cells[U(target)] = {};
                    --state.placed;
                }
            // 3. Nothing fits: the next entry with four doors.
            if (!state.cells[U(target)].opened)
                Place(state, target, state.deck[U(state.deckNext++)], 15u);
        }

        // 4. Still sealed (rare): open a door from any opened cell to an unopened neighbour.
        for (int cell = 0; cell < maxCells && Sealed(def, state); ++cell)
            for (int d = 0; d < 4; ++d)
            {
                const int next = Neighbour(def, cell, d);
                if (state.cells[U(cell)].opened && next >= 0 && !state.cells[U(next)].opened)
                {
                    state.cells[U(cell)].doors = static_cast<std::uint8_t>(state.cells[U(cell)].doors | (1u << d));
                    break;
                }
            }
        return target;
    }

    int Steps(const NightDef &def, const NightState &state, int from, int to)
    {
        // Breadth-first through passages.
        if (from < 0 || to < 0)
            return -1;
        std::array<int, maxCells> dist;
        dist.fill(-1);
        std::array<int, maxCells> queue{};
        int head = 0, tail = 0;
        dist[U(from)] = 0;
        queue[U(tail++)] = from;
        while (head < tail)
        {
            const int cell = queue[U(head++)];
            if (cell == to)
                return dist[U(cell)];
            for (int d = 0; d < 4; ++d)
            {
                const int next = Neighbour(def, cell, d);
                if (Passage(def, state, cell, d) && dist[U(next)] < 0)
                    dist[U(next)] = dist[U(cell)] + 1, queue[U(tail++)] = next;
            }
        }
        return -1;
    }

    bool Lit(const NightDef &def, const NightState &state, int viewer, int cell, int steps)
    {
        const int n = Steps(def, state, viewer, cell);
        return n >= 0 && n <= steps;
    }

    bool InLine(const NightDef &def, const NightState &state, int from, int to)
    {
        const int w = def.rules.board.width;
        if (from == to || (from % w != to % w && from / w != to / w))
            return false;
        const int direction = from / w == to / w ? (to > from ? 1 : 3) : (to > from ? 2 : 0);
        for (int cell = from; cell != to; cell = Neighbour(def, cell, direction))
            if (!Passage(def, state, cell, direction))
                return false;
        return true;
    }
}
