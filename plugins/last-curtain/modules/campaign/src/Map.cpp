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

    PlanBox Plan(const MapLens &lens, float areaW, float areaH, float aspect)
    {
        // 1. The plan's size: its width fits the area at zoom 1.
        const float w = areaW * lens.zoom, h = w / aspect;
        // 2. Where: centred on an axis it doesn't fill; else at the lens, kept so no edge comes inside the area.
        const float x = w <= areaW ? (areaW - w) * 0.5F : std::clamp(lens.x, areaW - w, 0.0F);
        const float y = h <= areaH ? (areaH - h) * 0.5F : std::clamp(lens.y, areaH - h, 0.0F);
        return {x, y, w, h};
    }

    void Drag(MapLens &lens, glm::vec2 by, float areaW, float areaH, float aspect)
    {
        const PlanBox b = Plan(lens, areaW, areaH, aspect);
        lens.x = b.x + by.x, lens.y = b.y + by.y;
        const PlanBox kept = Plan(lens, areaW, areaH, aspect);
        lens.x = kept.x, lens.y = kept.y;
    }

    void Zoom(MapLens &lens, float steps, glm::vec2 about, float areaW, float areaH, float aspect)
    {
        // The point under `about` stays under it: the plan grows around it.
        const PlanBox before = Plan(lens, areaW, areaH, aspect);
        const float zoom = std::clamp(lens.zoom * std::pow(1.25F, steps), 1.0F, 3.0F), k = zoom / lens.zoom;
        lens.zoom = zoom;
        lens.x = about.x - (about.x - before.x) * k, lens.y = about.y - (about.y - before.y) * k;
        const PlanBox kept = Plan(lens, areaW, areaH, aspect);
        lens.x = kept.x, lens.y = kept.y;
    }
}
