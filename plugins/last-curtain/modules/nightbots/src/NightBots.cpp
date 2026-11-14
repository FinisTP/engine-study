#include <NightBots.hpp>

#include <algorithm>
#include <climits>

// How a bot chooses: hard rules first, then the best score from its style (Balance.md "The bots").
namespace umbra::night
{
    namespace
    {
        std::size_t U(int i) { return static_cast<std::size_t>(i); }
        std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

        struct Bot
        {
            const NightDef &def;
            const NightState &state;
            int g;
            const Guest &me;
            const StyleDef &style;
            std::vector<Action> legal;

            bool chaseMinions = true;

            Bot(const NightDef &d, const NightState &s, int guest, bool minions)
                : def(d), state(s), g(guest), me(s.guests[U(guest)]), style(d.styles[U(d.guests[U(guest)].style)]), legal(LegalActions(d, s)),
                  chaseMinions(minions)
            {
            }

            bool Has(const Action &a) const { return std::find(legal.begin(), legal.end(), a) != legal.end(); }
            int MaxHp() const { return Stat(def, state, g, StatKind::MaxHp); }
            bool Below(int percent) const { return me.hp * 100 < percent * MaxHp(); }
            bool Up(int o) const { return !state.guests[U(o)].down && !state.guests[U(o)].dead; }
            bool Sees(int cell) const { return Lit(def, state, me.cell, cell, Stat(def, state, g, StatKind::Light)); }
            int StepsTo(int from, int cell) const { return Steps(def, state, from, cell); }

            // Steps from one cell to every cell through passages (-1 unreachable), in one search.
            std::array<int, maxCells> Distances(int from) const
            {
                std::array<int, maxCells> dist;
                dist.fill(-1);
                std::array<int, maxCells> queue{};
                int head = 0, tail = 0;
                dist[U(from)] = 0;
                queue[U(tail++)] = from;
                while (head < tail)
                {
                    const int cell = queue[U(head++)];
                    for (int d = 0; d < 4; ++d)
                        if (Passage(def, state, cell, d) && dist[U(Neighbour(def, cell, d))] < 0)
                            dist[U(Neighbour(def, cell, d))] = dist[U(cell)] + 1, queue[U(tail++)] = Neighbour(def, cell, d);
                }
                return dist;
            }

            // Where a legal Move leads (-1 for a doorway: an unknown room).
            int Next(const Action &a) const
            {
                if (a.a == transportMove)
                    for (int c = 0; c < maxCells; ++c)
                        if (c != me.cell && state.cells[U(c)].opened && def.rooms[U(state.cells[U(c)].room)].kind == RoomKind::Transport)
                            return c;
                return Passage(def, state, me.cell, a.a) ? Neighbour(def, me.cell, a.a) : -1;
            }

            // The move that brings the guest nearest to `cell` (or farthest, when `away`).
            Action Toward(int cell, bool away = false) const
            {
                Action best{ActionKind::EndTurn};
                int bestSteps = away ? StepsTo(me.cell, cell) : INT_MAX;
                for (const Action &a : legal)
                {
                    const int next = a.kind == ActionKind::Move ? Next(a) : -1;
                    const int n = next < 0 ? -1 : StepsTo(next, cell);
                    if (n >= 0 && (away ? n > bestSteps : n < bestSteps))
                        best = a, bestSteps = n;
                }
                return away || bestSteps < StepsTo(me.cell, cell) ? best : Action{ActionKind::EndTurn};
            }

            // The move that keeps the most steps between the guest and all these cells.
            Action AwayFrom(const std::vector<int> &cells) const
            {
                auto nearest = [&](int from) {
                    int n = INT_MAX;
                    for (int c : cells)
                        if (const int s = StepsTo(from, c); s >= 0)
                            n = std::min(n, s);
                    return n;
                };
                Action best{ActionKind::EndTurn};
                int bestSteps = nearest(me.cell);
                for (const Action &a : legal)
                    if (a.kind == ActionKind::Move && Next(a) >= 0 && nearest(Next(a)) > bestSteps)
                        best = a, bestSteps = nearest(Next(a));
                return best;
            }

            int Slot(ItemKind kind) const
            {
                for (int s = 0; s < maxCarry; ++s)
                    if (me.items[U(s)] >= 0 && def.items[U(me.items[U(s)])].kind == kind && Has({ActionKind::UseItem, I8(s)}))
                        return s;
                return -1;
            }

            bool Beatable(int m) const
            {
                Fighter mine{I8(g)}, it{-1, I8(m)};
                return Damage(def, state, mine, it) * def.rules.battle.rounds >= state.monsters[U(m)].hp || me.hp * 2 > MaxHp();
            }

