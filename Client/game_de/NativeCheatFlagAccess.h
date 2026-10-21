/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked borrowed read/write access to four real native cheat flags
 *****************************************************************************/
#pragma once
#include "NativeCheatProfile.h"
#include <limits>
#include <windows.h>
#include <intrin.h>

namespace mta::game_de
{
    class NativeCheatFlagAccess final
    {
    public:
        using Flag = NativeCheatProfile::Flag;
        using PageCheck = bool (*)(std::uintptr_t, std::size_t, bool, bool) noexcept;
        enum class WriteResult
        {
            Refused,
            Conflict,
            Unchanged,
            Written,
            Unconfirmed
        };

        // The caller supplies an already-qualified binding and retains the
        // synchronous engine thread/generation scope. Page access is neither
        // that authority nor permission to reset the full native cheat array.
        NativeCheatFlagAccess(const NativeCheatProfile::Binding& qualified, PageCheck pageCheck) noexcept : binding(qualified), pages(pageCheck) {}

        bool Read(Flag flag, bool& output) const noexcept
        {
            std::uintptr_t target = 0;
            unsigned char  value = 0;
            SIZE_T         copied = 0;
            if (!Range(flag, false, target) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(target), &value, 1, &copied) ||
                copied != 1 || value > 1)
                return false;
            output = value != 0;
            return true;
        }

        bool ReadWorld(MtaGame::CWorldCheatPolicy::Id id, bool& output) const noexcept
        {
            const auto*   member = NativeCheatProfile::FindRead(id);
            unsigned char value = 0;
            SIZE_T        copied = 0;
            if (!member || !binding.image || binding.image > (std::numeric_limits<std::uintptr_t>::max)() - NativeCheatProfile::ImageSize ||
                binding.flags != binding.image + NativeCheatProfile::FlagsRva || member->nativeIndex >= NativeCheatProfile::NativeFlagCount)
                return false;
            const auto target = binding.flags + member->nativeIndex;
            if (!pages || !pages(target, 1, false, false) ||
                !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(target), &value, 1, &copied) || copied != 1)
                return false;
            // Classic IsCheatEnabled observes the actual raw byte !=0. The
            // strict binary read/CAS used by the four setters stays unchanged.
            output = value != 0;
            return true;
        }

        WriteResult WriteIf(Flag flag, bool expected, bool replacement) const noexcept
        {
            std::uintptr_t target = 0;
            char           observed = 0;
            if (!Range(flag, true, target) || !CompareExchange(target, static_cast<char>(expected), static_cast<char>(replacement), observed))
                return WriteResult::Refused;
            // Cleanup uses the same comparison against its last owned value.
            // An intervening foreign edit must survive a stale restore attempt.
            if (observed != static_cast<char>(expected))
                return WriteResult::Conflict;
            bool readback = false;
            if (!Read(flag, readback) || readback != replacement)
                return WriteResult::Unconfirmed;
            return expected == replacement ? WriteResult::Unchanged : WriteResult::Written;
        }

    private:
        bool Range(Flag flag, bool writable, std::uintptr_t& output) const noexcept
        {
            const auto index = static_cast<std::size_t>(flag);
            if (index >= NativeCheatProfile::Members.size() || !binding.image ||
                binding.image > (std::numeric_limits<std::uintptr_t>::max)() - NativeCheatProfile::ImageSize ||
                binding.flags != binding.image + NativeCheatProfile::FlagsRva)
                return false;
            const auto nativeIndex = NativeCheatProfile::Members[index].nativeIndex;
            static_assert(NativeCheatProfile::FlagsRva + NativeCheatProfile::NativeFlagCount <= NativeCheatProfile::ImageSize);
            if (nativeIndex >= NativeCheatProfile::NativeFlagCount)
                return false;
            const auto target = binding.flags + nativeIndex;
            if (!pages || !pages(target, 1, writable, false))
                return false;
            output = target;
            return true;
        }

        static bool CompareExchange(std::uintptr_t target, char expected, char replacement, char& observed) noexcept
        {
            // A protection change after the borrowed page policy must refuse,
            // not crash GTA. The one-byte CAS prevents read/write races from
            // overwriting a value that no longer belongs to the caller.
            __try
            {
                observed = _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(target), replacement, expected);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
        const NativeCheatProfile::Binding binding;
        const PageCheck                   pages;
    };
}
