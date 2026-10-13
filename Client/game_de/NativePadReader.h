/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read a build-pinned DE pad without changing engine input
 *****************************************************************************/

#pragma once

#include <CControllerState.h>
#include "../sdk/game/CGameAdapter.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mta::game_de
{
    // Keep the observed engine bytes separate from MTA's synthesized actions.
    // Requiring the complete selected stride prevents a partial pad allocation
    // from being accepted merely because its leading controller fields fit.
    inline bool CopyNativePadStates(const void* bytes, std::size_t byteCount, std::uint32_t padIndex, CControllerState& current,
                                    CControllerState& previous) noexcept
    {
        constexpr std::size_t stride = 0x158;
        static_assert(sizeof(CControllerState) == 0x30);
        if (!bytes || padIndex >= 2 || byteCount < (static_cast<std::size_t>(padIndex) + 1) * stride)
            return false;

        CControllerState nextCurrent;
        CControllerState nextPrevious;
        const auto*      selected = static_cast<const unsigned char*>(bytes) + static_cast<std::size_t>(padIndex) * stride;
        std::memcpy(&nextCurrent, selected, sizeof(nextCurrent));
        std::memcpy(&nextPrevious, selected + 0x30, sizeof(nextPrevious));
        current = nextCurrent;
        previous = nextPrevious;
        return true;
    }

    class NativePadReader final
    {
    public:
        bool Bind() noexcept;
        bool Read(std::uint32_t padIndex, CControllerState& current, CControllerState& previous) const noexcept;
        bool ReadDeviceState(NativeDeviceState& state) const noexcept;
        void Reset() noexcept;
        bool IsBound() const noexcept;

    private:
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uintptr_t> padAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
