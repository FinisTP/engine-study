#include <MatchBots.hpp>

#include <algorithm>
#include <deque>

// The faction match's bots (Bots.md).
namespace umbra::match
{
    namespace
    {
        std::size_t U(int i) { return static_cast<std::size_t>(i); }
        std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

        // Weights (Bots.md "Weights").
        constexpr double performPerEssence = 10, stealPerEssence = 4, killCharacter = 12, killMonsterPerEssence = 3,
                         stagePull = 2.5, huntPull = 1.2, homePull = 3, guardPull = 2, defendPull = 2, huntDanger = 30, fallRisk = 20;

        // A number from the state, the unit and a key: the same for every call in one turn.
        double Noise(const MatchState &s, int unit, int key)
        {
            std::uint64_t z = s.rng ^ (static_cast<std::uint64_t>(s.round) << 40) ^ (static_cast<std::uint64_t>(unit) << 32) ^ static_cast<std::uint64_t>(key);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return static_cast<double>((z ^ (z >> 31)) % 2001) / 1000.0 - 1.0; // -1 .. 1
        }

        double Noisy(const MatchDef &def, const MatchState &s, BotLevel level, int unit, int key, double v)
        {
            return v + Noise(s, unit, key) * def.rules.bots.noise[U(static_cast<int>(level))] / 100.0 * (std::abs(v) + 5);
        }

        int Dist(const MatchState &s, int a, int b)
        {
            const int d = Steps(s.board, a, b);
            return d < 0 ? 99 : d;
        }

        // The squares reachable this turn (free, through doors), with the first step to each.
        struct Reach
        {
            std::vector<int> cells, firstDir;
        };

        Reach Reachable(const MatchDef &def, MatchState &work, int me)
        {
            const MatchState &s = work;
            Reach r;
            std::vector<int> steps(s.board.cells.size(), -1), first(s.board.cells.size(), -1);
            const int start = s.units[U(me)].cell;
            std::deque<int> open{start};
            steps[U(start)] = 0;
            while (!open.empty())
            {
                const int c = open.front();
                open.pop_front();
                r.cells.push_back(c);
                if (steps[U(c)] >= s.now.movesLeft)
                    continue;
                for (int d = 0; d < 4; ++d)
                {
                    const int n = Neighbour(s.board, c, d);
                    if (n < 0 || steps[U(n)] >= 0)
                        continue;
                    // 1. Ask the rules whether that step is allowed, as if the unit stood on c.
                    const std::int16_t cell = work.units[U(me)].cell;
                    const std::int8_t left = work.now.movesLeft;
                    work.units[U(me)].cell = static_cast<std::int16_t>(c);
                    work.now.movesLeft = 1;
                    const bool ok = Check(def, work, {ActionKind::Move, I8(d)}) == Why::Ok;
                    work.units[U(me)].cell = cell;
                    work.now.movesLeft = left;
                    if (!ok)
                        continue;
                    steps[U(n)] = steps[U(c)] + 1;
                    first[U(n)] = c == start ? d : first[U(c)];
                    open.push_back(n);
                }
            }
            for (int c : r.cells)
                r.firstDir.push_back(first[U(c)]);
            return r;
        }

        int BestStrike(const MatchDef &def, const MatchState &s, int me, int foe)
        {
            int best = 0;
            for (std::int8_t a : s.units[U(me)].abilities)
                if (a >= 0 && def.abilities[U(a)].kind == AbilityClass::Strike && s.units[U(me)].hp > def.abilities[U(a)].hpCost)
                {
                    const AbilityDef &ab = def.abilities[U(a)];
                    best = std::max(best, Damage(def, s, a, me, foe, ab.dice.count * (ab.dice.sides + 1) / 2));
                }
            return best;
        }

        // What attacking `foe` is worth: what it carries if it falls, less the risk of falling.
        double AttackValue(const MatchDef &def, const MatchState &s, int me, int foe)
        {
            const Unit &u = s.units[U(me)], &f = s.units[U(foe)];
            const int rounds = def.rules.battle.rounds;
            const bool wins = BestStrike(def, s, me, foe) * rounds >= f.hp;
            const bool loses = BestStrike(def, s, foe, me) * rounds >= u.hp;
            double v = 0;
            if (f.kind == 0)
                v = wins ? killCharacter + f.essence * stealPerEssence : f.essence * 0.5;
            else
                v = wins ? f.essence * killMonsterPerEssence + def.monsters[U(f.def)].xp : -3;
            if (loses)
                v -= fallRisk + u.essence * stealPerEssence;
            return v;
        }

        // What a hunter that sees `cell` and would beat me threatens: falling and losing what I carry.
        double Danger(const MatchDef &def, const MatchState &s, int me, int cell)
        {
            const Unit &u = s.units[U(me)];
            double danger = 0;
            for (int h = 0; h < static_cast<int>(s.units.size()); ++h)
            {
                const Unit &m = s.units[U(h)];
                if (m.kind != 1 || m.cell < 0 || m.hp <= 0 || def.monsters[U(m.def)].behaviour != Behaviour::Hunt)
                    continue;
                int d = 99;
                for (int c : Squares(s.board, m))
                    d = std::min(d, Dist(s, cell, c));
                if (d <= def.monsters[U(m.def)].sight && BestStrike(def, s, h, me) * def.rules.battle.rounds >= u.hp)
                    danger += huntDanger + u.essence * stealPerEssence;
            }
            return danger;
        }

