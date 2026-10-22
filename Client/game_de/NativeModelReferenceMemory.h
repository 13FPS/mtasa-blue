/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One immediate interior adjustment in an existing native model row
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de::NativeModelReferenceMemory
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    using Exchange = ClockResult (*)(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t root = 0, map = 0, row = 0;
        std::uint32_t  modelIndex = 0, before = 0, target = 0, observed = 0, exceptionCode = 0;
        bool           entered = false, accepted = false;
    };
    // This is the interior DWORD operation shared by the native assignment and
    // destruction paths. It cannot create a map entry, reach zero, run the
    // destructor's removal continuation, or hand out a retained model lease.
    // An entered failure requires the owning runtime to stop without replay;
    // accepted native effects are never undone by a bookkeeping rollback.
    ClockResult Adjust(std::uintptr_t image, std::uint32_t modelIndex, int delta, CheckOwner, Receipt* = nullptr, Exchange = nullptr) noexcept;
}
