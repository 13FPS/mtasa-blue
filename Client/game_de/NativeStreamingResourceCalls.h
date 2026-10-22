/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real native non-DFF streaming requests through the current owner
 ****************************************************************************/
#pragma once
#include "NativeStreamingRequestCalls.h"
namespace mta::game_de::NativeStreamingResourceCalls
{
    using CheckOwner = NativeStreamingRequestCalls::CheckOwner;
    using NativeFunction = NativeStreamingRequestCalls::NativeFunction;
    using Invoke = NativeStreamingRequestCalls::Invoke;
    struct Receipt
    {
        std::uintptr_t entryRva = 0, root = 0, datProvider = 0;
        std::uint32_t  modelIndex = 0, flags = 0, exceptionCode = 0;
        std::int32_t   imgFileCount = 0;
        unsigned char  rawFlagsBefore = 0, rawStateBefore = 0, rawFlagsAfter = 0, rawStateAfter = 0;
        bool           entered = false, returned = false;
    };
    // The real entry keeps its own cache rebuild, category decoder, recursive
    // retry and list publication. An entered failure stops the enclosing owner;
    // it never authorizes replay, inverse list edits or flag restoration.
    ClockResult Request(std::uintptr_t, std::uint32_t, std::uint32_t, CheckOwner, Receipt* = nullptr, Invoke = nullptr) noexcept;
}