        // How good it is to stand on `cell` for this role, before any action.
        double Place(const MatchDef &def, const MatchState &s, int me, int cell, BotRole role)
        {
            const Unit &u = s.units[U(me)];
            const double danger = Danger(def, s, me, cell);
            if (danger > 0)
                return -danger - homePull * Dist(s, cell, u.home);
            if (u.hp * 3 < u.maxHp)
                return -homePull * Dist(s, cell, u.home);
            if (u.essence > 0 && role == BotRole::Carry)
                return -stagePull * Dist(s, cell, s.stage) * (1 + u.essence / 10.0);
            if (role == BotRole::Guard)
                return -guardPull * Dist(s, cell, s.board.door[U(u.faction)]);
            int best = 99;
            for (const Unit &t : s.units)
            {
                const bool prey = role == BotRole::Defend ? t.kind == 0 && t.faction != u.faction && t.essence > 0
                                                          : t.kind == 1 && t.faction != u.faction && def.monsters[U(t.def)].tier != Tier::Guardian;
                if (prey && t.cell >= 0 && t.hp > 0)
                    best = std::min(best, Dist(s, cell, t.cell));
            }
            if (role == BotRole::Defend)
                return -defendPull * std::min(best, Dist(s, cell, s.stage));
            return -huntPull * best;
        }

        // The best action from where the unit stands, and its value (EndTurn = 0).
        std::pair<Action, double> BestHere(const MatchDef &def, const MatchState &s, int me)
        {
            std::pair<Action, double> best{{ActionKind::EndTurn}, 0};
            const Unit &u = s.units[U(me)];
            auto consider = [&](const Action &a, double v) {
                if (v > best.second && Check(def, s, a) == Why::Ok)
                    best = {a, v};
            };
            consider({ActionKind::Perform}, u.essence * performPerEssence);
            for (int t = 0; t < static_cast<int>(s.units.size()); ++t)
                consider({ActionKind::Attack, I8(t)}, AttackValue(def, s, me, t));
            for (int slot = 0; slot < maxAbilities; ++slot)
                for (int t = 0; t < static_cast<int>(s.units.size()); ++t)
                {
                    const Action a{ActionKind::BoardAbility, I8(slot), I8(t)};
                    const Forecast f = Preview(def, s, a);
                    const Unit &target = s.units[U(t)];
                    const double v = f.healed > 0 ? (target.hp * 5 < target.maxHp * 3 ? std::min(f.healed, target.maxHp - target.hp) * 0.8 : 0)
                                                  : f.dealtMin >= target.hp ? killCharacter * (target.kind == 0) + target.essence * stealPerEssence + 4 : f.dealtMin * 0.5;
                    consider(a, v - f.cost * 0.3);
                }
            for (int slot = 0; slot < maxItems; ++slot)
                if (u.items[U(slot)] >= 0)
                    consider({ActionKind::UseItem, I8(slot)}, def.items[U(u.items[U(slot)])].kind == ItemClass::HealItem
                                                                 ? (u.hp * 2 < u.maxHp ? def.items[U(u.items[U(slot)])].amount * 0.7 : 0)
                                                                 : 2);
            for (int f = 0; f < static_cast<int>(s.floor.size()); ++f)
                consider({ActionKind::PickUp, I8(f)}, s.floor[U(f)].essence * 2 + (s.floor[U(f)].item >= 0 ? 3 : 0) + (s.floor[U(f)].treasure ? 6 : 0));
            consider({ActionKind::Answer, 0}, 1.5);
            return best;
        }

        Action BattlePick(const MatchDef &def, const MatchState &s, int me, BotLevel level)
        {
            // 1. The affordable battle ability with the best value: damage, a shield when hurt, a heal when low.
            // At each foe standing in the other group (a group battle has up to two).
            const Unit &u = s.units[U(me)];
            Action best{ActionKind::BattleAbility, 0};
            double bestValue = -1e9;
            const auto mine = std::ranges::find(s.battle.fighters, me) - s.battle.fighters.begin();
            for (int place = mine < 2 ? 2 : 0, end = place + 2; place < end; ++place)
            for (int slot = 0; slot < maxAbilities; ++slot)
            {
                const int foe = s.battle.fighters[U(place)];
                const Action a{ActionKind::BattleAbility, I8(slot), I8(foe)};
                if (foe < 0 || s.units[U(foe)].hp <= 0 || s.units[U(foe)].cell < 0 || Check(def, s, a) != Why::Ok)
                    continue;
                const AbilityDef &ab = def.abilities[U(u.abilities[U(slot)])];
                const Forecast f = Preview(def, s, a);
                double v = (f.dealtMin + f.dealtMax) / 2.0 + (f.dealtMin >= s.units[U(foe)].hp ? 20 : 0);
                if (ab.kind == AbilityClass::Shield)
                    v = ab.power * (BestStrike(def, s, foe, me) >= u.hp ? 1.5 : 0.6);
                if (ab.kind == AbilityClass::Heal)
                    v = u.hp * 2 < u.maxHp ? ab.power : 0;
                v = Noisy(def, s, level, me, 900 + slot, v - f.cost * 0.5);
                if (v > bestValue)
                    best = a, bestValue = v;
            }
            return best;
        }
    }

