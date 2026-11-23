#include <CampaignCafe.hpp>

#include <algorithm>

namespace umbra::campaign
{
    void AddXp(Member &m, int xp, int capLevel)
    {
        // 1. XP fills levels of 100 each, stopping at the cap.
        for (m.xp += xp; m.level < std::min(capLevel, levelCap) && m.xp >= xpPerLevel;)
            m.xp -= xpPerLevel, ++m.level;
        if (m.level >= std::min(capLevel, levelCap))
            m.xp = std::min(m.xp, xpPerLevel - 1);
    }

    void WinStage(const match::MatchDef &def, Save &save, const StageDef &stage, const BuiltStage &fight, const std::vector<StageDef> &stages)
    {
        const match::MatchState &s = fight.state;
        // 1. XP for who fought; a skirmish stops at the next story stage's recommended level.
        int cap = levelCap;
        if (stage.kind == "skirmish")
            for (const StageDef &next : stages)
                if (next.kind == "story" && !save.facts.contains("won:" + next.id))
                    cap = std::min(cap, next.level);
        for (const std::string &who : fight.who)
            for (Member &m : save.roster)
                if (m.id == who)
                    AddXp(m, stage.kind == "skirmish" && m.level >= cap ? 0 : stage.xp, stage.kind == "skirmish" ? cap : levelCap);
        // 2. Finds to storage; take-once items picked up; the room cleared; the fight's facts.
        for (const std::int8_t item : s.found)
            ++save.storage[def.items[static_cast<std::size_t>(item)].id];
        Room &room = save.rooms[stage.id];
        for (const StageItem &it : stage.items)
            if (!save.taken.contains(it.id) && std::ranges::none_of(s.floor, [&](const match::Floor &f) { return f.cell == it.y * stage.width + it.x; }))
                room.taken.push_back(it.id), save.taken.insert(it.id);
        room.cleared = true;
        RecordFight(def, save, stage.id, s, fight.who);
        for (std::size_t i = 0; i < s.units.size(); ++i)
            if (s.units[i].flags & match::flag::Freed)
                save.facts["freed:" + fight.who[i]] = "1";
        if (stage.id == "t3")
            save.riteUnlocked = true; // Le Thanh teaches the rite after T3
        save.riteUsed |= s.riteUsed;
        // 3. A story win: every cleared skirmish fills again with its next twist.
        if (stage.kind == "story")
        {
            ++save.chapter;
            for (const StageDef &other : stages)
                if (other.kind == "skirmish" && save.rooms.contains(other.id) && save.rooms[other.id].cleared)
                    save.rooms[other.id].cleared = false, ++save.rooms[other.id].refill;
        }
    }

    Save LoseStage(const Save &before, const BuiltStage &fight)
    {
        Save out = before;
        out.riteUsed |= fight.state.riteUsed;
        return out;
    }
}
