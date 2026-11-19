#include <Match.hpp>

#include "MatchInner.hpp"

// The stage and Perform (Match.md "The stage").
namespace umbra::match
{
    using namespace inner;

    namespace inner
    {
        Why CheckPerform(const MatchDef &, const MatchState &s, int me)
        {
            const Unit &u = s.units[U(me)];
            if (s.now.acted)
                return Why::AlreadyActed;
            if (u.cell != s.stage)
                return Why::NotOnStage;
            return u.essence > 0 ? Why::Ok : Why::NothingToPerform;
        }

        void Perform(const MatchDef &def, MatchState &s, int me)
        {
            Unit &u = s.units[U(me)];
            s.score[U(u.faction)] += u.essence;
            Log(s, LogKind::Performed, me, u.essence);
            u.essence = 0;
            Acted(def, s);
        }
    }
}
