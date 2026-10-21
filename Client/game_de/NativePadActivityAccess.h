/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve CPad's qualified native last-input timestamp contract
 *****************************************************************************/
#pragma once

#include "NativePadActivityProfile.h"
#include <cstdint>
#include <limits>
#include <windows.h>

namespace mta::game_de
{
    // This borrows the same synchronous engine/thread/actor admission as state
    // access. A writable address alone cannot admit a pad or make the enclosing
    // queues, horn history, rumble and remote-player context safe to replay.
    class NativePadActivityAccess final
    {
    public:
        using PageCheck = bool (*)(std::uintptr_t, std::size_t, bool, bool) noexcept;

        NativePadActivityAccess(std::uintptr_t padAddress, PageCheck pageCheck) noexcept : address(padAddress), pages(pageCheck) {}

        bool ReadLastTimeTouched(std::uint32_t& output) const noexcept
        {
            std::uintptr_t target = 0;
            std::uint32_t  next = 0;
            SIZE_T         copied = 0;
            if (!Range(false, target) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(target), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }

        bool SetLastTimeTouched(std::uint32_t time) const noexcept
        {
            std::uintptr_t target = 0;
            SIZE_T         copied = 0;
            if (!Range(true, target) || !WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(target), &time, sizeof(time), &copied) ||
                copied != sizeof(time))
                return false;

            // Native code updates this field as a DWORD from the game timer.
            // Preserve wraparound and require observed bytes; a failed readback
            // must not fabricate success or overwrite a subsequent native edit.
            std::uint32_t observed = 0;
            return ReadLastTimeTouched(observed) && observed == time;
        }

    private:
        bool Range(bool writable, std::uintptr_t& target) const noexcept
        {
            static_assert(NativePadActivityProfile::FieldSize == sizeof(std::uint32_t));
            static_assert(NativePadActivityProfile::FieldOffset + NativePadActivityProfile::FieldSize <= NativePadActivityProfile::PadStride);
            if (!address || address > (std::numeric_limits<std::uintptr_t>::max)() - NativePadActivityProfile::PadStride)
                return false;
            target = address + NativePadActivityProfile::FieldOffset;
            return pages && pages(target, NativePadActivityProfile::FieldSize, writable, false);
        }

        const std::uintptr_t address;
        const PageCheck      pages;
    };
}
