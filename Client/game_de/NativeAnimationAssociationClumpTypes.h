/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact source operations and observations for private clump Init
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <array>
#include <cstddef>
#include <cstdint>
namespace mta::game_de
{
    enum class NativeAnimationAssociationClumpOperation : std::uint32_t
    {
        Construct,
        Initialize,
        InitializeCustom
    };
    namespace NativeAnimationAssociationCalls
    {
        using ClumpConstructorFunction = std::uintptr_t (*)(std::uintptr_t, std::uintptr_t, std::uintptr_t);
        using ClumpInitializeFunction = void (*)(std::uintptr_t, std::uintptr_t, std::uintptr_t);
        using InvokeClumpConstructor = ClockResult (*)(ClumpConstructorFunction, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t&,
                                                       std::uint32_t&) noexcept;
        using InvokeClumpInitialize = ClockResult (*)(ClumpInitializeFunction, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uint32_t&) noexcept;
        using ExchangeClumpDefault = ClockResult (*)(std::uintptr_t, std::size_t, std::uint64_t, std::uint64_t, std::uint64_t&, bool&, std::uint32_t&) noexcept;
        struct ClumpInvocations
        {
            InvokeClumpConstructor construct = nullptr;
            InvokeClumpInitialize  initialize = nullptr;
            ExchangeClumpDefault   defaults = nullptr;
        };
        struct ClumpDefaultReceipt
        {
            std::uintptr_t address = 0;
            std::uint64_t  before = 0, target = 0, observed = 0;
            std::size_t    width = 0;
            bool           attempted = false, returned = false, accepted = false, verified = false;
        };
        // Fixed observations never authorize adoption or release. Complete
        // maps, allocations and hierarchy dependencies stay in actual storage.
        struct ClumpReceipt
        {
            std::uint64_t                            receiverGeneration = 0, receiverSerial = 0, command = 0;
            NativeAnimationAssociationClumpOperation operation{NativeAnimationAssociationClumpOperation::Construct};
            std::uintptr_t                           entryRva = 0, receiver = 0, nativePed = 0, clump = 0, container = 0, hierarchy = 0;
            std::uintptr_t                           priorNodes = 0, nodes = 0, nodeRaw = 0, heap = 0;
            std::size_t                              nodeRequestBytes = 0;
            std::uint16_t                            priorNodeCount = 0, nodeCount = 0;
            std::uint32_t                            exceptionCode = 0, defaultsAttempted = 0, defaultsAccepted = 0, defaultsVerified = 0;
            std::array<ClumpDefaultReceipt, 13>      defaults{};
            bool                                     privateHierarchy = false, copyOwnedReference = false;
            bool                                     dependencyPlanned = false, dependencyHeld = false, oldDependencyReleased = false;
            bool                                     originalEntered = false, originalReturned = false, observed = false, verified = false;
            bool                                     partialAccepted = false, enteredUnknown = false, hadEffects = false, retained = false;
            ClockResult                              result{ClockResult::NotInitialized};
        };
    }
}
