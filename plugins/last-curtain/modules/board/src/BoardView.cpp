#include <BoardView.hpp>

#include <array>
#include <cmath>

namespace umbra::night
{
    namespace
    {
        std::size_t U(int i) { return static_cast<std::size_t>(i); }

        bool Up(const Guest &g) { return !g.down && !g.dead; }
    }

    BoardView MakeView(const NightDef &def, const NightState &state, int viewer, bool light)
    {
        BoardView view;
        const bool all = viewer < 0 || state.guests[U(viewer)].dead;
        const bool seeAll = all || !light; // the light switched off (options.csv): every room is lit

        // 1. Each cell: its shape when opened; lit when a guest the viewer controls lights it.
        for (int c = 0; c < maxCells; ++c)
        {
            const Cell &cell = state.cells[U(c)];
            CellView &v = view.cells[U(c)];
            if (!cell.opened)
                continue;
            v.opened = true;
            v.kind = def.rooms[U(cell.room)].kind;
            v.room = cell.room;
            v.doors = cell.doors;
            for (int d = 0; d < 4; ++d)
            {
                if (Doorway(def, state, c, d))
                    v.doorways = static_cast<std::uint8_t>(v.doorways | (1u << d));
                else if (!Passage(def, state, c, d))
                    v.walls = static_cast<std::uint8_t>(v.walls | (1u << d));
            }
            v.lit = seeAll;
            for (int g = 0; g < state.count && !v.lit; ++g)
                if (Controls(state, viewer, g) && Up(state.guests[U(g)]))
                    v.lit = Lit(def, state, state.guests[U(g)].cell, c, Stat(def, state, g, StatKind::Light));
            // Its marker: an armed event or trap, a room still holding one, a bonus the viewer has not had.
            if (v.lit && cell.event >= 0)
                v.mark = def.events[U(cell.event)].trap ? RoomMark::Trap : RoomMark::Event;
            else if (v.lit && !cell.resolved && v.kind == RoomKind::Trap)
                v.mark = RoomMark::Trap;
            else if (v.lit && !cell.resolved && v.kind == RoomKind::Event)
                v.mark = RoomMark::Event;
            else if (v.lit && !cell.resolved && v.kind == RoomKind::Item)
                v.mark = RoomMark::Item;
            else if (v.lit && v.kind == RoomKind::Bonus && viewer >= 0 && !state.guests[U(viewer)].bonusUsed[U(c)])
                v.mark = RoomMark::Bonus;
        }

        // 2. The viewer's steps: legal moves on a turn the viewer plays.
        const int acting = Current(state);
        if (!all && acting >= 0 && Controls(state, viewer, acting) && !state.battle.active)
        {
            const std::vector<Action> legal = LegalActions(def, state);
            for (std::size_t i = 0; i < legal.size(); ++i)
                if (legal[i].kind == ActionKind::Move && legal[i].a < 4)
                    view.cells[U(Neighbour(def, state.guests[U(acting)].cell, legal[i].a))].step = static_cast<std::int8_t>(i);
        }

        // 3. Pawns and monsters in lit cells only (the viewer's own pawn always). After midnight the Taken
        //    knows where everyone is, and everyone knows where the Taken is (the user, 2026-11-15).
        for (int g = 0; g < state.count; ++g)
        {
            const Guest &guest = state.guests[U(g)];
            const bool hunted = viewer >= 0 && (IsTaken(state, viewer) || IsTaken(state, g));
            if (!InPlay(state, g) || guest.cell < 0 || (g != viewer && !view.cells[U(guest.cell)].lit && !hunted))
                continue;
            view.pawns.push_back({g, guest.cell, g == viewer, g != viewer && Controls(state, viewer, g), IsTaken(state, g), guest.ghost});
        }
        for (std::size_t m = 0; m < state.monsters.size(); ++m)
        {
            const Monster &monster = state.monsters[m];
            if (monster.hp > 0 && view.cells[U(monster.cell)].lit)
                view.monsters.push_back({static_cast<int>(m), monster.cell, monster.kind, def.monsters[U(monster.kind)].taken >= 0});
        }
        // The killer, in a lit room.
        if (state.killer.cell >= 0 && view.cells[U(state.killer.cell)].lit)
            view.killer = state.killer.cell;
        // Items lying where the player can see.
        for (std::size_t i = 0; i < state.floor.size(); ++i)
            if (const FloorItem &f = state.floor[i]; f.cell >= 0 && f.item >= 0 && view.cells[U(f.cell)].lit)
                view.items.push_back({f.cell, f.item, static_cast<int>(i)});
        return view;
    }

    std::vector<int> PathTo(const NightDef &def, const NightState &state, int guest, int cell)
    {
        const Guest &me = state.guests[U(guest)];
        if (cell < 0 || cell >= maxCells || cell == me.cell)
            return {};
        const bool open = state.cells[U(cell)].opened;
        if (open && !Lit(def, state, me.cell, cell, Stat(def, state, guest, StatKind::Light)))
            return {};
        // 1. Breadth-first from the guest through passages; `from` remembers the way back.
        std::array<int, maxCells> from;
        from.fill(-2);
        std::array<int, maxCells> queue{};
        int head = 0, tail = 0, end = -1;
        from[U(me.cell)] = -1;
        queue[U(tail++)] = me.cell;
        while (head < tail && end < 0)
        {
            const int at = queue[U(head++)];
            for (int d = 0; d < 4 && end < 0; ++d)
            {
                const int next = Neighbour(def, at, d);
                // 2. An unopened target: reached through a doorway of a cell on the way.
                if (!open && next == cell && Doorway(def, state, at, d))
                    from[U(cell)] = at, end = cell;
                else if (Passage(def, state, at, d) && from[U(next)] == -2)
                {
                    from[U(next)] = at, queue[U(tail++)] = next;
                    if (next == cell)
                        end = cell;
                }
            }
        }
        // 3. The way back, turned round.
        std::vector<int> path;
        for (int c = end; c >= 0 && c != me.cell; c = from[U(c)])
            path.insert(path.begin(), c);
        return path;
    }

    glm::vec3 CellCenter(const NightDef &def, int cell)
    {
        const auto &b = def.rules.board;
        const float x = static_cast<float>(cell % b.width) - static_cast<float>(b.width - 1) * 0.5F;
        const float z = static_cast<float>(cell / b.width) - static_cast<float>(b.height - 1) * 0.5F;
        return {x * cellSize, 0.0F, z * cellSize};
    }

    int CellAtPoint(const NightDef &def, glm::vec3 point)
    {
        const auto &b = def.rules.board;
        const int x = static_cast<int>(std::floor(point.x / cellSize + static_cast<float>(b.width) * 0.5F));
        const int z = static_cast<int>(std::floor(point.z / cellSize + static_cast<float>(b.height) * 0.5F));
        return CellAt(def, x, z);
    }
}

namespace umbra::night
{
    int CellUnderRay(const NightDef &def, glm::vec3 origin, glm::vec3 direction)
    {
        // 1. Where origin + t * direction reaches y = 0; t must be ahead of the origin.
        if (direction.y > -1e-6F && direction.y < 1e-6F)
            return -1;
        const float t = -origin.y / direction.y;
        if (t < 0.0F)
            return -1;
        // 2. That point's cell.
        return CellAtPoint(def, origin + t * direction);
    }
}
