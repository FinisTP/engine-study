#include <EditorPanel.hpp>
#include <Soak.hpp>

#include <imgui.h>

#include <algorithm>

// "Town plot" window (S24): the project's outcome plan at a glance - beats, their windows and
// outcomes, what is never allowed, the pacing - with the plan's mistakes, and a soak test at a
// button. The plan is written by hand in `town/*.plan.json`; this window reads it.
// Storyteller.md "In the editor".

using namespace umbra::town;

namespace
{
    std::string Words(const TownDef &def, const Pattern &p)
    {
        const auto name = [&](int resident) { return resident < 0 ? std::string("anyone") : def.residents[static_cast<std::size_t>(resident)].name; };
        std::string text = std::string(KindName(p.kind)) + ": " + name(p.who);
        if (p.kind == TroubleKind::Theft)
            text += " from " + name(p.other);
        if (p.kind == TroubleKind::Feud)
            text += " with " + name(p.other);
        if (p.from > 1 || p.to < noLastDay)
            text += " (days " + std::to_string(p.from) + "-" + (p.to < noLastDay ? std::to_string(p.to) : std::string("end")) + ")";
        return text;
    }
    std::string Words(const TownDef &def, const std::vector<Pattern> &patterns)
    {
        std::string text;
        for (const Pattern &p : patterns)
            text += (text.empty() ? "" : "\n") + Words(def, p);
        return text.empty() ? "-" : text;
    }

    class TownPlotPanel final : public umbra::editor::EditorPanel
    {
    public:
        const char *Title() const override { return "Town plot"; }
        void Draw(umbra::editor::EditorHost &host) override;

    private:
        void Load(umbra::editor::EditorHost &host);

        bool loaded_ = false, ok_ = false;
        TownDef def_;
        PlanDef plan_;
        std::vector<umbra::core::Diagnostic> problems_;
        std::string file_, report_;
        int seeds_ = 200;
    };

    void TownPlotPanel::Load(umbra::editor::EditorHost &host)
    {
        // The first town file by name and the plan beside it, as the game does.
        loaded_ = true, ok_ = false;
        problems_.clear(), report_.clear();
        std::vector<std::filesystem::path> files;
        std::error_code ignored;
        for (const auto &entry : std::filesystem::directory_iterator(host.AssetRoot().Project() / "town", ignored))
            if (entry.path().filename().string().ends_with(".town.json"))
                files.push_back(entry.path());
        if (files.empty())
        {
            file_ = "no town/*.town.json in this project";
            return;
        }
        std::sort(files.begin(), files.end());
        std::filesystem::path plan = files.front();
        plan.replace_extension().replace_extension(".plan.json");
        file_ = plan.filename().string();
        ok_ = LoadTown(files.front(), nullptr, def_, problems_) && LoadPlan(plan, def_, plan_, problems_);
    }

    void TownPlotPanel::Draw(umbra::editor::EditorHost &host)
    {
        if (!loaded_)
            Load(host);
        ImGui::SetNextWindowSize(ImVec2(620, 520), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(Title()))
            return ImGui::End();
        ImGui::TextUnformatted(file_.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Reload"))
            Load(host);
        for (const umbra::core::Diagnostic &d : problems_)
            ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s%s%s", d.field.c_str(), d.field.empty() ? "" : ": ", d.message.c_str());
        if (!ok_)
            return ImGui::End();

        // 1. The beats in story order.
        if (ImGui::BeginTable("beats", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Beat");
            ImGui::TableSetupColumn("Days");
            ImGui::TableSetupColumn("Reached by any of");
            ImGui::TableSetupColumn("Forbidden meanwhile");
            ImGui::TableHeadersRow();
            for (const BeatDef &beat : plan_.beats)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s\n(%s)", beat.name.c_str(), beat.id.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%d-%d", beat.from, beat.to);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(Words(def_, beat.outcomes).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(Words(def_, beat.forbid).c_str());
            }
            ImGui::EndTable();
        }
        // 2. The rules around them.
        ImGui::SeparatorText("Never");
        ImGui::TextUnformatted(Words(def_, plan_.never).c_str());
        ImGui::SeparatorText("May leave or die (besides the beats)");
        ImGui::TextUnformatted(Words(def_, plan_.allow).c_str());
        ImGui::SeparatorText("Pacing");
        ImGui::Text("%d quiet day(s) after trouble; at most %d ill at once; at most %d gone; looks %d day(s) ahead; acts at %02d:00", plan_.quietDays, plan_.maxIll,
                    plan_.maxGone, plan_.lookAhead, plan_.hour);

        // 3. The soak test: the plan on many seeds (a second or so per thousand).
        ImGui::SeparatorText("Soak test");
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("playthroughs", &seeds_, 100);
        seeds_ = std::clamp(seeds_, 10, 5000);
        ImGui::SameLine();
        if (ImGui::Button("Run"))
        {
            int days = 7;
            for (const BeatDef &beat : plan_.beats)
                days = std::max(days, beat.to + 1);
            const SoakReport report = Soak(def_, plan_, 1, seeds_, days);
            report_ = Text(plan_, report) + (report.Holds() ? "the plan holds" : "THE PLAN DOES NOT HOLD");
        }
        if (!report_.empty())
            ImGui::TextUnformatted(report_.c_str());
        ImGui::End();
    }
}

UMBRA_EDITOR_PANEL(TownPlotPanel);
