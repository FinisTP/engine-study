#include "StageGuide.hpp"

#include <algorithm>

std::vector<MatchPlay::GuideStep> GuideFor(const umbra::campaign::StageDef &stage, const umbra::campaign::BuiltStage &built)
{
    const auto unitOf = [&](const std::string &who) {
        const auto it = std::ranges::find(built.who, who);
        return it == built.who.end() ? -1 : static_cast<int>(it - built.who.begin());
    };
    std::vector<MatchPlay::GuideStep> steps;
    for (const umbra::campaign::GuideDef &g : stage.guide)
    {
        MatchPlay::GuideStep step;
        step.unit = unitOf(g.unit), step.round = g.round, step.when = g.when, step.act = g.act, step.pick = g.pick, step.tip = g.tip;
        step.cell = g.x >= 0 ? g.y * stage.width + g.x : -1;
        step.with = g.move.starts_with("with:") ? unitOf(g.move.substr(5)) : -1;
        step.toward = g.move == "toward", step.safe = g.move == "safe";
        step.target = g.target.empty() ? -1 : unitOf(g.target);
        steps.push_back(step);
    }
    return steps;
}
