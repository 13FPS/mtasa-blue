/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Closed-origin dispatch into the existing issued association storage
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationClumpTypes.h"
#include <cstdint>
namespace mta::game_de
{
    class NativeAnimationAssociationStorage;
    class NativeAnimationAssociationClumpOrigin;
    namespace NativeAnimationAssociationCalls
    {
        // Only the real owner bridge can construct the synchronous origin.
        // Public raw clump/H arguments do not grant a gate or receiver lease.
        ClockResult ApplyClump(std::uintptr_t, NativeAnimationAssociationStorage&, std::uint64_t, NativeAnimationAssociationClumpOperation,
                               const NativeAnimationAssociationClumpOrigin&, ClockResult (*)() noexcept, std::uintptr_t&, ClumpReceipt* = nullptr,
                               const ClumpInvocations* = nullptr) noexcept;
    }
}
