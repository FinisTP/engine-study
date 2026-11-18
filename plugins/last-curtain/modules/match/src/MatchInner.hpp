#pragma once

#include <cstddef>
#include <cstdint>

// Small helpers shared by the match's source files.
namespace umbra::match::inner
{
    inline std::size_t U(int i) { return static_cast<std::size_t>(i); }

    // splitmix64: everything random follows from one 64-bit state.
    struct Rng
    {
        std::uint64_t s = 0;
        std::uint64_t Next()
        {
            std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }
        int Below(int n) { return n <= 0 ? 0 : static_cast<int>(Next() % static_cast<std::uint64_t>(n)); }
    };
}
