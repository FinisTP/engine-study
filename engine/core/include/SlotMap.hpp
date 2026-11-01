#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace umbra::core
{
    // Generational handle. Tag makes handles of different kinds incompatible.
    // generation 0 is never issued, so a default handle is always invalid.
    template <typename Tag>
    struct Handle final
    {
        std::uint32_t index = 0;
        std::uint32_t generation = 0;

        [[nodiscard]] explicit operator bool() const noexcept { return generation != 0; }
        friend bool operator==(const Handle &, const Handle &) = default;
    };

    // Stores values in reusable slots and hands out generational handles.
    // See SlotMap.md for how stale handles are detected.
    template <typename T, typename Tag = T>
    class SlotMap final
    {
    public:
        using HandleType = Handle<Tag>;

        HandleType Insert(T value)
        {
            // 1. Reuse a free slot, or grow the array.
            std::uint32_t index = 0;
            if (!free_.empty())
            {
                index = free_.back();
                free_.pop_back();
            }
            else
            {
                index = static_cast<std::uint32_t>(slots_.size());
                slots_.emplace_back();
            }

            // 2. Bump the generation so handles to the previous occupant stop matching.
            Slot &slot = slots_[index];
            slot.generation = slot.generation == UINT32_MAX ? 1 : slot.generation + 1;
            slot.value.emplace(std::move(value));
            ++size_;
            return {index, slot.generation};
        }

        bool Remove(HandleType handle)
        {
            if (Get(handle) == nullptr)
            {
                return false;
            }
            slots_[handle.index].value.reset();
            free_.push_back(handle.index);
            --size_;
            return true;
        }

        [[nodiscard]] const T *Get(HandleType handle) const noexcept
        {
            if (handle.index >= slots_.size())
            {
                return nullptr;
            }
            const Slot &slot = slots_[handle.index];
            return slot.generation == handle.generation && slot.value ? &*slot.value : nullptr;
        }

        [[nodiscard]] T *Get(HandleType handle) noexcept
        {
            return const_cast<T *>(std::as_const(*this).Get(handle));
        }

        [[nodiscard]] bool Contains(HandleType handle) const noexcept { return Get(handle) != nullptr; }
        [[nodiscard]] std::size_t Size() const noexcept { return size_; }

        // Removes everything but keeps generations, so all old handles stay stale.
        void Clear()
        {
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                if (slots_[index].value)
                {
                    slots_[index].value.reset();
                    free_.push_back(index);
                }
            }
            size_ = 0;
        }

        // Calls function(handle, value) for every live value, in slot order.
        template <typename Function>
        void ForEach(Function &&function)
        {
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                if (slots_[index].value)
                {
                    function(HandleType{index, slots_[index].generation}, *slots_[index].value);
                }
            }
        }

        template <typename Function>
        void ForEach(Function &&function) const
        {
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                if (slots_[index].value)
                {
                    function(HandleType{index, slots_[index].generation}, *slots_[index].value);
                }
            }
        }

    private:
        struct Slot final
        {
            std::uint32_t generation = 0;
            std::optional<T> value;
        };

        std::vector<Slot> slots_;
        std::vector<std::uint32_t> free_;
        std::size_t size_ = 0;
    };
}
