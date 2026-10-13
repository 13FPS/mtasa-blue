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
    // The tested executable's native updater copies these two 48-byte blocks,
    // and its caller selects objects 0x188 apart. The public SDK's enclosing
    // CPad layout starts them too early and aliases mouse/timer data instead.
    constexpr std::size_t NativePadStride = 0x188;
    constexpr std::size_t NativePadCurrentOffset = 0x38;
    constexpr std::size_t NativePadPreviousOffset = 0x68;

    // Keep the observed engine bytes separate from MTA's synthesized actions.
    // Requiring the complete selected stride prevents a partial pad allocation
    // from being accepted merely because its leading controller fields fit.
    inline bool CopyNativePadStates(const void* bytes, std::size_t byteCount, std::uint32_t padIndex, CControllerState& current,
                                    CControllerState& previous) noexcept
    {
        static_assert(sizeof(CControllerState) == 0x30);
        static_assert(NativePadCurrentOffset + sizeof(CControllerState) == NativePadPreviousOffset);
        static_assert(NativePadPreviousOffset + sizeof(CControllerState) <= NativePadStride);
        if (!bytes || padIndex >= 2 || byteCount < (static_cast<std::size_t>(padIndex) + 1) * NativePadStride)
            return false;

        CControllerState nextCurrent;
        CControllerState nextPrevious;
        const auto*      selected = static_cast<const unsigned char*>(bytes) + static_cast<std::size_t>(padIndex) * NativePadStride;
        std::memcpy(&nextCurrent, selected + NativePadCurrentOffset, sizeof(nextCurrent));
        std::memcpy(&nextPrevious, selected + NativePadPreviousOffset, sizeof(nextPrevious));
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
