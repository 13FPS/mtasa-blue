/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA sequence storage SDK dispatch through one explicit owner
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include <game/CAnimBlock.h>
#include <game/CAnimBlendHierarchy.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <game/CAnimBlendSequence.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "AnimationSequenceStorageFixture.generated.h"
using namespace mta::game_de;
namespace SequenceStorageSdkTests
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL %u: %s\n", checks, why);
            std::exit(1);
        }
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationSequenceStorageAccess
    {
        std::uint64_t                   currentGeneration = 0xfedcba9876543210ull, receivedGeneration = 0;
        std::uintptr_t                  issuedTag = 0x20000, receivedTag = 0;
        std::size_t                     receivedCapacity = 0;
        unsigned                        allocations = 0, frees = 0;
        ClockResult                     result = ClockResult::Ok;
        bool                            forceNull = false, forceZeroTag = false;
        std::size_t*                    callerCapacity = nullptr;
        CAnimBlendSequenceSAInterface** callerTag = nullptr;
        ClockResult                     AllocateAnimationSequences(std::uint64_t generation, std::size_t capacity, std::uintptr_t& output) noexcept override
        {
            ++allocations;
            receivedGeneration = generation;
            receivedCapacity = capacity;
            if (callerCapacity)
                *callerCapacity = 0;
            // These are opaque owned fixture tags, not a simulated engine
            // allocator or a successful complete CGame provider.
            output = forceNull ? 0 : (!capacity && !forceZeroTag ? 0 : issuedTag);
            if (!generation || generation != currentGeneration)
                return ClockResult::OwnerConflict;
            return result;
        }
        ClockResult FreeAnimationSequences(std::uint64_t generation, std::uintptr_t tag) noexcept override
        {
            ++frees;
            receivedGeneration = generation;
            receivedTag = tag;
            if (callerTag)
                *callerTag = nullptr;
            if (!generation || generation != currentGeneration)
                return ClockResult::OwnerConflict;
            if (tag && tag != issuedTag)
                return ClockResult::OwnerConflict;
            return result;
        }
    };
    template <class Operation>
    void Refuses(Owner& owner, Operation operation, const char* method, bool allocation)
    {
        const auto calls = allocation ? owner.allocations : owner.frees;
        bool       refused = false;
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            refused = std::string(error.what()).find(method) != std::string::npos;
        }
        Check(refused && (allocation ? owner.allocations : owner.frees) == calls + 1,
              "SDK owner refusal names the actual operation and dispatches exactly once without native retry or fallback");
    }
}
int main()
{
    using namespace SequenceStorageSdkTests;
    static_assert(std::is_abstract_v<NativeAnimationSequenceStorageAccess> && std::has_virtual_destructor_v<NativeAnimationSequenceStorageAccess>);
    static_assert(std::is_abstract_v<CAnimManagerDE> && std::is_base_of_v<CAnimManager, CAnimManagerDE>);
    Owner                                   owner;
    auto                                    suppliedGeneration = owner.currentGeneration;
    AnimationSequenceStorageManagerReceiver receiver(owner, owner, suppliedGeneration);
    CAnimManager&                           sdk = receiver;
    suppliedGeneration = 0;
    Check(!owner.allocations && !owner.frees &&
              dynamic_cast<void*>(static_cast<NativeAnimationManagerAccess*>(&owner)) ==
                  dynamic_cast<void*>(static_cast<NativeAnimationSequenceStorageAccess*>(&owner)),
          "constructing an explicit same-complete-object provider acquires no native allocation or cleanup authority");
    for (const auto capacity : {std::size_t{0}, std::size_t{1}, std::size_t{65536}, (std::numeric_limits<std::size_t>::max)()})
    {
        const auto before = owner.allocations;
        const auto tag = sdk.AllocateCustomAnimSequences(capacity);
        Check(reinterpret_cast<std::uintptr_t>(tag) == (capacity ? owner.issuedTag : 0) && owner.allocations == before + 1 &&
                  owner.receivedCapacity == capacity && owner.receivedGeneration == owner.currentGeneration,
              "actual SDK forwards full size_t capacity and immutable copied generation once, including zero, without native layout adoption");
    }
    std::size_t callerCapacity = 7;
    owner.callerCapacity = &callerCapacity;
    Check(sdk.AllocateCustomAnimSequences(callerCapacity) != nullptr && !callerCapacity && owner.receivedCapacity == 7,
          "capacity is a copied value before entering the owner callback");
    owner.callerCapacity = nullptr;
    for (const auto tag : {static_cast<CAnimBlendSequenceSAInterface*>(nullptr), reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.issuedTag)})
    {
        const auto before = owner.frees;
        sdk.FreeCustomAnimSequences(tag);
        Check(owner.frees == before + 1 && owner.receivedTag == reinterpret_cast<std::uintptr_t>(tag) && owner.receivedGeneration == owner.currentGeneration,
              "actual SDK dispatches both null and its currently issued first tag through current generation without freeing frames");
    }
    auto callerTag = reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.issuedTag);
    owner.callerTag = &callerTag;
    sdk.FreeCustomAnimSequences(callerTag);
    Check(!callerTag && owner.receivedTag == owner.issuedTag, "SDK captures the raw first-tag value before owner callback changes caller storage");
    owner.callerTag = nullptr;
    for (const auto result :
         {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::WriteFailure,
          ClockResult::Busy, ClockResult::Expired, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
    {
        owner.result = result;
        Refuses(owner, [&] { sdk.AllocateCustomAnimSequences(2); }, "AllocateCustomAnimSequences", true);
        Refuses(owner, [&] { sdk.FreeCustomAnimSequences(nullptr); }, "FreeCustomAnimSequences", false);
    }
    owner.result = ClockResult::Ok;
    owner.forceNull = true;
    const auto malformedFrees = owner.frees;
    Refuses(owner, [&] { sdk.AllocateCustomAnimSequences(2); }, "AllocateCustomAnimSequences", true);
    Check(owner.frees == malformedFrees, "malformed successful allocation output never causes a guessed SDK cleanup call");
    owner.forceNull = false;
    owner.forceZeroTag = true;
    Refuses(owner, [&] { sdk.AllocateCustomAnimSequences(0); }, "AllocateCustomAnimSequences", true);
    owner.forceZeroTag = false;
    Refuses(
        owner, [&] { sdk.FreeCustomAnimSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.issuedTag + 80)); }, "FreeCustomAnimSequences", false);
    ++owner.currentGeneration;
    Refuses(owner, [&] { sdk.AllocateCustomAnimSequences(0); }, "AllocateCustomAnimSequences", true);
    Refuses(owner, [&] { sdk.FreeCustomAnimSequences(nullptr); }, "FreeCustomAnimSequences", false);
    Check(owner.receivedGeneration + 1 == owner.currentGeneration, "old facade generation is not silently renewed by a newer owner");
    AnimationSequenceStorageManagerReceiver zeroGeneration(owner, owner, 0);
    Refuses(owner, [&] { zeroGeneration.AllocateCustomAnimSequences(0); }, "AllocateCustomAnimSequences", true);
    Refuses(owner, [&] { zeroGeneration.FreeCustomAnimSequences(nullptr); }, "FreeCustomAnimSequences", false);
    AnimationSequenceStorageManagerReceiver fresh(owner, owner, owner.currentGeneration);
    Check(fresh.AllocateCustomAnimSequences(1) != nullptr && owner.receivedGeneration == owner.currentGeneration,
          "a fresh explicitly constructed facade dispatches the newly admitted owner generation");
    Owner      other;
    bool       splitRefused = false;
    const auto beforeAlloc = owner.allocations, beforeFree = other.frees;
    try
    {
        AnimationSequenceStorageManagerReceiver split(owner, other, owner.currentGeneration);
    }
    catch (const std::invalid_argument&)
    {
        splitRefused = true;
    }
    Check(splitRefused && owner.allocations == beforeAlloc && other.frees == beforeFree,
          "split complete provider objects refuse construction before any service call even with matching shapes and generations");
    NativeAnimationManagerAccess            observationOnly;
    AnimationSequenceStorageManagerReceiver missing(observationOnly, owner.currentGeneration);
    bool                                    missingAlloc = false, missingFree = false;
    try
    {
        missing.AllocateCustomAnimSequences(0);
    }
    catch (const std::runtime_error&)
    {
        missingAlloc = true;
    }
    try
    {
        missing.FreeCustomAnimSequences(nullptr);
    }
    catch (const std::runtime_error&)
    {
        missingFree = true;
    }
    Check(missingAlloc && missingFree && owner.allocations == beforeAlloc && other.frees == beforeFree,
          "legacy observation-only constructor does not fabricate an optional storage owner, even for zero and null");
    const auto nativeFrees = owner.frees;
    {
        auto facade = std::make_unique<AnimationSequenceStorageManagerReceiver>(owner, owner, owner.currentGeneration);
        Check(facade->AllocateCustomAnimSequences(1) != nullptr, "prepare exact-type CPU facade destruction after a real SDK service dispatch");
    }
    Check(owner.frees == nativeFrees, "exact CPU facade destruction never adopts native allocations or calls storage cleanup implicitly");
    std::puts("Actual custom animation sequence storage SDK fixture completed");
    std::printf("Passed %u custom sequence storage SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
}
