#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace umbra::core
{
    // Holds objects until the GPU can no longer be using them, then destroys them.
    // Each object is tagged with the frame serial at which it was retired.
    // See engine/render/src/ResourceLifetime.md for how the serials are chosen.
    template <typename T>
    class RetireQueue final
    {
    public:
        void Retire(T value, std::uint64_t serial) { pending_.push_back({serial, std::move(value)}); }

        // Destroys every object retired at or before `completedSerial`. Returns how many.
        std::size_t Collect(std::uint64_t completedSerial)
        {
            return std::erase_if(pending_, [&](const Entry &entry) { return entry.serial <= completedSerial; });
        }

        // Destroys everything. Only call when the GPU is idle.
        void Flush() { pending_.clear(); }

        [[nodiscard]] std::size_t Size() const noexcept { return pending_.size(); }

    private:
        struct Entry final
        {
            std::uint64_t serial = 0;
            T value;
        };

        std::vector<Entry> pending_;
    };
}
