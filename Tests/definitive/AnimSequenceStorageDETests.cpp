/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual four-method MTA animation storage SDK dispatch, no engine
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include <game/CAnimBlock.h>
#include <game/CAnimBlendHierarchy.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <game/CAnimBlendSequence.h>
#include <array>
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
namespace AnimationStorageSdkTests
{
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL %u: %s\n", checks, reason);
            std::exit(1);
        }
    }
    enum Operation : std::size_t
    {
        ArrayAllocate,
        ArrayFree,
        FrameAllocate,
        FrameFree
    };
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationSequenceStorageAccess
    {
        std::uint64_t           currentGeneration = 0xfedcba9876543210ull, receivedGeneration = 0;
        std::uintptr_t          arrayTag = 0x20000, frameBase = ((std::numeric_limits<std::uintptr_t>::max)() & ~std::uintptr_t{255}) - 4096;
        std::uintptr_t          receivedArrayTag = 0, receivedFrameBase = 0, forcedFrameAddress = 0;
        std::size_t             receivedCapacity = 0;
        std::uint32_t           receivedBytes = 0;
        std::array<unsigned, 4> calls{};
        ClockResult             result = ClockResult::Ok;
        bool                    forceFrameAddress = false;
        std::uint32_t*          callerBytes = nullptr;
        void**                  callerFrame = nullptr;
        ClockResult             Enter(Operation operation, std::uint64_t generation) noexcept
        {
            ++calls[operation];
            receivedGeneration = generation;
            return generation && generation == currentGeneration ? result : ClockResult::OwnerConflict;
        }
        ClockResult AllocateAnimationSequences(std::uint64_t generation, std::size_t capacity, std::uintptr_t& output) noexcept override
        {
            receivedCapacity = capacity;
            output = capacity ? arrayTag : 0;
            return Enter(ArrayAllocate, generation);
        }
        ClockResult FreeAnimationSequences(std::uint64_t generation, std::uintptr_t tag) noexcept override
        {
            receivedArrayTag = tag;
            const auto status = Enter(ArrayFree, generation);
            return status != ClockResult::Ok ? status : (tag && tag != arrayTag ? ClockResult::OwnerConflict : ClockResult::Ok);
        }
        ClockResult AllocateAnimationKeyFrames(std::uint64_t generation, std::uint32_t bytes, std::uintptr_t& output) noexcept override
        {
            receivedBytes = bytes;
            output = forceFrameAddress ? forcedFrameAddress : (bytes ? frameBase : 0);
            if (callerBytes)
                *callerBytes = 0;
            return Enter(FrameAllocate, generation);
        }
        ClockResult FreeAnimationKeyFrames(std::uint64_t generation, std::uintptr_t base) noexcept override
        {
            receivedFrameBase = base;
            if (callerFrame)
                *callerFrame = nullptr;
            const auto status = Enter(FrameFree, generation);
            return status != ClockResult::Ok ? status : (base && base != frameBase ? ClockResult::OwnerConflict : ClockResult::Ok);
        }
    };
    template <class Call>
    void Refuses(Owner& owner, Operation operation, Call call, const char* method, ClockResult expectedResult = ClockResult::OwnerConflict)
    {
        auto expected = owner.calls;
        ++expected[operation];
        bool refused = false;
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            const std::string message = error.what();
            const auto        suffix = std::string(": ") + std::to_string(static_cast<unsigned>(expectedResult));
            refused = message.find(method) != std::string::npos && message.size() >= suffix.size() &&
                      message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused && owner.calls == expected,
              "owner refusal has actual operation context and exactly one callback, with no retry or other allocation/free");
    }
    template <class Call>
    void Missing(Owner& owner, Call call, const char* method)
    {
        const auto before = owner.calls;
        bool       refused = false;
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            refused = std::string(error.what()).find(method) != std::string::npos;
        }
        Check(refused && owner.calls == before, "observation-only constructor does not invent a provider even for zero/null storage calls");
    }
}
int main()
{
    using namespace AnimationStorageSdkTests;
    static_assert(std::is_abstract_v<CAnimManagerDE> && std::is_abstract_v<NativeAnimationSequenceStorageAccess>);
    static_assert(std::is_same_v<decltype(&NativeAnimationSequenceStorageAccess::AllocateAnimationKeyFrames),
                                 ClockResult (NativeAnimationSequenceStorageAccess::*)(std::uint64_t, std::uint32_t, std::uintptr_t&) noexcept>);
    static_assert(std::is_same_v<decltype(&NativeAnimationSequenceStorageAccess::FreeAnimationKeyFrames),
                                 ClockResult (NativeAnimationSequenceStorageAccess::*)(std::uint64_t, std::uintptr_t) noexcept>);
    Owner                                   owner;
    auto                                    suppliedGeneration = owner.currentGeneration;
    AnimationSequenceStorageManagerReceiver receiver(owner, owner, suppliedGeneration);
    CAnimManager&                           sdk = receiver;
    suppliedGeneration = 0;
    Check(owner.calls == std::array<unsigned, 4>{}, "same-complete-owner CPU construction performs no allocation, observation or cleanup");
    // One interleaved actual SDK itinerary covers all four methods without
    // replaying the previous exhaustive array-only matrix.
    const auto capacity = (std::numeric_limits<std::size_t>::max)();
    auto*      array = sdk.AllocateCustomAnimSequences(capacity);
    Check(reinterpret_cast<std::uintptr_t>(array) == owner.arrayTag && owner.receivedCapacity == capacity, "array still forwards full native size_t capacity");
    auto* frames = sdk.AllocateKeyFramesMemory(16);
    Check(reinterpret_cast<std::uintptr_t>(frames) == owner.frameBase && owner.receivedBytes == 16, "frame allocation returns exact full-width opaque base");
    sdk.FreeCustomAnimSequences(array);
    sdk.FreeKeyFramesMemory(frames);
    Check(owner.calls == std::array<unsigned, 4>{1, 1, 1, 1} && owner.receivedGeneration == owner.currentGeneration,
          "four actual SDK methods use the same copied generation and distinct service callbacks");
    Check(owner.receivedArrayTag == owner.arrayTag && owner.receivedFrameBase == owner.frameBase, "array tags and no-cookie frame bases are not interchanged");
    for (const auto bytes : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{65536}, (std::numeric_limits<std::uint32_t>::max)()})
    {
        const auto before = owner.calls;
        auto       expected = before;
        ++expected[FrameAllocate];
        auto* value = sdk.AllocateKeyFramesMemory(bytes);
        Check(owner.calls == expected && owner.receivedBytes == bytes && owner.receivedGeneration == owner.currentGeneration &&
                  reinterpret_cast<std::uintptr_t>(value) == (bytes ? owner.frameBase : 0),
              "frame bytes preserve all32 bits on both CPU widths and zero still dispatches");
    }
    std::uint32_t callerBytes = 0xdeadbeef;
    owner.callerBytes = &callerBytes;
    Check(sdk.AllocateKeyFramesMemory(callerBytes) != nullptr && callerBytes == 0 && owner.receivedBytes == 0xdeadbeef,
          "requested bytes are copied before the owner changes caller storage");
    owner.callerBytes = nullptr;
    owner.forceFrameAddress = true;
    owner.forcedFrameAddress = 0;
    const auto freesBeforeNull = owner.calls[FrameFree];
    Check(sdk.AllocateKeyFramesMemory(128) == nullptr && owner.calls[FrameFree] == freesBeforeNull,
          "successful nullable frame allocation stays null for real MTA caller handling, with no invented cleanup or array-shape exception");
    owner.forcedFrameAddress = owner.frameBase;
    Check(reinterpret_cast<std::uintptr_t>(sdk.AllocateKeyFramesMemory(0)) == owner.frameBase,
          "SDK preserves an authorized zero-byte provider result without imposing the controller's separate zero policy");
    owner.forceFrameAddress = false;
    for (auto* value : {static_cast<void*>(nullptr), reinterpret_cast<void*>(owner.frameBase)})
    {
        auto expected = owner.calls;
        ++expected[FrameFree];
        sdk.FreeKeyFramesMemory(value);
        Check(owner.calls == expected && owner.receivedFrameBase == reinterpret_cast<std::uintptr_t>(value),
              "free forwards both null and the full current frame base once");
    }
    void* callerFrame = reinterpret_cast<void*>(owner.frameBase);
    owner.callerFrame = &callerFrame;
    sdk.FreeKeyFramesMemory(callerFrame);
    Check(callerFrame == nullptr && owner.receivedFrameBase == owner.frameBase, "frame base is copied before the owner changes caller storage");
    owner.callerFrame = nullptr;
    for (const auto result :
         {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::WriteFailure,
          ClockResult::Busy, ClockResult::Expired, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
    {
        owner.result = result;
        Refuses(owner, FrameAllocate, [&] { sdk.AllocateKeyFramesMemory(0); }, "AllocateKeyFramesMemory", result);
        Refuses(owner, FrameFree, [&] { sdk.FreeKeyFramesMemory(nullptr); }, "FreeKeyFramesMemory", result);
    }
    owner.result = ClockResult::Ok;
    Refuses(owner, FrameFree, [&] { sdk.FreeKeyFramesMemory(reinterpret_cast<void*>(owner.arrayTag)); }, "FreeKeyFramesMemory");
    Refuses(
        owner, ArrayFree, [&] { sdk.FreeCustomAnimSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(owner.frameBase)); }, "FreeCustomAnimSequences");
    ++owner.currentGeneration;
    Refuses(owner, ArrayAllocate, [&] { sdk.AllocateCustomAnimSequences(0); }, "AllocateCustomAnimSequences");
    Refuses(owner, ArrayFree, [&] { sdk.FreeCustomAnimSequences(nullptr); }, "FreeCustomAnimSequences");
    Refuses(owner, FrameAllocate, [&] { sdk.AllocateKeyFramesMemory(0); }, "AllocateKeyFramesMemory");
    Refuses(owner, FrameFree, [&] { sdk.FreeKeyFramesMemory(nullptr); }, "FreeKeyFramesMemory");
    Check(owner.receivedGeneration + 1 == owner.currentGeneration, "a stale facade never silently renews its copied generation");
    AnimationSequenceStorageManagerReceiver zero(owner, owner, 0);
    Refuses(owner, FrameAllocate, [&] { zero.AllocateKeyFramesMemory(0); }, "AllocateKeyFramesMemory");
    Refuses(owner, FrameFree, [&] { zero.FreeKeyFramesMemory(nullptr); }, "FreeKeyFramesMemory");
    AnimationSequenceStorageManagerReceiver fresh(owner, owner, owner.currentGeneration);
    Check(fresh.AllocateKeyFramesMemory(1) != nullptr && owner.receivedGeneration == owner.currentGeneration,
          "a newly constructed same-owner facade uses its new admitted generation");
    Owner      other;
    const auto oldCalls = owner.calls, otherCalls = other.calls;
    bool       split = false;
    try
    {
        AnimationSequenceStorageManagerReceiver invalid(owner, other, owner.currentGeneration);
    }
    catch (const std::invalid_argument&)
    {
        split = true;
    }
    Check(split && owner.calls == oldCalls && other.calls == otherCalls, "split complete CPU providers refuse before any storage call");
    NativeAnimationManagerAccess            observations;
    AnimationSequenceStorageManagerReceiver missing(observations, owner.currentGeneration);
    Missing(owner, [&] { missing.AllocateCustomAnimSequences(0); }, "AllocateCustomAnimSequences");
    Missing(owner, [&] { missing.FreeCustomAnimSequences(nullptr); }, "FreeCustomAnimSequences");
    Missing(owner, [&] { missing.AllocateKeyFramesMemory(0); }, "AllocateKeyFramesMemory");
    Missing(owner, [&] { missing.FreeKeyFramesMemory(nullptr); }, "FreeKeyFramesMemory");
    const auto arrayFrees = owner.calls[ArrayFree], frameFrees = owner.calls[FrameFree];
    {
        auto facade = std::make_unique<AnimationSequenceStorageManagerReceiver>(owner, owner, owner.currentGeneration);
        Check(facade->AllocateKeyFramesMemory(1) != nullptr, "prepare exact-type CPU destruction after frame SDK dispatch");
    }
    Check(owner.calls[ArrayFree] == arrayFrees && owner.calls[FrameFree] == frameFrees, "CPU facade destruction adopts neither array nor frame cleanup debt");
    std::puts("Actual custom animation sequence storage SDK fixture completed");
    std::puts("Actual animation keyframe storage SDK fixture completed");
    std::puts("Actual animation storage four-method SDK fixture completed");
    std::printf("Passed %u custom animation storage SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
}