            Action Battle() const
            {
                const auto &b = state.battle;
                // 1. Heal when low; a boost in the first round.
                if (const int heal = Slot(ItemKind::Heal); heal >= 0 && Below(style.healBelowPercent))
                    return {ActionKind::UseItem, I8(heal)};
                if (const int boost = Slot(ItemKind::Boost); boost >= 0 && b.round == 1)
                    return {ActionKind::UseItem, I8(boost)};
                // 2. Leave when nearly down.
                if (Has({ActionKind::Leave}) && Below(style.fleeBelowHpPercent))
                    return {ActionKind::Leave};
                // 3. Strike the weakest enemy.
                Action best{ActionKind::Guard};
                int bestHp = INT_MAX;
                for (const Action &a : legal)
                    if (a.kind == ActionKind::Strike)
                    {
                        const Fighter &t = b.fighters[U(a.a)];
                        const int hp = t.monster >= 0 ? state.monsters[U(t.monster)].hp : state.guests[U(t.guest)].hp;
                        if (hp < bestHp)
                            best = a, bestHp = hp;
                    }
                return Has(best) ? best : legal.front();
            }

            Action Taken() const
            {
                // 1. The power, on the weakest guest it can reach.
                Action power{};
                int weakest = INT_MAX;
                for (const Action &a : legal)
                    if (a.kind == ActionKind::Power)
                    {
                        const int hp = a.a >= 0 && a.a < state.count ? state.guests[U(a.a)].hp : INT_MAX - 1;
                        if (hp < weakest)
                            power = a, weakest = hp;
                    }
                if (power.kind == ActionKind::Power)
                    return power;
                // 2. Lone guests are prey; three or more together are to be avoided.
                std::vector<int> crowds, lone;
                for (int o = 0; o < state.count; ++o)
                {
                    if (o == g || !Up(o))
                        continue;
                    int around = 0;
                    for (int p = 0; p < state.count; ++p)
                        if (p != g && Up(p) && StepsTo(state.guests[U(o)].cell, state.guests[U(p)].cell) >= 0 &&
                            StepsTo(state.guests[U(o)].cell, state.guests[U(p)].cell) <= style.huntNearSteps)
                            ++around;
                    (around >= style.crowd ? crowds : around == 1 ? lone : crowds).push_back(state.guests[U(o)].cell);
                }
                for (const Action &a : legal)
                    if (a.kind == ActionKind::Attack && std::find(lone.begin(), lone.end(), me.cell) != lone.end())
                        return a;
                int target = -1, best = INT_MAX;
                for (int c : lone)
                    if (const int n = StepsTo(me.cell, c); n >= 0 && n < best)
                        target = c, best = n;
                if (target >= 0 && Toward(target).kind == ActionKind::Move)
                    return Toward(target);
                return AwayFrom(crowds);
            }

            Action Hunt() const
            {
                // The company: go for the Taken when strong and not alone, else gather.
                const int taken = state.taken.guest, at = state.guests[U(taken)].cell;
                bool known = false;
                for (int o = 0; o < state.count; ++o)
                    known = known || (o != taken && Up(o) && Lit(def, state, state.guests[U(o)].cell, at, Stat(def, state, o, StatKind::Light)));
                const int goal = known ? at : state.understage;
                bool friendNear = false;
                int nearest = -1, nearestSteps = INT_MAX;
                for (int o = 0; o < state.count; ++o)
                {
                    if (o == g || o == taken || !Up(o))
                        continue;
                    const int n = StepsTo(me.cell, state.guests[U(o)].cell);
                    friendNear = friendNear || (n >= 0 && n <= style.huntNearSteps);
                    if (n > 0 && n < nearestSteps)
                        nearest = state.guests[U(o)].cell, nearestSteps = n;
                }
                const bool strong = !Below(style.huntHpPercent);
                if (strong && Has({ActionKind::Attack, -1, I8(taken)}))
                    return {ActionKind::Attack, -1, I8(taken)};
                // Minions first: each one killed takes a share of the Taken's life.
                if (chaseMinions && strong)
                {
                    for (const Action &a : legal)
                        if (a.kind == ActionKind::Attack && a.a >= 0)
                            return a;
                    int minion = -1, best = INT_MAX;
                    for (const Monster &m : state.monsters)
                        if (const int n = StepsTo(me.cell, m.cell); m.hp > 0 && n > 0 && n < best)
                            minion = m.cell, best = n;
                    if (minion >= 0 && Toward(minion).kind == ActionKind::Move)
                        return Toward(minion);
                }
                if (strong && friendNear)
                    if (const Action step = Toward(goal); step.kind == ActionKind::Move)
                        return step;
                if (nearest >= 0)
                    return Toward(nearest);
                return {ActionKind::EndTurn};
            }

