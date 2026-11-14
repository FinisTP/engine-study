#include "NightInner.hpp"

#include <algorithm>

// Bonds, the player's allies and omens (Night.md "The company", "Omens").
namespace umbra::night
{
    using namespace inner;

    namespace
    {
        bool Up(const Guest &g) { return !g.down && !g.dead; }

        int GroupSize(const NightState &state, int group)
        {
            return static_cast<int>(std::count_if(state.guests.begin(), state.guests.begin() + state.count,
                                                   [&](const Guest &g) { return g.group == group && !g.dead; }));
        }
    }

    int Bond(const NightState &state, int from, int to) { return state.bonds[U(from * maxGuests + to)]; }

    bool Controls(const NightState &state, int controller, int guest)
    {
        return controller >= 0 && (guest == controller || (state.guests[U(controller)].group == 0 && state.guests[U(guest)].group == 0));
    }

    Outcome Result(const NightState &state) { return state.outcome; }

    int CurseTarget(const NightDef &def, const NightState &state)
    {
        const auto &omens = def.rules.omens;
        const int hours = (state.round - 1) * 30 / 60;
        return omens.targets[U(std::min<int>(state.omensTaken, static_cast<int>(omens.targets.size()) - 1))] + omens.perHour * hours;
    }

    int ChancePercent(int stat, int target) { return std::clamp((21 - (target - stat)) * 5, 0, 100); }

    int AnswerTarget(const NightDef &def, const NightState &state, int answer)
    {
        const QuestionDef &q = def.questions[U(state.chat.question)];
        const int bond = Bond(state, state.chat.asker, state.player);
        return (answer == 0 ? q.physicalTarget : q.magicalTarget) + (bond > 0 ? -def.rules.persuade.bond : bond < 0 ? def.rules.persuade.bond : 0);
    }

    void inner::CompanyLegal(const NightDef &def, const NightState &state, int g, std::vector<Action> &legal)
    {
        const Guest &me = state.guests[U(g)];
        const int player = state.player;
        for (int o = 0; o < state.count; ++o)
        {
            const Guest &other = state.guests[U(o)];
            if (o == g || other.cell != me.cell || !Up(other))
                continue;
            // 1. The only fight between guests is with the Taken, after midnight.
            if (state.taken.present && (IsTaken(state, g) || IsTaken(state, o)))
                legal.push_back({ActionKind::Attack, -1, I8(o)});
            if (state.taken.present || GroupSize(state, 0) >= 3)
                continue;
            // 2. The player chats with a guest not yet theirs.
            if (g == player && other.group != 0)
                legal.push_back({ActionKind::Chat, I8(o)});
            // 3. A bot may ask the player a question, before it moves, once a turn.
            if (o == player && g != player && me.group != 0 && me.approvals < def.rules.persuade.approvals && me.chatRound != state.round &&
                me.movesLeft == Stat(def, state, g, StatKind::Speed))
                legal.push_back({ActionKind::OpenChat, I8(o)});
        }
    }

    void inner::OpenChat(const NightDef &def, NightState &state, int asker, bool byBot)
    {
        // A question of the asker's pool not asked yet (all asked: any of them).
        Guest &guest = state.guests[U(asker)];
        std::vector<int> pool, fresh;
        for (std::size_t q = 0; q < def.questions.size(); ++q)
            if (def.questions[q].guest == asker)
            {
                if (!((guest.asked >> pool.size()) & 1u))
                    fresh.push_back(static_cast<int>(pool.size()));
                pool.push_back(static_cast<int>(q));
            }
        if (pool.empty())
            return;
        const int pick = fresh.empty() ? Below(state, static_cast<int>(pool.size())) : fresh[U(Below(state, static_cast<int>(fresh.size())))];
        guest.asked = static_cast<std::uint8_t>(guest.asked | (1u << pick));
        if (byBot)
            guest.chatRound = state.round;
        state.chat = {true, byBot, I8(asker), I8(pool[U(pick)])};
    }

    void inner::AnswerChat(const NightDef &def, NightState &state, int answer)
    {
        // A passed roll is one approval; enough of them, and the asker joins the player's group.
        const int asker = state.chat.asker, player = state.player;
        const Check check = RollCheck(def, state, player, answer == 0 ? StatKind::Might : StatKind::Sanity, AnswerTarget(def, state, answer));
        Guest &guest = state.guests[U(asker)];
        if (check.passed)
            guest.approvals = I8(std::min<int>(guest.approvals + 1, def.rules.persuade.approvals));
        if (guest.approvals >= def.rules.persuade.approvals && GroupSize(state, 0) < 3)
            state.guests[U(player)].group = guest.group = 0;
        Log(state, LogKind::Persuade, player, asker, check.passed ? 1 : 0, guest.approvals);
        state.chat.active = false;
    }

    void inner::EndNight(NightState &state, Outcome outcome)
    {
        state.outcome = outcome;
        state.over = true;
        state.battle.active = false;
        Log(state, LogKind::Ended, -1, -1, static_cast<int>(outcome));
    }

    void inner::OfferOmen(const NightDef &def, NightState &state, int cell)
    {
        // The room's omen is drawn the first time anyone walks in.
        Cell &c = state.cells[U(cell)];
        if (c.omen < 0)
        {
            std::vector<int> left;
            for (std::size_t o = 0; o < def.omens.size() && o < 8; ++o)
                if (!((state.omensUsed >> o) & 1u))
                    left.push_back(static_cast<int>(o));
            if (left.empty())
            {
                c.resolved = true;
                return;
            }
            c.omen = I8(left[U(Below(state, static_cast<int>(left.size())))]);
            state.omensUsed = static_cast<std::uint8_t>(state.omensUsed | (1u << c.omen));
        }
        state.omenOffer = I8(cell);
    }

    bool inner::TakeOmen(const NightDef &def, NightState &state, int g)
    {
        Guest &guest = state.guests[U(g)];
        Cell &c = state.cells[U(guest.cell)];
        const OmenDef &omen = def.omens[U(c.omen)];
        // 1. The curse roll: a plain d20, the same odds for everyone, against a target that rises
        //    with every omen taken and with every hour of the night.
        const int target = CurseTarget(def, state);
        const Check check = RollCheck(def, state, g, StatKind::None, target);
        // 2. The gift.
        guest.omens = static_cast<std::uint8_t>(guest.omens | (1u << c.omen));
        guest.hp = I16(guest.hp + omen.maxhp);
        ++state.omensTaken;
        c.resolved = true;
        Log(state, LogKind::Omen, g, -1, c.omen, check.passed ? 1 : 0);
        // 3. A failed roll: this guest turns, now.
        if (!check.passed)
            state.marked = I8(g);
        return !check.passed;
    }
}
