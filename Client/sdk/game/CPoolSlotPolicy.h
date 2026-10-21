/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's pool slot bounds and free-slot selection across engines
 *****************************************************************************/
#pragma once

#include <cstdint>

namespace MtaGame
{
    class CPoolSlotPolicy final
    {
    public:
        static constexpr std::uint32_t InvalidSlot = 0xffffffffu;

        static constexpr bool ContainsIndex(std::int32_t count, std::int32_t index) noexcept { return count > 0 && index >= 0 && index < count; }

        static constexpr bool ContainsIndex(std::int32_t count, std::uint32_t index) noexcept { return count > 0 && index < static_cast<std::uint32_t>(count); }

        static constexpr std::int32_t IndexOrInvalid(std::int32_t count, std::uint32_t index) noexcept
        {
            return ContainsIndex(count, index) ? static_cast<std::int32_t>(index) : -1;
        }

        template <typename EmptyProbe>
        static std::uint32_t FindFreeAfter(std::int32_t count, std::int32_t cursor, EmptyProbe&& isEmpty)
        {
            if (count <= 0)
                return InvalidSlot;

            // Preserve classic GetFreeSlot's probe order, including its final
            // full scan after wrapping. The caller retains storage/cursor and
            // allocation ownership; selecting an empty slot does not reserve it.
            // Comparing before addition also gives a defined clamp for an
            // invalid INT_MAX cursor without overflowing signed arithmetic.
            std::int32_t index = cursor >= -1 && cursor < count - 1 ? cursor + 1 : 0;
            bool         looped = false;
            for (;;)
            {
                if (index >= count)
                {
                    if (looped)
                        return InvalidSlot;
                    index = 0;
                    looped = true;
                }
                if (isEmpty(index))
                    return static_cast<std::uint32_t>(index);
                ++index;
            }
        }
    };
}
