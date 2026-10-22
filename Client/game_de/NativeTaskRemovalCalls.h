/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: A bounded genuine native secondary-task deletion transaction
 ****************************************************************************/
#pragma once
#include "NativePedIntelligenceReader.h"
#include "NativeTaskPoolReader.h"

namespace mta::game_de::NativeTaskRemovalCalls
{
    using CheckOwner = NativePedPoolReader::CheckOwner;
    // Win64: RCX is the embedded manager, RDX is a null replacement, and R8D
    // carries the exact signed SDK index. No SDK wrapper is a native argument.
    using NativeFunction = void (*)(std::uintptr_t, std::uintptr_t, std::int32_t);
    using Invoke = ClockResult (*)(NativeFunction, std::uintptr_t, std::int32_t, std::uint32_t&) noexcept;
    struct Receipt
    {
        std::uintptr_t entryRva = 0, nativeIntelligence = 0, manager = 0, oldTask = 0;
        std::int32_t   index = -1;
        std::uint32_t  oldTaskSlot = 0, oldTaskType = 0, exceptionCode = 0;
        unsigned char  oldTaskGeneration = 0;
        bool           entered = false, returned = false, alreadyEmpty = false;
    };

    // Admits only an empty cell or the two exact already-qualified classes.
    // Complex305 must have no child, and the task cannot alias another manager
    // cell. This is a synchronous destructive intent, never a task lease.
    // An entered failure MUST stop the enclosing owner: deletion may already
    // have happened, so neither retry nor manager/task snapshot restoration is
    // valid. The owner must recheck its retained ped token after success.
    ClockResult RemoveSecondary(std::uintptr_t image, const NativePedPoolReader::Value& ped, int index, CheckOwner, Receipt* = nullptr,
                                Invoke = nullptr) noexcept;
}
