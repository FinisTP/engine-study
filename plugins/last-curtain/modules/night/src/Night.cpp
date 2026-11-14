#include "NightInner.hpp"

namespace umbra::night
{
    NightState NewNight(const NightDef &def, std::uint64_t seed)
    {
        NightState state;
        state.seed = seed;
        state.random = (seed ^ 0x9E3779B97F4A7C15ULL) | 1u;
        inner::BuildBoard(def, state);
        return state;
    }
}
