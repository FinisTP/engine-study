#include "NightInner.hpp"

#include <algorithm>

// Bonds and the player's allies (Night.md "The company").
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

    int ChancePercent(int stat, int target) { return std::clamp((21 - (target - stat)) * 5, 0, 100); }

    int AnswerTarget(const NightDef &def, const NightState &state, int answer)
    {
        const QuestionDef &q = def.questions[U(state.chat.question)];
        const int bond = Bond(state, state.chat.asker, state.chat.answerer);
        return (answer == 0 ? q.physicalTarget : q.magicalTarget) + (bond > 0 ? -def.rules.persuade.bond : bond < 0 ? def.rules.persuade.bond : 0);
    }

    namespace
    {
        // The group `g` recruits into: the player's is 0, a bot's is 1 + its index (-1: none yet).
        int GroupOf(const NightState &state, int g)
        {
            const int group = state.guests[U(g)].group;
            return group >= 0 ? group : g == state.player ? 0 : 1 + g;
        }
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
            if (state.taken.present)
                continue;
            // 2. Any guest may talk with one not in its group, once a round, while its group has room
            //    and the other is not yet won over.
            if (me.chatRound == state.round)
                continue;
            //    The player is never the asker (bots ask the player through OpenChat), and the
            //    player's allies are not won away.
            if (o != player && other.group != 0 && other.group != GroupOf(state, g) && GroupSize(state, GroupOf(state, g)) < 3 &&
                state.approvals[U(o * maxGuests + g)] < def.rules.persuade.approvals)
                legal.push_back({ActionKind::Chat, I8(o)});
            // 3. A bot may ask the player a question, before it moves.
            if (o == player && me.group != 0 && GroupSize(state, 0) < 3 && state.approvals[U(g * maxGuests + o)] < def.rules.persuade.approvals &&
                me.movesLeft == Stat(def, state, g, StatKind::Speed))
                legal.push_back({ActionKind::OpenChat, I8(o)});
        }
    }

    void inner::OpenChat(const NightDef &def, NightState &state, int opener, int other, bool askedByOpener)
    {
        // The asker puts a question of its pool not asked yet (all asked: any of them) to the answerer.
        const int asker = askedByOpener ? opener : other, answerer = askedByOpener ? other : opener;
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
        state.guests[U(opener)].chatRound = state.round;
        state.chat = {true, I8(asker), I8(answerer), I8(pool[U(pick)])};
    }

    void inner::AnswerChat(const NightDef &def, NightState &state, int answer)
    {
        // 1. A passed roll is one approval, and the asker's goal is told (between bots, both ways).
        const int asker = state.chat.asker, answerer = state.chat.answerer;
        const Check check = RollCheck(def, state, answerer, answer == 0 ? StatKind::Might : StatKind::Sanity, AnswerTarget(def, state, answer));
        std::int8_t &approvals = state.approvals[U(asker * maxGuests + answerer)];
        if (check.passed)
        {
            approvals = I8(std::min<int>(approvals + 1, def.rules.persuade.approvals));
            Guest &a = state.guests[U(asker)], &b = state.guests[U(answerer)];
            b.knows = static_cast<std::uint8_t>(b.knows | (1u << asker));
            if (asker != state.player && answerer != state.player)
                a.knows = static_cast<std::uint8_t>(a.knows | (1u << answerer));
        }
        // 2. Enough approvals: the asker joins the answerer's group.
        const int group = GroupOf(state, answerer);
        if (approvals >= def.rules.persuade.approvals && GroupSize(state, group) < 3)
            state.guests[U(answerer)].group = state.guests[U(asker)].group = I8(group);
        Log(state, LogKind::Persuade, answerer, asker, check.passed ? 1 : 0, approvals);
        state.chat.active = false;
    }

    bool GoalDone(const NightDef &def, const NightState &state, int g)
    {
        const Guest &guest = state.guests[U(g)];
        if (guest.goal < 0)
            return false;
        const GoalDef &goal = def.goals[U(guest.goal)];
        switch (goal.kind)
        {
        case GoalKind::Room: return guest.goalRoomSeen;
        case GoalKind::Item: return std::find(guest.items.begin(), guest.items.end(), I8(goal.target)) != guest.items.end();
        case GoalKind::Events:
        case GoalKind::Monsters: return guest.goalCount >= goal.count;
        }
        return false;
    }

    void inner::PlaceKeepsake(const NightDef &def, NightState &state, int cell)
    {
        // The keepsake of this room lies on its floor when someone's drawn goal asks for it.
        const int room = state.cells[U(cell)].room;
        for (std::size_t i = 0; i < def.items.size(); ++i)
        {
            if (def.items[i].room != room)
                continue;
            for (int g = 0; g < state.count; ++g)
                if (const int goal = state.guests[U(g)].goal; goal >= 0 && def.goals[U(goal)].kind == GoalKind::Item && def.goals[U(goal)].target == static_cast<int>(i))
                {
                    state.floor.push_back({I8(cell), I8(i)});
                    break;
                }
        }
    }

    void inner::EndNight(NightState &state, Outcome outcome)
    {
        state.outcome = outcome;
        state.over = true;
        state.battle.active = false;
        Log(state, LogKind::Ended, -1, -1, static_cast<int>(outcome));
    }
}
