/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: IFP-local bounds without changing other CFileReader consumers
 ****************************************************************************/
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace MtaGame::IfpRead
{
    inline void RequireBytes(std::size_t bytes, std::size_t remaining)
    {
        if (bytes > remaining || bytes > (std::numeric_limits<std::uint32_t>::max)())
            throw std::runtime_error("Truncated or oversized IFP data");
    }
    inline std::size_t Product(std::size_t count, std::size_t stride)
    {
        if (stride && count > (std::numeric_limits<std::uint32_t>::max)() / stride)
            throw std::runtime_error("IFP frame byte size is not representable");
        return count * stride;
    }
    inline std::size_t SequenceCapacity(std::int32_t input)
    {
        // All64 known/dummy bones coexist with every possible unknown input
        // row. Keep native allocation capacity independent of the WORD count.
        if (input < 0 || input > (std::numeric_limits<std::uint16_t>::max)() - 64)
            throw std::runtime_error("IFP sequence count is not representable");
        return 64u + static_cast<std::size_t>(input);
    }
    constexpr std::uint32_t Padded(std::uint32_t size)
    {
        const auto padding = (4u - (size & 3u)) & 3u;
        if (size > (std::numeric_limits<std::uint32_t>::max)() - padding)
            throw std::runtime_error("IFP chunk padding overflows");
        return size + padding;
    }
}
