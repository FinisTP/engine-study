#pragma once

#include <Campaign.hpp>
#include <MatchPlay.hpp>

// A stage's guided steps for the fight (Campaign.md "Guided steps"): cast ids become the fight's
// units, [x, y] its squares, "with:<id>", "toward" and "safe" their flags.
std::vector<MatchPlay::GuideStep> GuideFor(const umbra::campaign::StageDef &stage, const umbra::campaign::BuiltStage &built);
