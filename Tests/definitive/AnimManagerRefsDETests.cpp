/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual integer manager reference SDK policy and raw-name affinity
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlockDE.h"
#include <game/CAnimBlendHierarchy.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <game/CAnimBlendSequence.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "AnimationManagerRefsFixture.generated.h"
#include "AnimationManagerUnsignedBlockRefs.generated.h"
namespace ManagerRefsSdkTests
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
        Identity,
        Read,
        Add,
        Remove
    };
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationBlockAccess
    {
        std::uint64_t                generation = 0xfedcba9876543210ull, receivedGeneration = 0;
        std::uint32_t                receivedIndex = 0;
        NativeAnimationBlockName     name{};
        NativeAnimationBlockIdentity received{};
        std::uint16_t                bits = 0;
        std::array<unsigned, 4>      calls{};
        unsigned                     oldIdentities = 0, oldReads = 0, oldAdds = 0;
        ClockResult                  identityResult = ClockResult::Ok, counterResult = ClockResult::Ok;
        bool                         badIndex = false, badGeneration = false, mutateOnFailure = false;
        int*                         callerIndex = nullptr;
        Owner()
        {
            for (std::size_t index = 0; index != name.size(); ++index)
                name[index] = static_cast<unsigned char>(0x80 + index);
        }
        ClockResult ReadAnimationBlockCounterIdentity(std::uint64_t value, std::uint32_t index, NativeAnimationBlockIdentity& output) noexcept override
        {
            ++calls[Identity];
            receivedGeneration = value;
            receivedIndex = index;
            if (callerIndex)
                *callerIndex = 170;
            if (value != generation || !value)
                return ClockResult::OwnerConflict;
            if (index >= 180)
                return ClockResult::InvalidArgument;
            output.index = badIndex ? index + 1 : index;
            output.ownerGeneration = badGeneration ? value + 1 : value;
            output.name = name;
            return identityResult;
        }
        ClockResult Gate(Operation operation, const NativeAnimationBlockIdentity& identity) noexcept
        {
            ++calls[operation];
            received = identity;
            if (identity.ownerGeneration != generation || identity.index != receivedIndex || identity.name != name)
                return ClockResult::BindingChanged;
            return counterResult;
        }
        ClockResult ReadAnimationBlockCounterRefs(const NativeAnimationBlockIdentity& identity, std::uint16_t& output) noexcept override
        {
            const auto result = Gate(Read, identity);
            output = bits;
            return result;
        }
        ClockResult AddAnimationBlockCounterRef(const NativeAnimationBlockIdentity& identity) noexcept override
        {
            const auto result = Gate(Add, identity);
            if (result == ClockResult::Ok || mutateOnFailure)
                ++bits;
            return result;
        }
        ClockResult RemoveAnimationBlockRefWithoutDelete(const NativeAnimationBlockIdentity& identity) noexcept override
        {
            const auto result = Gate(Remove, identity);
            if (result == ClockResult::Ok || mutateOnFailure)
                --bits;
            return result;
        }
        // Legacy channels stay separate. Only the explicit unsigned block
        // getter comparison below is allowed to use its named read channel.
        ClockResult ReadAnimationBlockIdentity(std::uint64_t, std::uint32_t, NativeAnimationBlockIdentity&) noexcept override
        {
            ++oldIdentities;
            return ClockResult::Stopped;
        }
        ClockResult ReadAnimationBlockRefs(const NativeAnimationBlockIdentity& identity, std::uint16_t& output) noexcept override
        {
            ++oldReads;
            received = identity;
            output = bits;
            return ClockResult::Ok;
        }
        ClockResult AddAnimationBlockRef(const NativeAnimationBlockIdentity&) noexcept override
        {
            ++oldAdds;
            return ClockResult::Stopped;
        }
    };
    bool Error(const std::runtime_error& error, const char* method, ClockResult result)
    {
        const std::string text = error.what(), suffix = std::string(": ") + std::to_string(static_cast<unsigned>(result));
        return text.find(method) != std::string::npos && text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
    template <class Call>
    void Refuses(Owner& owner, Call call, const char* method, ClockResult result, int counter = -1, bool identity = true)
    {
        auto expected = owner.calls;
        if (identity)
            ++expected[Identity];
        if (counter >= 0)
            ++expected[static_cast<std::size_t>(counter)];
        bool refused = false;
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            refused = Error(error, method, result);
        }
        Check(refused && owner.calls == expected,
              "refusal preserves status/operation and reaches each intended service once without retry or guessed inverse write");
    }
    struct DefaultOwner final : NativeAnimationManagerAccess, NativeAnimationBlockAccess
    {
        unsigned    identities = 0;
        ClockResult ReadAnimationBlockCounterIdentity(std::uint64_t generation, std::uint32_t index, NativeAnimationBlockIdentity& output) noexcept override
        {
            ++identities;
            output.index = index;
            output.ownerGeneration = generation;
            output.name.fill('X');
            return ClockResult::Ok;
        }
    };
}
int main()
{
    using namespace ManagerRefsSdkTests;
    static_assert(std::is_abstract_v<CAnimManagerDE>);
    static_assert(std::is_same_v<decltype(&NativeAnimationManagerAccess::ReadAnimationBlockCounterIdentity),
                                 ClockResult (NativeAnimationManagerAccess::*)(std::uint64_t, std::uint32_t, NativeAnimationBlockIdentity&) noexcept>);
    NativeAnimationManagerAccess unavailable;
    NativeAnimationBlockIdentity untouched;
    untouched.index = 17;
    untouched.ownerGeneration = 97;
    untouched.name.fill(0xa5);
    const auto original = untouched;
    Check(unavailable.ReadAnimationBlockCounterIdentity(7, 0, untouched) == ClockResult::NotInitialized && untouched.index == original.index &&
              untouched.ownerGeneration == original.ownerGeneration && untouched.name == original.name,
          "default raw identity access refuses without fabricating or overwriting affinity");
    Owner                        owner;
    auto                         suppliedGeneration = owner.generation;
    AnimationManagerRefsReceiver receiver(owner, owner, suppliedGeneration);
    CAnimManager&                sdk = receiver;
    suppliedGeneration = 0;
    Check(owner.calls == std::array<unsigned, 4>{}, "counter-only same-owner construction makes no native calls and needs no CGame/storage provider");
    for (const int index : {0, 1, 179})
    {
        const auto before = owner.calls;
        sdk.AddAnimBlockRef(index);
        auto expected = before;
        ++expected[Identity];
        ++expected[Add];
        Check(owner.calls == expected && owner.receivedIndex == static_cast<std::uint32_t>(index) && owner.receivedGeneration == owner.generation &&
                  owner.received.name == owner.name && owner.received.index == static_cast<std::uint32_t>(index),
              "integer add preserves copied ordinal/generation and all16 high/nonterminated affinity bytes via the raw counter channel");
    }
    int callerIndex = 3;
    owner.callerIndex = &callerIndex;
    sdk.AddAnimBlockRef(callerIndex);
    owner.callerIndex = nullptr;
    Check(callerIndex == 170 && owner.receivedIndex == 3 && owner.received.index == 3,
          "caller integer mutation during identity callback cannot change the copied ordinal");
    struct Value
    {
        std::uint16_t bits;
        int           result;
    };
    for (const auto value : {Value{0, 0}, Value{1, 1}, Value{0x7fff, 32767}, Value{0x8000, -32768}, Value{0xffff, -1}})
    {
        owner.bits = value.bits;
        const auto before = owner.calls;
        const auto result = sdk.GetNumRefsToAnimBlock(2);
        auto       expected = before;
        ++expected[Identity];
        ++expected[Read];
        Check(result == value.result && owner.calls == expected && owner.received.name == owner.name,
              "actual manager getter sign-extends all raw16 boundary bits without a C-string or nonnegative counter filter");
    }
    owner.bits = 0xffff;
    const auto readsBeforeAdd = owner.calls[Read];
    sdk.AddAnimBlockRef(2);
    Check(owner.bits == 0 && owner.calls[Read] == readsBeforeAdd, "add dispatches once atffff without a CPU read, clamp or overflow policy");
    owner.bits = 0;
    const auto readsBeforeRemove = owner.calls[Read];
    sdk.RemoveAnimBlockRefWithoutDelete(2);
    Check(owner.bits == 0xffff && owner.calls[Read] == readsBeforeRemove,
          "without-delete dispatches unconditionally at zero and does not invent cleanup or a zero guard");
    Check(owner.oldIdentities == 0 && owner.oldReads == 0 && owner.oldAdds == 0,
          "integer manager references do not fall back to stricter name/factory channels");
    owner.name.fill(0);
    std::memcpy(owner.name.data(), "block", 5);
    NativeAnimationBlockIdentity blockIdentity;
    blockIdentity.index = 2;
    blockIdentity.ownerGeneration = owner.generation;
    blockIdentity.name = owner.name;
    animation_block_refs_fixture::RefsReceiver block(owner, blockIdentity);
    CAnimBlock&                                blockSdk = block;
    for (const auto value : {Value{0x7fff, 32767}, Value{0x8000, -32768}, Value{0xffff, -1}})
    {
        owner.bits = value.bits;
        Check(sdk.GetNumRefsToAnimBlock(2) == value.result && blockSdk.GetRefs() == value.bits,
              "actual manager signed getter and actual separate block unsigned getter deliberately preserve different SDK return contracts");
    }
    for (const int index : {-1, (std::numeric_limits<int>::min)()})
    {
        Refuses(owner, [&] { sdk.AddAnimBlockRef(index); }, "AddAnimBlockRef", ClockResult::InvalidArgument, -1, false);
        Refuses(owner, [&] { sdk.GetNumRefsToAnimBlock(index); }, "GetNumRefsToAnimBlock", ClockResult::InvalidArgument, -1, false);
        Refuses(owner, [&] { sdk.RemoveAnimBlockRefWithoutDelete(index); }, "RemoveAnimBlockRefWithoutDelete", ClockResult::InvalidArgument, -1, false);
    }
    Refuses(owner, [&] { sdk.GetNumRefsToAnimBlock((std::numeric_limits<int>::max)()); }, "GetNumRefsToAnimBlock", ClockResult::InvalidArgument);
    Check(owner.receivedIndex == static_cast<std::uint32_t>((std::numeric_limits<int>::max)()),
          "positive int maximum reaches current native index admission without SDK narrowing or clamping");
    owner.badIndex = true;
    Refuses(owner, [&] { sdk.GetNumRefsToAnimBlock(2); }, "GetNumRefsToAnimBlock", ClockResult::BindingChanged);
    owner.badIndex = false;
    owner.badGeneration = true;
    Refuses(owner, [&] { sdk.GetNumRefsToAnimBlock(2); }, "GetNumRefsToAnimBlock", ClockResult::BindingChanged);
    owner.badGeneration = false;
    for (const auto status :
         {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::WriteFailure,
          ClockResult::Busy, ClockResult::Expired, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
    {
        owner.identityResult = status;
        Refuses(owner, [&] { sdk.GetNumRefsToAnimBlock(2); }, "GetNumRefsToAnimBlock", status);
        owner.identityResult = ClockResult::Ok;
        owner.counterResult = status;
        Refuses(owner, [&] { sdk.GetNumRefsToAnimBlock(2); }, "GetNumRefsToAnimBlock", status, Read);
        Refuses(owner, [&] { sdk.AddAnimBlockRef(2); }, "AddAnimBlockRef", status, Add);
        Refuses(owner, [&] { sdk.RemoveAnimBlockRefWithoutDelete(2); }, "RemoveAnimBlockRefWithoutDelete", status, Remove);
    }
    owner.counterResult = ClockResult::WriteFailure;
    owner.mutateOnFailure = true;
    owner.bits = 0;
    Refuses(owner, [&] { sdk.AddAnimBlockRef(2); }, "AddAnimBlockRef", ClockResult::WriteFailure, Add);
    Check(owner.bits == 1, "entered add failure is not silently rolled back or replayed");
    Refuses(owner, [&] { sdk.RemoveAnimBlockRefWithoutDelete(2); }, "RemoveAnimBlockRefWithoutDelete", ClockResult::WriteFailure, Remove);
    Check(owner.bits == 0, "entered decrement failure is not silently rolled back or replayed");
    owner.counterResult = ClockResult::Ok;
    owner.mutateOnFailure = false;
    ++owner.generation;
    Refuses(owner, [&] { sdk.GetNumRefsToAnimBlock(2); }, "GetNumRefsToAnimBlock", ClockResult::OwnerConflict);
    AnimationManagerRefsReceiver zero(owner, owner, 0);
    Refuses(owner, [&] { zero.AddAnimBlockRef(2); }, "AddAnimBlockRef", ClockResult::OwnerConflict);
    AnimationManagerRefsReceiver fresh(owner, owner, owner.generation);
    owner.bits = 0x8000;
    Check(fresh.GetNumRefsToAnimBlock(2) == -32768 && owner.receivedGeneration == owner.generation,
          "fresh facade explicitly selects the current admitted generation");
    Owner      other;
    const auto beforeSplit = owner.calls, otherBefore = other.calls;
    bool       split = false;
    try
    {
        AnimationManagerRefsReceiver invalid(owner, other, owner.generation);
    }
    catch (const std::invalid_argument&)
    {
        split = true;
    }
    Check(split && owner.calls == beforeSplit && other.calls == otherBefore, "split complete native providers refuse before any service callback");
    AnimationManagerRefsReceiver missing(owner, owner.generation);
    Refuses(owner, [&] { missing.AddAnimBlockRef(0); }, "AddAnimBlockRef", ClockResult::NotInitialized, -1, false);
    Refuses(owner, [&] { missing.GetNumRefsToAnimBlock(0); }, "GetNumRefsToAnimBlock", ClockResult::NotInitialized, -1, false);
    Refuses(owner, [&] { missing.RemoveAnimBlockRefWithoutDelete(0); }, "RemoveAnimBlockRefWithoutDelete", ClockResult::NotInitialized, -1, false);
    Refuses(owner, [&] { fresh.GetAnimationBlock(0); }, "GetAnimationBlock", ClockResult::NotInitialized, -1, false);
    Refuses(owner, [&] { fresh.AllocateKeyFramesMemory(0); }, "AllocateKeyFramesMemory", ClockResult::NotInitialized, -1, false);
    DefaultOwner                 defaults;
    AnimationManagerRefsReceiver unsupported(defaults, defaults, 7);
    bool                         readRefused = false, addRefused = false, removeRefused = false;
    try
    {
        unsupported.GetNumRefsToAnimBlock(0);
    }
    catch (const std::runtime_error& error)
    {
        readRefused = Error(error, "GetNumRefsToAnimBlock", ClockResult::NotInitialized);
    }
    try
    {
        unsupported.AddAnimBlockRef(0);
    }
    catch (const std::runtime_error& error)
    {
        addRefused = Error(error, "AddAnimBlockRef", ClockResult::NotInitialized);
    }
    try
    {
        unsupported.RemoveAnimBlockRefWithoutDelete(0);
    }
    catch (const std::runtime_error& error)
    {
        removeRefused = Error(error, "RemoveAnimBlockRefWithoutDelete", ClockResult::NotInitialized);
    }
    Check(readRefused && addRefused && removeRefused && defaults.identities == 3,
          "all raw counter access defaults refuse instead of fabricating values or writes");
    const auto beforeDestroy = owner.calls;
    {
        AnimationManagerRefsReceiver temporary(owner, owner, owner.generation);
    }
    Check(owner.calls == beforeDestroy, "CPU facade destruction performs no inverse reference update or native cleanup");
    std::puts("Actual animation manager raw reference SDK fixture completed");
    std::puts("Actual manager signed and block unsigned reference fixture completed");
    std::printf("Passed %u animation manager reference SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
}