            // The style's score for one ordinary action in the evening.
            int Score(const Action &a) const
            {
                switch (a.kind)
                {
                case ActionKind::Move:
                {
                    const int next = Next(a);
                    if (next < 0)
                        return style.explore * 10; // a new room
                    if (const int m = [&] {
                            for (std::size_t i = 0; i < state.monsters.size(); ++i)
                                if (state.monsters[i].cell == next && state.monsters[i].hp > 0)
                                    return static_cast<int>(i);
                            return -1;
                        }();
                        m >= 0)
                        return Beatable(m) ? style.fight * 10 : -100;
                    int score = -1;
                    const auto fromHere = Distances(me.cell), fromNext = Distances(next);
                    auto goal = [&](int cell, int weight) {
                        const int now = fromHere[U(cell)], then = fromNext[U(cell)];
                        if (then >= 0 && now >= 0 && then < now)
                            score = std::max(score, weight * 10 - then);
                    };
                    for (int c = 0; c < maxCells; ++c)
                    {
                        if (!state.cells[U(c)].opened)
                            continue;
                        for (int d = 0; d < 4; ++d)
                            if (Doorway(def, state, c, d))
                                goal(c, style.explore);
                        if (def.rooms[U(state.cells[U(c)].room)].kind == RoomKind::Bonus && !((me.bonusUsed >> c) & 1u))
                            goal(c, style.bonus);
                    }
                    for (const FloorItem &f : state.floor)
                        if (Sees(f.cell))
                            goal(f.cell, style.loot);
                    for (std::size_t m = 0; m < state.monsters.size(); ++m)
                        if (Sees(state.monsters[m].cell) && Beatable(static_cast<int>(m)))
                            goal(state.monsters[m].cell, style.fight);
                    for (int o = 0; o < state.count; ++o)
                        if (o != g && Up(o) && Bond(state, g, o) < 0 && Sees(state.guests[U(o)].cell) &&
                            fromNext[U(state.guests[U(o)].cell)] < fromHere[U(state.guests[U(o)].cell)])
                            score -= style.avoidGuests * 10;
                    return score;
                }
                case ActionKind::UseItem:
                    return a.b < 0 && Below(style.healBelowPercent) ? 50 : a.b >= 0 ? 40 : -1; // relics wake the fallen
                case ActionKind::PickUp:
                    return style.loot * 10;
                case ActionKind::Attack:
                    return a.a >= 0 && Beatable(a.a) ? style.fight * 10 : -100;
                case ActionKind::EndTurn:
                {
                    const RoomDef &room = def.rooms[U(state.cells[U(me.cell)].room)];
                    return room.kind == RoomKind::Bonus && !((me.bonusUsed >> me.cell) & 1u) ? style.bonus * 10 + 5 : 0;
                }
                default:
                    return -100;
                }
            }

            Action Choose() const
            {
                if (legal.empty())
                    return {ActionKind::EndTurn};
                // 1. A level: the stat of its weapon.
                if (legal.front().kind == ActionKind::LevelUp)
                    return {ActionKind::LevelUp, I8(def.guests[U(g)].weapon == Weapon::Physical ? 0 : 1)};
                if (state.battle.active)
                    return Battle();
                // A question to answer (the player's guest as a bot): the likelier roll.
                if (legal.front().kind == ActionKind::Answer)
                {
                    const int physical = ChancePercent(Stat(def, state, g, StatKind::Might), AnswerTarget(def, state, 0));
                    const int magical = ChancePercent(Stat(def, state, g, StatKind::Sanity), AnswerTarget(def, state, 1));
                    return {ActionKind::Answer, I8(physical >= magical ? 0 : 1)};
                }
                // A chatty bot, or one fond of the player, asks the player a question first.
                if (state.player >= 0 && Has({ActionKind::OpenChat, I8(state.player)}) && (style.chatty >= 5 || Bond(state, g, state.player) > 0))
                    return {ActionKind::OpenChat, I8(state.player)};
                if (IsTaken(state, g))
                    return Taken();
                // 2. Hurt: heal, else get away from monsters (and the Taken).
                if (Below(style.fleeBelowHpPercent))
                {
                    if (const int heal = Slot(ItemKind::Heal); heal >= 0)
                        return {ActionKind::UseItem, I8(heal)};
                    std::vector<int> threats;
                    for (const Monster &m : state.monsters)
                        threats.push_back(m.cell);
                    if (state.taken.present)
                        threats.push_back(state.guests[U(state.taken.guest)].cell);
                    if (!threats.empty())
                        return AwayFrom(threats);
                }
                if (state.taken.present)
                    return Hunt();
                // 3. Otherwise the best score; ties to the first.
                Action best = legal.front();
                int bestScore = INT_MIN;
                for (const Action &a : legal)
                    if (const int s = Score(a); s > bestScore)
                        best = a, bestScore = s;
                return best;
            }
        };
    }

    Action ChooseAction(const NightDef &def, const NightState &state, int guest, bool chaseMinions)
    {
        return Bot(def, state, guest, chaseMinions).Choose();
    }

    int PlayNight(const NightDef &def, NightState &state)
    {
        int applies = 0;
        while (Current(state) >= 0 && applies < 5000)
        {
            if (!Apply(def, state, ChooseAction(def, state, Current(state))))
                break;
            ++applies;
        }
        return applies;
    }
}
