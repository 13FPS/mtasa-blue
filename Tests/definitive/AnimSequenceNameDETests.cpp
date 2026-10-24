/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SetName dispatch and the shared bounded raw-name/hash policy
 ****************************************************************************/
#include "AnimationSequenceDEFixture.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    using namespace mta::game_de;
    using animation_sequence_fixture::AnimationSequenceReceiver;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    bool Same(const NativeAnimationSequenceReferenceToken& a, const NativeAnimationSequenceReferenceToken& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.firstTag == b.firstTag && a.index == b.index;
    }
    struct Owner final : NativeAnimationSequenceAccess
    {
        NativeAnimationSequenceReferenceToken expected{}, seen{};
        NativeAnimationSequenceName           name{};
        ClockResult                           result = ClockResult::Ok;
        unsigned                              calls = 0, unexpected = 0;
        bool                                  correct = true;
        char*                                 changeInput = nullptr;
        ClockResult SetAnimationSequenceName(const NativeAnimationSequenceReferenceToken& token, const NativeAnimationSequenceName& value) noexcept override
        {
            ++calls;
            seen = token;
            correct = correct && Same(token, expected);
            name = value;
            if (changeInput)
            {
                changeInput[0] = '!';
                changeInput[1] = 0;
            }
            return result;
        }
        ClockResult ReadAnimationSequence(const NativeAnimationSequenceReferenceToken&, NativeAnimationSequenceValues&) noexcept override
        {
            ++unexpected;
            return ClockResult::Stopped;
        }
        ClockResult ReadAnimationSequenceHash(const NativeAnimationSequenceReferenceToken&, std::uint32_t&) noexcept override
        {
            ++unexpected;
            return ClockResult::Stopped;
        }
        ClockResult InitializeAnimationSequence(const NativeAnimationSequenceReferenceToken&) noexcept override
        {
            ++unexpected;
            return ClockResult::Stopped;
        }
        ClockResult SetAnimationSequenceBoneTag(const NativeAnimationSequenceReferenceToken&, std::int32_t) noexcept override
        {
            ++unexpected;
            return ClockResult::Stopped;
        }
        ClockResult CopyAnimationSequenceProperties(const NativeAnimationSequenceReferenceToken&, std::uintptr_t) noexcept override
        {
            ++unexpected;
            return ClockResult::Stopped;
        }
        ClockResult ReadAnimationSequenceFrame(const NativeAnimationSequenceReferenceToken&, std::size_t, std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++unexpected;
            return ClockResult::Stopped;
        }
    };
    void Policy()
    {
        static_assert(sizeof(NativeAnimationSequenceName) == 64);
        NativeAnimationSequenceName name;
        name.fill('q');
        const auto sentinel = name;
        Check(!NativeAnimationSequenceNamePolicy::Copy(nullptr, name) && name == sentinel, "null source changed typed name or pretended success");
        std::array<char, 65> overflow;
        overflow.fill('x');
        overflow[64] = 0;
        Check(!NativeAnimationSequenceNamePolicy::Copy(overflow.data(), name) && name == sentinel, "64-character source was silently truncated");
        std::size_t   length = 1234;
        std::uint32_t hash = 0x12345678u;
        Check(!NativeAnimationSequenceNamePolicy::Length(name, length) && length == 1234, "nonterminated typed value changed length output");
        Check(!NativeAnimationSequenceNamePolicy::Hash(name, hash) && hash == 0x12345678u, "nonterminated typed value published a hash");
        struct Vector
        {
            const char*   text;
            std::size_t   length;
            std::uint32_t hash;
        };
        const char                  mixedHigh[] = {'m', 'i', 'X', static_cast<char>(0xe9), 0};
        const char                  rawHigh[] = {'a', static_cast<char>(0x80), static_cast<char>(0xff), 'z', 0};
        const std::array<Vector, 4> vectors{{{"run_wuzi", 8, 0x00dbb691u}, {mixedHigh, 4, 0x633a9bccu}, {rawHigh, 4, 0x04bc41a3u}, {"", 0, 0xffffffffu}}};
        for (const auto& vector : vectors)
        {
            Check(NativeAnimationSequenceNamePolicy::Copy(vector.text, name) && NativeAnimationSequenceNamePolicy::Length(name, length) &&
                      length == vector.length && !std::memcmp(name.data(), vector.text, vector.length + 1),
                  "name copy changed raw case/high bytes/terminator");
            const auto raw = name;
            Check(NativeAnimationSequenceNamePolicy::Hash(name, hash) && hash == vector.hash && name == raw,
                  "classic ASCII normalization/raw existing CRC changed input or used CRT upper on high bytes");
        }
        std::array<char, 64> maximum;
        maximum.fill('a');
        maximum[63] = 0;
        Check(NativeAnimationSequenceNamePolicy::Copy(maximum.data(), name) && NativeAnimationSequenceNamePolicy::Length(name, length) && length == 63 &&
                  NativeAnimationSequenceNamePolicy::Hash(name, hash) && hash == 0xb88dd333u,
              "maximal63-byte name was truncated/rejected or hashed incorrectly");
        std::puts("Shared MTA bounded raw sequence name policy completed");
    }
    void Dispatch()
    {
        NativeAnimationSequenceAccess         missing;
        NativeAnimationSequenceReferenceToken empty{};
        NativeAnimationSequenceName           none{};
        Check(missing.SetAnimationSequenceName(empty, none) == ClockResult::NotInitialized, "default typed SetName did not refuse");
        Owner                                 owner;
        const auto                            max = (std::numeric_limits<std::uintptr_t>::max)();
        NativeAnimationSequenceReferenceToken token{0xfedcba9876543210ull, max - 4095u, (std::numeric_limits<std::size_t>::max)() - 7u};
        owner.expected = token;
        AnimationSequenceReceiver receiver(owner, token);
        CAnimBlendSequence&       sdk = receiver;
        token = {};
        Check(!owner.calls && !owner.unexpected, "facade construction acquired native state");
        char       input[] = {'m', 'i', 'X', static_cast<char>(0xe9), 0};
        const auto expected = std::array<char, 5>{{'m', 'i', 'X', static_cast<char>(0xe9), 0}};
        owner.changeInput = input;
        sdk.SetName(input);
        Check(input[0] == '!' && !input[1] && !std::memcmp(owner.name.data(), expected.data(), expected.size()) && owner.calls == 1 && owner.correct &&
                  !owner.unexpected,
              "SetName used caller bytes/token after callback or computed native state locally");
        owner.changeInput = nullptr;
        std::array<char, 65> overflow;
        overflow.fill('x');
        overflow[64] = 0;
        const std::array<const char*, 2> invalidInputs{{nullptr, overflow.data()}};
        for (const char* invalid : invalidInputs)
        {
            const auto before = owner.calls;
            bool       refused = false;
            try
            {
                sdk.SetName(invalid);
            }
            catch (const std::invalid_argument& error)
            {
                refused = std::string(error.what()) == "DE custom animation sequence SetName requires a nonnull NUL-terminated name within64 bytes";
            }
            Check(refused && owner.calls == before && !owner.unexpected, "null/overflow input reached owner callback or lost explicit argument refusal");
        }
        sdk.SetName("");
        Check(!owner.name[0] && owner.correct, "empty bounded name was replaced or rejected before owner admission");
        std::array<char, 64> maximum;
        maximum.fill('b');
        maximum[63] = 0;
        sdk.SetName(maximum.data());
        Check(!std::memcmp(owner.name.data(), maximum.data(), maximum.size()), "SDK did not preserve all63 input characters and NUL");
        const std::array<ClockResult, 14> errors{{ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread,
                                                  ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::WriteFailure, ClockResult::RestoreFailure,
                                                  ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired, ClockResult::UnsupportedBuild,
                                                  ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
        for (auto status : errors)
        {
            owner.result = status;
            const auto before = owner.calls;
            bool       refused = false;
            try
            {
                sdk.SetName("sourceName");
            }
            catch (const std::runtime_error& error)
            {
                refused = error.what() == std::string("DE custom animation sequence owner refused SetName: ") + std::to_string(static_cast<unsigned>(status));
            }
            Check(refused && owner.calls == before + 1 && owner.correct && !owner.unexpected,
                  "SetName refusal retried, queried old state or attempted cleanup/inverse");
        }
        owner.result = ClockResult::Ok;
        NativeAnimationSequenceReferenceToken other{7, 0x2000u, 2};
        AnimationSequenceReceiver             second(owner, other);
        owner.expected = other;
        second.SetName("second");
        Check(Same(owner.seen, other), "second receiver changed another allocation's name authority");
        owner.expected = {0xfedcba9876543210ull, max - 4095u, (std::numeric_limits<std::size_t>::max)() - 7u};
        sdk.SetName("first");
        Check(Same(owner.seen, owner.expected) && owner.correct && !owner.unexpected, "another receiver changed immutable original owner intent");
        std::puts("Actual MTA sequence SetName SDK fixture completed");
    }
}
int main()
{
    try
    {
        Policy();
        Dispatch();
        std::printf("Passed %u animation sequence name SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Sequence name SDK check failed: %s\n", error.what());
        return 1;
    }
}
