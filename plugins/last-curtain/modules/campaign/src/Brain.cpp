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
}