    std::vector<BotRole> Roles(const MatchDef &def, const MatchState &s, int faction)
    {
        std::vector<BotRole> roles(s.units.size(), BotRole::Carry);
        std::vector<int> mine;
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[U(i)].kind == 0 && s.units[U(i)].faction == faction && s.units[U(i)].cell >= 0)
                mine.push_back(i);
        auto nearest = [&](int cell, int skip) {
            int best = -1;
            for (int i : mine)
                if (i != skip && (best < 0 || Dist(s, s.units[U(i)].cell, cell) < Dist(s, s.units[U(best)].cell, cell)))
                    best = i;
            return best;
        };
        // 1. Guard: the one nearest home, while an enemy is close to the hub and the guardian stands.
        const int hub = s.board.hub[U(faction)];
        int guard = -1;
        const bool threat = std::ranges::any_of(s.units, [&](const Unit &t) {
            return t.kind == 0 && t.faction != faction && t.cell >= 0 && Dist(s, t.cell, hub) <= def.rules.board.strongMin;
        });
        const bool guardian = std::ranges::any_of(s.units, [&](const Unit &t) {
            return t.kind == 1 && t.faction == faction && t.hp > 0 && def.monsters[U(t.def)].tier == Tier::Guardian;
        });
        if (threat && guardian && (guard = nearest(hub, -1)) >= 0)
            roles[U(guard)] = BotRole::Guard;
        // 2. Defend: the one nearest the stage, while an enemy carries essence.
        const bool carrier = std::ranges::any_of(s.units, [&](const Unit &t) { return t.kind == 0 && t.faction != faction && t.essence > 0 && t.cell >= 0; });
        if (carrier && mine.size() > 1)
            if (const int d = nearest(s.stage, guard); d >= 0 && s.units[U(d)].essence == 0)
                roles[U(d)] = BotRole::Defend;
        return roles;
    }

    Action ChooseAction(const MatchDef &def, const MatchState &s, BotLevel level)
    {
        const int me = Current(s);
        if (me < 0)
            return {ActionKind::EndTurn};
        const Unit &u = s.units[U(me)];
        if (u.pending >= 0)
        {
            // A level's cards: a new ability, attack or an upgrade first; HP, defence; then speed and move.
            static const double byStat[] = {2.0, 2.6, 2.0, 1.0, 1.4};
            int best = 0;
            double bestValue = -1e9;
            for (int c = 0; c < static_cast<int>(s.offer.size()); ++c)
            {
                const LevelCard &card = s.offer[U(c)];
                const double v = card.kind == 0 ? 3.0 : card.kind == 1 ? byStat[U(card.a)] : (card.b == 0 ? 2.6 : 1.6);
                if (const double n = Noisy(def, s, level, me, 600 + c, v); n > bestValue)
                    best = c, bestValue = n;
            }
            return {ActionKind::LevelUp, I8(best)};
        }
        if (s.battle.on)
            return BattlePick(def, s, me, level);
        if (!s.cards.empty()) // face down: any card is as good as another
            return {ActionKind::Card, I8(static_cast<int>(Noise(s, me, 31) > 0.0 ? 0 : s.cards.size() - 1))};
        if (s.now.acted)
            return {ActionKind::EndTurn};
        // 1. Every reachable square: its place value plus the best action there; go to the best.
        const BotRole role = Roles(def, s, u.faction)[U(me)];
        MatchState work = s;
        work.log.clear(); // the bots never read it
        const Reach reach = Reachable(def, work, me);
        int bestCell = u.cell;
        double bestValue = -1e9;
        Action bestAction{ActionKind::EndTurn};
        for (std::size_t i = 0; i < reach.cells.size(); ++i)
        {
            work.units[U(me)].cell = static_cast<std::int16_t>(reach.cells[i]);
            const auto [action, value] = BestHere(def, work, me);
            const double v = Noisy(def, s, level, me, reach.cells[i], Place(def, work, me, reach.cells[i], role) + value);
            if (v > bestValue)
                bestValue = v, bestCell = reach.cells[i], bestAction = action;
        }
        if (bestCell != u.cell)
        {
            const auto at = std::ranges::find(reach.cells, bestCell) - reach.cells.begin();
            return {ActionKind::Move, I8(reach.firstDir[U(static_cast<int>(at))])};
        }
        return Check(def, s, bestAction) == Why::Ok ? bestAction : Action{ActionKind::EndTurn};
    }
}
