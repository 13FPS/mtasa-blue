/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe original dynamic clump results without granting ownership
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationClumpTypes.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace mta::game_de::NativeAnimationClumpInitializationPlan
{
    struct Snapshot;
}
namespace mta::game_de::NativeAnimationAssociationClumpVerification
{
    using Operation = NativeAnimationAssociationClumpOperation;
    using Bytes = std::array<unsigned char, 104>;
    struct Observation
    {
        Bytes                       row{};
        std::uintptr_t              nodes = 0, nodeRaw = 0;
        std::size_t                 requestBytes = 0;
        std::uint16_t               count = 0;
        std::vector<std::uintptr_t> sequenceMap;
    };
    bool Qualify(std::uintptr_t image) noexcept;
    // The actual owner reserves the exact frame-count map before any native
    // effects. This observation never grows it, adopts storage, or frees it;
    // only the caller's entered journal can grant resource authority.
    ClockResult Observe(std::uintptr_t image, std::uintptr_t receiver, Operation, const Bytes& before,
                        const NativeAnimationClumpInitializationPlan::Snapshot& input, Observation&) noexcept;
}
