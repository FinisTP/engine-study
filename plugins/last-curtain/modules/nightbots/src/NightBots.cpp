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
                // A bot never walks into a room it has already been in this turn (it heads somewhere; no pacing).
                std::erase_if(legal, [&](const Action &a) { return a.kind == ActionKind::Move && Next(a) >= 0 && me.visited.test(U(Next(a))); });
            }

            bool Has(const Action &a) const { return std::find(legal.begin(), legal.end(), a) != legal.end(); }
            int MaxHp() const { return Stat(def, state, g, StatKind::MaxHp); }
            bool Below(int percent) const { return me.hp * 100 < percent * MaxHp(); }
            bool Up(int o) const { return Living(state, o); }
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

            // How a fight with guest `o` would go, roughly: what each deals over the battle's rounds against
            // what the other has left. Positive: this bot would come out ahead.
            int Edge(int o) const
            {
                const Fighter mine{I8(g)}, theirs{I8(o)};
                const int rounds = def.rules.battle.rounds;
                return (Damage(def, state, mine, theirs) * rounds - state.guests[U(o)].hp) - (Damage(def, state, theirs, mine) * rounds - me.hp);
            }

            // A ghost of the house: strike the living in its room, else go after the nearest.
            Action Ghost() const
            {
                Action strike{}, toward{ActionKind::EndTurn};
                int weakest = INT_MAX, nearest = INT_MAX;
                for (const Action &a : legal)
                    if (a.kind == ActionKind::Attack && state.guests[U(a.b)].hp < weakest)
                        strike = a, weakest = state.guests[U(a.b)].hp;
                if (strike.kind == ActionKind::Attack)
                    return strike;
                for (int o = 0; o < state.count; ++o)
                    if (const int n = Up(o) ? StepsTo(me.cell, state.guests[U(o)].cell) : -1; n > 0 && n < nearest)
                        if (const Action step = Toward(state.guests[U(o)].cell); step.kind == ActionKind::Move)
                            toward = step, nearest = n;
                return toward;
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
                const int goal = at; // after midnight everyone knows where the Taken is (the user)
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

            // Whether the bot's side needs this item: healing, or a keepsake of its own or a known member's goal.
            bool Needed(int item) const
            {
                if (def.items[U(item)].kind == ItemKind::Heal)
                    return true;
                for (int o = 0; o < state.count; ++o)
                    if (o == g || (me.group >= 0 && state.guests[U(o)].group == me.group && ((me.knows >> o) & 1u)))
                        if (const int goal = state.guests[U(o)].goal; goal >= 0 && def.goals[U(goal)].kind == GoalKind::Item &&
                                                                     def.goals[U(goal)].target == item && !GoalDone(def, state, o))
                            return true;
                return false;
            }

            // The nearest cell that guest `who`'s goal needs, as this bot knows it (-1: none).
            int GoalCell(int who) const
            {
                const Guest &holder = state.guests[U(who)];
                if (holder.goal < 0 || GoalDone(def, state, who))
                    return -1;
                const GoalDef &goal = def.goals[U(holder.goal)];
                const bool mine = who == g;
                int best = -1, bestSteps = INT_MAX;
                auto consider = [&](int cell) {
                    if (const int n = StepsTo(me.cell, cell); n >= 0 && n < bestSteps)
                        best = cell, bestSteps = n;
                };
                switch (goal.kind)
                {
                case GoalKind::Room: // opened rooms are known to all
                    for (int c = 0; mine && c < maxCells; ++c)
                        if (state.cells[U(c)].opened && state.cells[U(c)].room == goal.target)
                            consider(c);
                    break;
                case GoalKind::Item: // on the floor of its room, or carried by a guest in the light
                    for (const FloorItem &f : state.floor)
                        if (f.item == goal.target)
                            consider(f.cell);
                    for (int o = 0; o < state.count; ++o)
                        if (o != g && o != who && Up(o) && Sees(state.guests[U(o)].cell) &&
                            std::find(state.guests[U(o)].items.begin(), state.guests[U(o)].items.end(), goal.target) != state.guests[U(o)].items.end())
                            consider(state.guests[U(o)].cell);
                    break;
                case GoalKind::Events: // a room with a test still in it
                    for (int c = 0; mine && c < maxCells; ++c)
                        if (state.cells[U(c)].opened && !state.cells[U(c)].resolved && def.rooms[U(state.cells[U(c)].room)].kind == RoomKind::Event)
                            consider(c);
                    break;
                case GoalKind::Monsters:
                    for (std::size_t m = 0; mine && m < state.monsters.size(); ++m)
                        if (state.monsters[m].hp > 0 && Sees(state.monsters[m].cell) && Beatable(static_cast<int>(m)))
                            consider(state.monsters[m].cell);
                    break;
                }
                return best;
            }

            // The goal work in reach: a chat, an attack or a pick-up here, else a step toward the target.
            Action Purpose() const
            {
                // Picking up costs no step; a chat ends the turn.
                for (const Action &a : legal)
                    if (a.kind == ActionKind::PickUp && Needed(state.floor[U(a.a)].item))
                        return a;
                const int goal = me.goal < 0 || GoalDone(def, state, g) ? -1 : me.goal;
                if (goal >= 0)
                {
                    const GoalDef &def0 = def.goals[U(goal)];
                    if (def0.kind == GoalKind::Monsters)
                        for (const Action &a : legal)
                            if (a.kind == ActionKind::Attack && a.a >= 0 && Beatable(a.a))
                                return a;
                }
                // 1. Its own goal; 2. a known goal of a group member.
                int target = GoalCell(g);
                for (int o = 0; target < 0 && o < state.count; ++o)
                    if (o != g && me.group >= 0 && state.guests[U(o)].group == me.group && ((me.knows >> o) & 1u))
                        target = GoalCell(o);
                if (target < 0 || target == me.cell)
                    return {ActionKind::EndTurn};
                return Toward(target);
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
                    // The killer: never into its room; keep out of its reach (it senses senseSteps away).
                    if (state.killer.cell >= 0)
                    {
                        const int k = StepsTo(next, state.killer.cell), now = StepsTo(me.cell, state.killer.cell);
                        if (k == 0)
                            return -1000;
                        if (k >= 0 && k <= def.rules.killer.senseSteps && (now < 0 || k <= now))
                            return -200 + k * 20 + (now >= 0 && k > now ? 100 : 0);
                    }
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
                        const RoomKind kind = def.rooms[U(state.cells[U(c)].room)].kind;
                        if (!state.cells[U(c)].resolved && (kind == RoomKind::Item || kind == RoomKind::Event)) // the map is whole: rooms still holding something
                            goal(c, kind == RoomKind::Item ? style.loot : style.explore);
                        if (kind == RoomKind::Bonus && !me.bonusUsed[U(c)])
                            goal(c, style.bonus);
                    }
                    for (int o = 0; o < state.count; ++o) // prey it would beat, in its light
                        if (o != g && Up(o) && Sees(state.guests[U(o)].cell) && Edge(o) > 0)
                            goal(state.guests[U(o)].cell, style.fight);
                    for (const FloorItem &f : state.floor)
                        if (Sees(f.cell))
                            goal(f.cell, style.loot);
                    for (std::size_t m = 0; m < state.monsters.size(); ++m)
                        if (Sees(state.monsters[m].cell) && Beatable(static_cast<int>(m)))
                            goal(state.monsters[m].cell, style.fight);
                    for (int o = 0; o < state.count; ++o) // someone to talk with
                        if (o != g && Up(o) && Bond(state, g, o) >= 0 && me.chatRound != state.round && Sees(state.guests[U(o)].cell) &&
                            (me.group < 0 || state.guests[U(o)].group != me.group))
                            goal(state.guests[U(o)].cell, style.chatty + (me.goal < 0 || GoalDone(def, state, g) ? 5 : 0)); // idle: seek company
                    for (int o = 0; o < state.count; ++o) // a guest who would beat it, or a ghost: keep away
                        if (o != g && InPlay(state, o) && (state.guests[U(o)].ghost || Edge(o) < 0 || Bond(state, g, o) < 0) && Sees(state.guests[U(o)].cell) &&
                            fromNext[U(state.guests[U(o)].cell)] < fromHere[U(state.guests[U(o)].cell)])
                            score -= (style.avoidGuests + 2) * 10;
                    return score;
                }
                case ActionKind::UseItem:
                    return a.b < 0 && Below(style.healBelowPercent) ? 50 : a.b >= 0 ? 40 : -1; // relics wake the fallen
                case ActionKind::PickUp:
                    return style.greed * style.loot - 1; // greed 0: never (needed items go by Purpose)
                case ActionKind::Attack:
                    if (a.b >= 0) // a guest (or a ghost) in the room: only a fight it would win
                        return state.guests[U(a.b)].ghost ? (Edge(a.b) > 0 ? style.fight * 5 : -100) : Edge(a.b) > 0 ? style.fight * 10 + 5 : -100;
                    return a.a >= 0 && Beatable(a.a) ? style.fight * 10 : -100;
                case ActionKind::EndTurn:
                {
                    const RoomDef &room = def.rooms[U(state.cells[U(me.cell)].room)];
                    return room.kind == RoomKind::Bonus && !me.bonusUsed[U(me.cell)] ? style.bonus * 10 + 5 : 0;
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
                if (me.ghost)
                    return Ghost();
                if (IsTaken(state, g))
                    return Taken();
                // 1b. The killer in sense: run (its reach grows with the hour).
                if (state.killer.cell >= 0 && StepsTo(me.cell, state.killer.cell) >= 0 && StepsTo(me.cell, state.killer.cell) <= def.rules.killer.senseSteps)
                    if (const Action away = AwayFrom({state.killer.cell}); away.kind == ActionKind::Move)
                        return away;
                // 2. Hurt: heal, else get away from monsters (and the Taken).
                if (Below(style.fleeBelowHpPercent))
                {
                    if (const int heal = Slot(ItemKind::Heal); heal >= 0)
                        return {ActionKind::UseItem, I8(heal)};
                    std::vector<int> threats;
                    for (const Monster &m : state.monsters)
                        threats.push_back(m.cell);
                    for (int o = 0; o < state.count; ++o) // guests who would beat it, and ghosts
                        if (o != g && InPlay(state, o) && state.guests[U(o)].cell >= 0 && (state.guests[U(o)].ghost || Edge(o) < 0))
                            threats.push_back(state.guests[U(o)].cell);
                    if (state.taken.present)
                        threats.push_back(state.guests[U(state.taken.guest)].cell);
                    if (!threats.empty())
                        return AwayFrom(threats);
                }
                if (state.taken.present)
                    return Hunt();
                // 3. Goal work: its own, then a group member's.
                if (const Action purpose = Purpose(); purpose.kind != ActionKind::EndTurn)
                    return purpose;
                // 4. A very chatty bot whose goal is done talks with someone in the room before it moves.
                if (style.chatty >= 6 && (me.goal < 0 || GoalDone(def, state, g)) && me.movesLeft == Stat(def, state, g, StatKind::Speed))
                    for (const Action &a : legal)
                        if (a.kind == ActionKind::Chat)
                            return a;
                // 5. A chatty bot, or one fond of the player, asks the player a question first.
                if (state.player >= 0 && Has({ActionKind::OpenChat, I8(state.player)}) && (style.chatty >= 7 || Bond(state, g, state.player) > 0))
                    return {ActionKind::OpenChat, I8(state.player)};
                // 6. Otherwise the best score; ties to the first.
                Action best = legal.front();
                int bestScore = INT_MIN;
                for (const Action &a : legal)
                    if (const int s = Score(a); s > bestScore)
                        best = a, bestScore = s;
                // 7. Nothing better to do here: talk with someone in the room.
                if (best.kind == ActionKind::EndTurn && style.chatty > 0)
                    for (const Action &a : legal)
                        if (a.kind == ActionKind::Chat)
                            return a;
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
        while (Current(state) >= 0 && applies < 40000) // a run of nights, until one guest is left
        {
            if (!Apply(def, state, ChooseAction(def, state, Current(state))))
                break;
            ++applies;
        }
        return applies;
    }
}
