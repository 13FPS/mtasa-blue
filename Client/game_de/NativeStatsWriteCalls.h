/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualified Win64 stat entry calls rather than bypassing native reactions
 *****************************************************************************/
#pragma once
#include "NativeStatsReader.h"
#include "NativeStatsWriteAccess.h"

namespace mta::game_de::NativeStatsWriteCalls
{
    enum class Operation : std::uint32_t
    {
        Set,
        Modify
    };
    // Promote the ushort SDK ID: the Win64 ABI does not guarantee the upper
    // bits of smaller arguments. The actual native leaf consumes its low CX;
    // the second float argument is in XMM1, independently confirmed by callers.
    using NativeFunction = void (*)(std::uint32_t, float);
    using CheckOwner = ClockResult (*)() noexcept;
    using Invoke = ClockResult (*)(NativeFunction, std::uint32_t, float, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t entryRva = 0;
        std::uint32_t  promotedIndex = 0, exceptionCode = 0;
        float          passedAmount = 0;
        bool           entered = false, returned = false;
    };

    // The retained owner must enforce its real generation/thread/callback
    // scope AND live native PLAYING9 on each CheckOwner call. This primitive
    // independently checks the actual loaded local-player identity and special
    // script inputs. State9 intent alone does not create a loaded player.
    // Completion preserves native clamping, reactions and intentional no-ops;
    // no target-equality bank store or generic snapshot rollback is performed.
    // Once entered, failures must never automatically retry this transaction.
    ClockResult Apply(const NativeStatsReader& reader, std::uintptr_t image, Operation operation, unsigned short index, float amount, CheckOwner checkOwner,
                      Receipt* receipt = nullptr, Invoke invoke = nullptr) noexcept;
    // Invoke is an internal typed ordinary-host test seam. Production callers
    // omit it and execute only the entry resolved from the qualified image.
}
