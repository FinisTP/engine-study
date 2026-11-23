#include <CampaignCafe.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace umbra::campaign
{
    bool LoadMap(const std::filesystem::path &file, const std::vector<StageDef> &stages, TheatreMap &out, std::vector<core::Diagnostic> &d)
    {
        const std::size_t before = d.size();
        const auto error = [&](const std::string &field, const std::string &message) { d.push_back({core::Severity::Error, "campaign.map", file.string(), field, message}); };
        std::ifstream in(file);
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (!j.is_object() || j.value("format", "") != "umbra.lc-map" || j.value("version", 0) != 1)
            return error("format", "expected format umbra.lc-map, version 1"), false;
        out.plan = j.value("plan", "");
        const nlohmann::json list = j.value("tickets", nlohmann::json::array());
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            const nlohmann::json &t = list[i];
            const std::string at = "tickets[" + std::to_string(i) + "]";
            Ticket ticket{t.value("id", ""), t.value("name", ""), t.value("kind", ""), t.value("stage", ""), t.value("opensAfter", ""), t.value("lock", "")};
            const std::vector<float> xy = t.value("at", std::vector<float>{});
            if (xy.size() == 2)
                ticket.x = xy[0], ticket.y = xy[1];
            else
                error(at + ".at", "expected [x, y] between 0 and 1");
            if (ticket.kind != "story" && ticket.kind != "skirmish" && ticket.kind != "unknown")
                error(at + ".kind", "expected story, skirmish or unknown");
            if (ticket.kind != "unknown" && std::ranges::none_of(stages, [&](const StageDef &s) { return s.id == ticket.stage; }))
                error(at + ".stage", "\"" + ticket.stage + "\" is not a stage file");
            out.tickets.push_back(ticket);
        }
        return d.size() == before;
    }

    std::vector<TicketView> MapView(const TheatreMap &map, const Save &save, const std::vector<StageDef> &stages)
    {
        std::vector<TicketView> out;
        bool nextGiven = false;
        for (const Ticket &t : map.tickets)
        {
            TicketView v{t.id, t.name, "unknown", t.lock, t.x, t.y, false};
            const bool won = save.facts.contains("won:" + t.stage);
            const bool opened = t.kind != "unknown" && (t.opensAfter.empty() || save.facts.contains("won:" + t.opensAfter));
            // 1. Story: cleared, the first open unwon one is next, else locked. Skirmish: open or locked.
            if (t.kind == "story")
                v.mark = won ? "cleared" : opened && !nextGiven ? (nextGiven = true, "next") : "locked";
            else if (t.kind == "skirmish")
                v.mark = opened ? "skirmish" : "locked";
            v.open = v.mark == "next" || v.mark == "skirmish" || v.mark == "cleared";
            // 2. A skirmish names its current twist.
            if (t.kind == "skirmish")
                for (const StageDef &s : stages)
                    if (s.id == t.stage)
                    {
                        const int refill = save.rooms.contains(t.id) ? save.rooms.at(t.id).refill : 0;
                        v.detail = refill > 0 && !s.twists.empty() ? s.twists[static_cast<std::size_t>(std::min<int>(refill, static_cast<int>(s.twists.size())) - 1)].name : s.title;
                    }
            out.push_back(v);
        }
        return out;
    }

    std::string Nearest(const TheatreMap &map, const std::string &from, char direction)
    {
        const auto it = std::ranges::find(map.tickets, from, &Ticket::id);
        if (it == map.tickets.end())
            return {};
        // 1. Tickets on that side, scored by distance with the sideways part weighed double.
        std::string best;
        float bestScore = 1e9F;
        for (const Ticket &t : map.tickets)
        {
            const float dx = t.x - it->x, dy = t.y - it->y;
            const float along = direction == 'r' ? dx : direction == 'l' ? -dx : direction == 'd' ? dy : -dy;
            const float side = direction == 'r' || direction == 'l' ? std::abs(dy) : std::abs(dx);
            if (t.id == from || along <= 0.0F)
                continue;
            if (const float score = along + 2.0F * side; score < bestScore)
                best = t.id, bestScore = score;
        }
        return best;
    }
}
