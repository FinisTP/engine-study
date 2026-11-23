#include <Campaign.hpp>
#include <MatchBots.hpp>

namespace umbra::campaign
{
    match::Action EnemyAction(const match::MatchDef &def, const BuiltStage &stage, const match::MatchState &s, match::BotLevel level)
    {
        const int me = match::Current(s);
        if (me < 0 || static_cast<std::size_t>(me) >= stage.behaviour.size())
            return match::ChooseAction(def, s, level);
        const match::Unit &u = s.units[static_cast<std::size_t>(me)];
        const UnitBehaviour &b = stage.behaviour[static_cast<std::size_t>(me)];
        const match::Action hold{match::ActionKind::EndTurn};
        // 1. Aggressive after n: holds until that round.
        if (b.kind == Behaviour::Aggressive && s.round < b.after)
            return hold;
        // 2. Passive: holds until hurt, or until a party unit stands in its room within 4 squares.
        if (b.kind == Behaviour::Passive && u.hp >= u.maxHp)
        {
            const auto room = [&](int cell) { return cell >= 0 ? stage.rooms[static_cast<std::size_t>(cell)] : -2; };
            const bool sees = std::ranges::any_of(s.units, [&](const match::Unit &o) {
                return o.faction == 0 && match::Up(o) && room(o.cell) == room(u.cell) && room(u.cell) >= 0 &&
                       match::Steps(s.board, u.cell, o.cell) <= stage.sight;
            });
            if (!sees)
                return hold;
        }
        // 3. Otherwise the faction bots play it.
        return match::ChooseAction(def, s, level);
    }

    match::Action PartyAction(const match::MatchDef &def, const BuiltStage &stage, const match::MatchState &s, match::BotLevel level)
    {
        using match::Action;
        using match::ActionKind;
        const int me = match::Current(s);
        const auto i8 = [](int v) { return static_cast<std::int8_t>(v); };
        const auto ok = [&](const Action &a) { return match::Check(def, s, a) == match::Why::Ok; };
        int recruit = -1;
        for (std::size_t i = 0; i < s.units.size(); ++i)
            if ((s.units[i].flags & match::flag::Strung) && s.units[i].cell >= 0)
                recruit = static_cast<int>(i);
        const match::Unit &u = s.units[static_cast<std::size_t>(me)];
        if (recruit < 0 || !s.mode.mustFree || s.battle.on || u.pending >= 0 || !s.cards.empty())
            return match::ChooseAction(def, s, level);
        // 1. Cut when beside; attack the recruit when it stands.
        if (const Action cut{ActionKind::Cut, i8(recruit)}; ok(cut))
            return cut;
        const match::Unit &r = s.units[static_cast<std::size_t>(recruit)];
        if (const Action hit{ActionKind::Attack, i8(recruit)}; match::Up(r) && ok(hit))
            return hit;
        // 2. Otherwise a step nearer to it (the player only once it has fallen), then end the turn.
        const bool go = match::Up(r) || (u.flags & match::flag::Player);
        const int now = match::Steps(s.board, u.cell, r.cell);
        for (int d = 0; go && d < 4; ++d)
            if (const int n = match::Neighbour(s.board, u.cell, d); n >= 0 && match::Steps(s.board, n, r.cell) < now && ok({ActionKind::Move, i8(d)}))
                return {ActionKind::Move, i8(d)};
        return {ActionKind::EndTurn};
    }
}
