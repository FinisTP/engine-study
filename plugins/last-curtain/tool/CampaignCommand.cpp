// UmbraTool campaign-soak: a campaign stage played by bots many times (Campaign.md "The soak").

#include <ToolCommand.hpp>

#include <CampaignSoak.hpp>

#include <fstream>
#include <iostream>

namespace
{
    namespace campaign = umbra::campaign;

    int CampaignSoak(const umbra::tool::Arguments &args)
    {
        // 1. The data: the match's rules, the cast, the growth, the stage and its bands.
        const std::filesystem::path folder = args.Has("data") ? args.Path("data") : std::filesystem::path(UMBRA_CAMPAIGN_DEFAULT_DATA);
        const std::string id = args.positional.empty() ? std::string() : args.positional.front();
        std::vector<umbra::core::Diagnostic> d;
        umbra::match::MatchDef def;
        campaign::Cast cast;
        campaign::Growth growth;
        campaign::StageDef stage;
        if (id.empty() || !umbra::match::LoadMatch(folder / "../match", def, d) || !campaign::LoadCast(folder / "cast.csv", cast, d) ||
            !campaign::LoadGrowth(folder / "growth.csv", growth, d) || !campaign::LoadStage(folder / "stages" / (id + ".json"), cast, def, folder / "scenes", stage, d))
        {
            if (id.empty())
                std::cerr << "Which stage? For example: campaign-soak stage1\n";
            return umbra::tool::Report(d, 11, args.json);
        }
        nlohmann::json bands;
        if (std::ifstream in(folder / "balance.json"); in)
            bands = nlohmann::json::parse(in, nullptr, false);
        // 2. The runs, the report, the bands for this stage (and refill).
        const int refill = args.Int("refill", 0);
        const campaign::StageReport r = campaign::SoakStage(def, cast, growth, stage, refill, static_cast<std::uint64_t>(args.Long("seed", 1)), args.Int("seeds", 200));
        std::cout << campaign::Text(stage, r);
        const std::string key = refill > 0 ? id + "/" + std::to_string(refill) : id;
        const nlohmann::json band = bands.is_object() && bands.contains(key) ? bands[key] : bands.is_object() && bands.contains(id) ? bands[id] : nlohmann::json::object();
        const std::vector<std::string> missed = campaign::Missed(r, band);
        for (const std::string &m : missed)
            std::cout << "  MISSED " << m << "\n";
        std::cout << (missed.empty() ? "the stage holds\n" : "THE STAGE DOES NOT HOLD\n");
        return missed.empty() ? 0 : 1;
    }
}

UMBRA_TOOL_COMMAND(CampaignSoak, "campaign-soak", "campaign-soak <stage> [--seeds 200] [--refill n] [--seed 1] [--data <campaign folder>]", CampaignSoak);
