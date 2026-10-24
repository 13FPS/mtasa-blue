/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Only the two new actual sequence copy and frame-view SDK methods
 ****************************************************************************/
#include "AnimationSequenceDEFixture.h"
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    using namespace mta::game_de;
    using animation_sequence_fixture::AnimationSequenceReceiver;
    unsigned checks = 0;
    void     Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(message);
    }
    bool Same(const NativeAnimationSequenceReferenceToken& a, const NativeAnimationSequenceReferenceToken& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.firstTag == b.firstTag && a.index == b.index;
    }
    struct Owner final : NativeAnimationSequenceAccess
    {
        NativeAnimationSequenceReferenceToken expected{}, seen{};
        ClockResult                           result = ClockResult::Ok;
        unsigned                              copies = 0, views = 0, unexpected = 0;
        std::uintptr_t                        source = 0, output = 0;
        std::size_t                           index = 0;
        std::uint32_t                         stride = 0;
        bool                                  correctToken = true;
        CAnimBlendSequenceSAInterface**       changeCaller = nullptr;
        void                                  Reset() noexcept
        {
            copies = views = unexpected = 0;
            correctToken = true;
        }
        ClockResult CopyAnimationSequenceProperties(const NativeAnimationSequenceReferenceToken& token, std::uintptr_t locator) noexcept override
        {
            ++copies;
            seen = token;
            correctToken = correctToken && Same(token, expected);
            source = locator;
            if (changeCaller)
                *changeCaller = nullptr;
            return result;
        }
        ClockResult ReadAnimationSequenceFrame(const NativeAnimationSequenceReferenceToken& token, std::size_t frame, std::uint32_t bytes,
                                               std::uintptr_t& value) noexcept override
        {
            ++views;
            seen = token;
            correctToken = correctToken && Same(token, expected);
            index = frame;
            stride = bytes;
            value = output;
            return result;
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
    };
    template <class Call>
    void Refusal(Owner& owner, const char* operation, ClockResult status, bool copy, Call call)
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
            refused =
                error.what() == std::string("DE custom animation sequence owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(status));
        }
        Check(refused, "new operation lost exact owner status or returned a usable value after refusal");
        Check(owner.copies == (copy ? 1u : 0u) && owner.views == (copy ? 0u : 1u) && !owner.unexpected && owner.correctToken,
              "new operation retried, queried old metadata or attempted inverse cleanup");
    }
}

int main()
{
    try
    {
        NativeAnimationSequenceAccess         missing;
        NativeAnimationSequenceReferenceToken empty{};
        std::uintptr_t                        untouched = 0x12345678u;
        Check(missing.CopyAnimationSequenceProperties(empty, 0) == ClockResult::NotInitialized,
              "default CopyProperties must refuse rather than silently accept a copy");
        Check(missing.ReadAnimationSequenceFrame(empty, (std::numeric_limits<std::size_t>::max)(), 0, untouched) == ClockResult::NotInitialized &&
                  untouched == 0x12345678u,
              "default frame view changed output or hid missing owner");

        Owner                                 owner;
        const auto                            max = (std::numeric_limits<std::uintptr_t>::max)();
        const auto                            maxIndex = (std::numeric_limits<std::size_t>::max)();
        NativeAnimationSequenceReferenceToken callerToken{0xfedcba9876543210ull, max - 4095u, maxIndex - 3u};
        owner.expected = callerToken;
        AnimationSequenceReceiver receiver(owner, callerToken);
        CAnimBlendSequence&       sdk = receiver;
        callerToken = {};
        Check(!owner.copies && !owner.views && !owner.unexpected, "constructing a partial facade queried or allocated native storage");
        for (const auto locator : {std::uintptr_t{1}, owner.expected.firstTag, max - 31u})
        {
            owner.Reset();
            auto* caller = reinterpret_cast<CAnimBlendSequenceSAInterface*>(locator);
            owner.changeCaller = &caller;
            sdk.CopySequenceProperties(caller);
            Check(!caller && owner.source == locator && owner.copies == 1 && !owner.views && !owner.unexpected && owner.correctToken,
                  "actual base CopyProperties changed source width/token or used caller pointer after callback");
        }
        owner.changeCaller = nullptr;
        owner.output = max - 127u;
        const std::array<std::size_t, 5>   indices{{0, 1, 65535, 65536, maxIndex}};
        const std::array<std::uint32_t, 3> strides{{0, 1, 0xffffffffu}};
        for (auto index : indices)
            for (auto stride : strides)
            {
                owner.Reset();
                Check(reinterpret_cast<std::uintptr_t>(sdk.GetKeyFrame(index, stride)) == owner.output && owner.index == index && owner.stride == stride &&
                          owner.views == 1 && !owner.copies && !owner.unexpected && owner.correctToken,
                      "actual GetKeyFrame narrowed/clamped inputs, inferred count/format or calculated its own address");
            }
        // Zero stride with SIZE_MAX is intentionally sent to the same owner;
        // the wrapper cannot substitute a WORD count or reject multiplication.
        Refusal(owner, "CopySequenceProperties", ClockResult::InvalidArgument, true, [&] { sdk.CopySequenceProperties(nullptr); });
        Check(!owner.source, "null source was prefiltered or replaced before the owner admitted it");
        const std::array<ClockResult, 14> errors{{ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread,
                                                  ClockResult::BindingChanged, ClockResult::ReadFailure, ClockResult::WriteFailure, ClockResult::RestoreFailure,
                                                  ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired, ClockResult::UnsupportedBuild,
                                                  ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
        for (auto error : errors)
        {
            Refusal(owner, "CopySequenceProperties", error, true,
                    [&] { sdk.CopySequenceProperties(reinterpret_cast<CAnimBlendSequenceSAInterface*>(max - 31u)); });
            Refusal(owner, "GetKeyFrame", error, false, [&] { sdk.GetKeyFrame(maxIndex, 0xffffffffu); });
        }
        owner.result = ClockResult::Ok;
        owner.Reset();
        NativeAnimationSequenceReferenceToken another{7, 0x2000u, 2};
        AnimationSequenceReceiver             second(owner, another);
        owner.expected = another;
        second.CopySequenceProperties(reinterpret_cast<CAnimBlendSequenceSAInterface*>(0x3000u));
        Check(Same(owner.seen, another), "second receiver copied through the first receiver's row");
        owner.expected = {0xfedcba9876543210ull, max - 4095u, maxIndex - 3u};
        sdk.GetKeyFrame(0, 0);
        Check(Same(owner.seen, owner.expected) && owner.correctToken && !owner.unexpected, "another receiver changed immutable destination intent");
        std::puts("Actual MTA sequence copy and frame-view SDK fixture completed");
        std::printf("Passed %u custom animation sequence copy/frame SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Custom sequence copy/frame SDK check failed: %s\n", error.what());
        return 1;
    }
}
