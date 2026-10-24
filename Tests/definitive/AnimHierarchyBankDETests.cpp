/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual nine-method hierarchy SDK dispatch without a native factory
 ****************************************************************************/
#include "AnimationHierarchyDEFixture.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    using namespace mta::game_de;
    using animation_hierarchy_fixture::AnimationHierarchyReceiver;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    bool Same(const NativeAnimationHierarchyReferenceToken& a, const NativeAnimationHierarchyReferenceToken& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.locator == b.locator;
    }
    struct Owner final : NativeAnimationHierarchyAccess
    {
        enum Channel : unsigned
        {
            Scalar,
            Many,
            One
        };
        NativeAnimationHierarchyReferenceToken expected{}, seen{};
        NativeAnimationHierarchyValues         values{};
        std::array<unsigned, 3>                calls{};
        ClockResult                            result = ClockResult::Ok;
        std::uintptr_t                         many = 0, one = 0;
        std::uint32_t                          index = 0;
        bool                                   correct = true;
        unsigned                               Total() const noexcept { return calls[0] + calls[1] + calls[2]; }
        void                                   Reset() noexcept
        {
            calls = {};
            correct = true;
        }
        void Observe(const NativeAnimationHierarchyReferenceToken& token, Channel channel) noexcept
        {
            seen = token;
            correct = correct && Same(token, expected);
            ++calls[channel];
        }
        ClockResult ReadAnimationHierarchy(const NativeAnimationHierarchyReferenceToken& token, NativeAnimationHierarchyValues& output) noexcept override
        {
            Observe(token, Scalar);
            output = values;
            return result;
        }
        ClockResult ReadAnimationHierarchySequences(const NativeAnimationHierarchyReferenceToken& token, std::uintptr_t& output) noexcept override
        {
            Observe(token, Many);
            output = many;
            return result;
        }
        ClockResult ReadAnimationHierarchySequence(const NativeAnimationHierarchyReferenceToken& token, std::uint32_t requested,
                                                   std::uintptr_t& output) noexcept override
        {
            Observe(token, One);
            index = requested;
            output = one;
            return result;
        }
    };
    std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t bits;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    template <class Call>
    void Refusal(Owner& owner, const char* operation, Owner::Channel channel, ClockResult status, Call call)
    {
        owner.Reset();
        owner.result = status;
        bool refused = false;
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            refused = error.what() == std::string("DE animation hierarchy owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(status));
        }
        Check(refused, "hierarchy refusal lost exact operation/status context or exposed failed output");
        Check(owner.Total() == 1 && owner.calls[channel] == 1 && owner.correct, "hierarchy read retried, used another channel or changed the copied token");
    }
}
int main()
{
    try
    {
        static_assert(!noexcept(std::declval<const CAnimBlendHierarchyDE&>().GetTotalTime()));
        NativeAnimationHierarchyAccess         missing;
        NativeAnimationHierarchyReferenceToken none{};
        NativeAnimationHierarchyValues         untouched{1, 2, 3, 4, 5, 6};
        std::uintptr_t                         pointer = 0x12345678u;
        Check(missing.ReadAnimationHierarchy(none, untouched) == ClockResult::NotInitialized && untouched.interfaceAddress == 1 && untouched.hash == 2 &&
                  untouched.totalTimeBits == 3 && untouched.sequenceCount == 4 && untouched.runningCompressed == 5 && untouched.blockId == 6,
              "default scalar service changed caller output or pretended success");
        Check(missing.ReadAnimationHierarchySequences(none, pointer) == ClockResult::NotInitialized && pointer == 0x12345678u,
              "default array service changed caller output");
        Check(missing.ReadAnimationHierarchySequence(none, 0xffffffffu, pointer) == ClockResult::NotInitialized && pointer == 0x12345678u,
              "default selected sequence service changed caller output");
        Owner                                  owner;
        const auto                             max = (std::numeric_limits<std::uintptr_t>::max)();
        NativeAnimationHierarchyReferenceToken caller{0xfedcba9876543210ull, max - 4095u};
        owner.expected = caller;
        AnimationHierarchyReceiver receiver(owner, caller);
        CAnimBlendHierarchy&       sdk = receiver;
        const CAnimBlendHierarchy& immutable = sdk;
        caller = {};
        Check(!owner.Total(), "constructing a CPU hierarchy facade queried or leased native storage");
        for (std::uint32_t bits : {0u, 0x80000000u, 0x00000001u, 0x3f800000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc54321u})
        {
            owner.Reset();
            owner.values.totalTimeBits = bits;
            Check(Bits(immutable.GetTotalTime()) == bits && owner.calls[Owner::Scalar] == 1 && owner.Total() == 1 && owner.correct,
                  "actual const float read changed signed zero/quietNaN/raw bits or hid native admission");
        }
        for (std::int32_t block : {-1, -2, 0, 1, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()})
        {
            owner.Reset();
            owner.values.blockId = block;
            Check(sdk.GetAnimBlockID() == block && sdk.IsCustom() == (block == -1) && owner.Total() == 2 && owner.calls[Owner::Scalar] == 2 && owner.correct,
                  "block ID/IsCustom changed source-exact raw native field behavior or cached wrapper kind");
        }
        for (std::uint16_t count : {std::uint16_t{0}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
        {
            owner.Reset();
            owner.values.sequenceCount = count;
            Check(sdk.GetNumSequences() == count && owner.Total() == 1, "published WORD count was sign-extended or clamped");
        }
        for (std::uint8_t compressed : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{0xff}})
        {
            owner.Reset();
            owner.values.runningCompressed = compressed;
            Check(sdk.IsRunningCompressed() == (compressed != 0) && owner.Total() == 1, "compressed observation required exactly BYTE1 instead of source bool");
        }
        for (std::uint32_t hash : {0u, 1u, 0x80000000u, 0xffffffffu})
        {
            owner.Reset();
            owner.values.hash = hash;
            Check(sdk.GetNameHashKey() == hash && owner.Total() == 1, "hash rawDWORD bits changed or invoked a name/cache operation");
        }
        owner.values.interfaceAddress = max - 63u;
        owner.Reset();
        Check(reinterpret_cast<std::uintptr_t>(sdk.GetInterface()) == max - 63u && owner.Total() == 1 && owner.correct,
              "opaque hierarchy locator was narrowed, dereferenced or derived from classic layout");
        owner.values.sequenceCount = 0;
        owner.many = max - 127u;
        owner.one = max - 255u;
        owner.Reset();
        Check(reinterpret_cast<std::uintptr_t>(sdk.GetSequences()) == owner.many && owner.calls[Owner::Many] == 1 && !owner.calls[Owner::Scalar],
              "nested array getter used scalar path or its own pointer arithmetic");
        for (std::uint32_t index : {0u, 1u, 65535u, 65536u, 0xffffffffu})
        {
            owner.Reset();
            Check(reinterpret_cast<std::uintptr_t>(sdk.GetSequence(index)) == owner.one && owner.index == index && owner.Total() == 1 &&
                      owner.calls[Owner::One] == 1 && owner.correct,
                  "selected sequence narrowed DWORD input or substituted published WORD count for owner capacity");
        }
        owner.many = owner.one = 0;
        owner.Reset();
        Check(!sdk.GetSequences() && !sdk.GetSequence(0) && owner.calls[Owner::Many] == 1 && owner.calls[Owner::One] == 1 && !owner.calls[Owner::Scalar],
              "source null nested borrows were replaced or qualified via another channel");
        const std::array<ClockResult, 14> errors{{ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread,
                                                  ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::WriteFailure, ClockResult::RestoreFailure,
                                                  ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired, ClockResult::UnsupportedBuild,
                                                  ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
        for (auto status : errors)
        {
            Refusal(owner, "GetTotalTime", Owner::Scalar, status, [&] { immutable.GetTotalTime(); });
            Refusal(owner, "GetNumSequences", Owner::Scalar, status, [&] { sdk.GetNumSequences(); });
            Refusal(owner, "IsRunningCompressed", Owner::Scalar, status, [&] { sdk.IsRunningCompressed(); });
            Refusal(owner, "IsCustom", Owner::Scalar, status, [&] { sdk.IsCustom(); });
            Refusal(owner, "GetAnimBlockID", Owner::Scalar, status, [&] { sdk.GetAnimBlockID(); });
            Refusal(owner, "GetInterface", Owner::Scalar, status, [&] { sdk.GetInterface(); });
            Refusal(owner, "GetNameHashKey", Owner::Scalar, status, [&] { sdk.GetNameHashKey(); });
            Refusal(owner, "GetSequences", Owner::Many, status, [&] { sdk.GetSequences(); });
            Refusal(owner, "GetSequence", Owner::One, status, [&] { sdk.GetSequence(0xffffffffu); });
        }
        owner.result = ClockResult::Ok;
        owner.Reset();
        NativeAnimationHierarchyReferenceToken other{7, 0x2000u};
        AnimationHierarchyReceiver             second(owner, other);
        owner.expected = other;
        second.GetNameHashKey();
        Check(Same(owner.seen, other), "second receiver reused the first hierarchy's token");
        owner.expected = {0xfedcba9876543210ull, max - 4095u};
        sdk.GetNameHashKey();
        Check(Same(owner.seen, owner.expected) && owner.correct, "another receiver changed immutable first hierarchy intent");
        owner.Reset();
        {
            AnimationHierarchyReceiver temporary(owner, other);
        }
        Check(!owner.Total(), "CPU destruction queried native rows or adopted nested cleanup");
        std::puts("Actual MTA bank hierarchy SDK fixture completed");
        std::printf("Passed %u animation hierarchy bank SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Hierarchy bank SDK check failed: %s\n", error.what());
        return 1;
    }
}
