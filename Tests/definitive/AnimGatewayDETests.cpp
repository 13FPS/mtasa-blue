/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA gateway equality through one raw hierarchy observation
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include <game/CAnimBlock.h>
#include <game/CAnimBlendHierarchy.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <game/CAnimBlendSequence.h>
#include <game/CAnimGatewayNames.h>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "AnimationGatewayFixture.generated.h"
using namespace mta::game_de;
namespace GatewaySdkTests
{
    // Independent raw CRC32 vector for uppercase RUN_WUZI, seedffffffff and
    // no final XOR. This is not recomputed by the method under test.
    constexpr std::uint32_t gatewayHash = 0x00dbb691;
    unsigned                checks = 0;
    void                    Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL %u: %s\n", checks, reason);
            std::exit(1);
        }
    }
    struct Owner final : NativeAnimationManagerAccess
    {
        std::uint64_t                    generation = 0xfedcba9876543210ull, receivedGeneration = 0;
        std::uintptr_t                   allowedLocator = (std::numeric_limits<std::uintptr_t>::max)() - 63, receivedLocator = 0;
        std::uint32_t                    offeredHash = gatewayHash;
        unsigned                         observations = 0, unexpectedQueries = 0;
        ClockResult                      result = ClockResult::Ok;
        CAnimBlendHierarchySAInterface** callerPointer = nullptr;
        ClockResult                      ReadAnimationHierarchyHash(std::uint64_t value, std::uintptr_t locator, std::uint32_t& output) noexcept override
        {
            ++observations;
            receivedGeneration = value;
            receivedLocator = locator;
            output = offeredHash;
            if (callerPointer)
                *callerPointer = nullptr;
            if (value != generation || !value)
                return ClockResult::OwnerConflict;
            if (!locator || locator != allowedLocator)
                return ClockResult::InvalidArgument;
            return result;
        }
        ClockResult ReadAnimationManagerCounts(std::uint64_t, NativeAnimationManagerCounts&) noexcept override
        {
            ++unexpectedQueries;
            return ClockResult::Stopped;
        }
        ClockResult ReadAnimationBlockIdentity(std::uint64_t, std::uint32_t, NativeAnimationBlockIdentity&) noexcept override
        {
            ++unexpectedQueries;
            return ClockResult::Stopped;
        }
        ClockResult FindAnimationBlockIndex(std::uint64_t, const NativeAnimationBlockName&, std::int32_t&) noexcept override
        {
            ++unexpectedQueries;
            return ClockResult::Stopped;
        }
        ClockResult ReadAnimationBlockCounterIdentity(std::uint64_t, std::uint32_t, NativeAnimationBlockIdentity&) noexcept override
        {
            ++unexpectedQueries;
            return ClockResult::Stopped;
        }
    };
    template <class Call>
    void Refuses(Owner& owner, Call call, ClockResult status)
    {
        const auto before = owner.observations;
        bool       refused = false;
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text = error.what(), suffix = std::string(": ") + std::to_string(static_cast<unsigned>(status));
            refused = text.find("isGateWayAnimationHierarchy") != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused && owner.observations == before + 1 && owner.unexpectedQueries == 0,
              "refusal remains an exact contextual exception, not a false mismatch, retry, name query or fabricated observation");
    }
}
int main()
{
    using namespace GatewaySdkTests;
    static_assert(std::is_abstract_v<CAnimManagerDE>);
    static_assert(std::is_same_v<decltype(&NativeAnimationManagerAccess::ReadAnimationHierarchyHash),
                                 ClockResult (NativeAnimationManagerAccess::*)(std::uint64_t, std::uintptr_t, std::uint32_t&) noexcept>);
    Check(std::setlocale(LC_CTYPE, "C") != nullptr, "owned test process uses the C locale for its independent known CRC vector");
    Check(std::strcmp(MtaGame::CAnimGatewayNames::GetAnimationName(), "run_wuzi") == 0, "the comparison uses MTA's exact authored gateway animation literal");
    NativeAnimationManagerAccess unavailable;
    std::uint32_t                untouched = 0x12345678;
    Check(unavailable.ReadAnimationHierarchyHash(7, 1, untouched) == ClockResult::NotInitialized && untouched == 0x12345678,
          "optional hash access defaults to refusal without inventing a field value");
    Owner                    owner;
    auto                     suppliedGeneration = owner.generation;
    AnimationGatewayReceiver receiver(owner, suppliedGeneration);
    CAnimManager&            sdk = receiver;
    suppliedGeneration = 0;
    Check(owner.observations == 0 && owner.unexpectedQueries == 0, "observation-only CPU construction does not need a block, CGame, factory or hash state");
    for (const auto locator : {std::uintptr_t{1}, std::uintptr_t{0x20000}, (std::numeric_limits<std::uintptr_t>::max)() - 63})
    {
        owner.allowedLocator = locator;
        const auto before = owner.observations;
        Check(sdk.isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(locator)) && owner.observations == before + 1 &&
                  owner.receivedLocator == locator && owner.receivedGeneration == owner.generation,
              "actual SDK compares the accepted hash and forwards each full-width opaque locator and copied generation once without pointer dereference");
    }
    auto* caller = reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.allowedLocator);
    owner.callerPointer = &caller;
    Check(sdk.isGateWayAnimationHierarchy(caller) && caller == nullptr && owner.receivedLocator == owner.allowedLocator,
          "caller pointer mutation during service observation does not change its copied locator");
    owner.callerPointer = nullptr;
    for (const auto mismatch : {std::uint32_t{0}, gatewayHash ^ 1u, std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
    {
        owner.offeredHash = mismatch;
        const auto before = owner.observations;
        Check(!sdk.isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.allowedLocator)) && owner.observations == before + 1,
              "all successful nonmatching raw32 hash values return false without normalization or enum filtering");
    }
    owner.offeredHash = gatewayHash;
    Refuses(owner, [&] { sdk.isGateWayAnimationHierarchy(nullptr); }, ClockResult::InvalidArgument);
    for (const auto result :
         {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::WriteFailure,
          ClockResult::Busy, ClockResult::Expired, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
    {
        owner.result = result;
        Refuses(owner, [&] { sdk.isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.allowedLocator)); }, result);
    }
    owner.result = ClockResult::Ok;
    ++owner.generation;
    Refuses(
        owner, [&] { sdk.isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.allowedLocator)); }, ClockResult::OwnerConflict);
    Check(owner.receivedGeneration + 1 == owner.generation, "a stale facade does not silently renew its generation");
    AnimationGatewayReceiver zero(owner, 0);
    Refuses(
        owner, [&] { zero.isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.allowedLocator)); }, ClockResult::OwnerConflict);
    AnimationGatewayReceiver fresh(owner, owner.generation);
    Check(fresh.isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.allowedLocator)),
          "a fresh explicit observer uses the newly admitted generation");
    AnimationGatewayReceiver missing(unavailable, 7);
    bool                     missingRefused = false;
    try
    {
        missing.isGateWayAnimationHierarchy(reinterpret_cast<CAnimBlendHierarchySAInterface*>(std::uintptr_t{1}));
    }
    catch (const std::runtime_error& error)
    {
        missingRefused = std::string(error.what()).find("isGateWayAnimationHierarchy") != std::string::npos;
    }
    Check(missingRefused, "an absent hash service cannot masquerade as a valid false comparison");
    const auto beforeDestroy = owner.observations;
    {
        AnimationGatewayReceiver temporary(owner, owner.generation);
    }
    Check(owner.observations == beforeDestroy && owner.unexpectedQueries == 0,
          "exact CPU destruction introduces no hierarchy lease or metadata/hash operation");
    std::puts("Actual MTA gateway hierarchy hash SDK fixture completed");
    std::printf("Passed %u animation gateway SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
}
