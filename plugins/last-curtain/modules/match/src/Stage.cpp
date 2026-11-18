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

        void StageRound(const MatchDef &def, MatchState &s)
        {
            // 1. One round before it closes, the next stage room is announced; then the stage moves there.
            if (s.round == s.stageCloses - 1 && s.board.stageRooms.size() > 1)
            {
                do
                    s.nextStage = s.board.stageRooms[U(Roll(s, static_cast<int>(s.board.stageRooms.size())))];
                while (s.nextStage == s.stage);
                Log(s, LogKind::StageNext, s.nextStage);
            }
            if (s.round == s.stageCloses)
            {
                if (s.nextStage >= 0)
                    s.stage = s.nextStage;
                s.nextStage = -1;
                s.stageCloses += def.rules.stage.openRounds;
                Log(s, LogKind::StageOpen, s.stage);
            }
        }
    }
}
