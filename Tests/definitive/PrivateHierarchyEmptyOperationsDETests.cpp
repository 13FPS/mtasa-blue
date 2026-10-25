/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual empty/cache0 hierarchy operation dispatch, not a state model
 ****************************************************************************/
#include "AnimationHierarchyDEFixture.h"
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
namespace
{
    using namespace mta::game_de;
    using animation_hierarchy_fixture::AnimationHierarchyReceiver;
    unsigned checks = 0;
    void     Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(message);
    }
    bool Same(const NativeAnimationHierarchyReferenceToken& a, const NativeAnimationHierarchyReferenceToken& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.locator == b.locator;
    }
    struct Owner final : NativeAnimationHierarchyAccess
    {
        NativeAnimationHierarchyReferenceToken expected{}, seen{};
        ClockResult                            result = ClockResult::Ok;
        unsigned                               cacheCalls = 0, removeCalls = 0;
        bool                                   correct = true;
        unsigned                               Total() const noexcept { return cacheCalls + removeCalls; }
        ClockResult RemovePrivateAnimationHierarchyFromUncompressedCache(const NativeAnimationHierarchyReferenceToken& token) noexcept override
        {
            ++cacheCalls;
            seen = token;
            correct = correct && Same(token, expected);
            return result;
        }
        ClockResult RemoveEmptyPrivateAnimationHierarchySequences(const NativeAnimationHierarchyReferenceToken& token) noexcept override
        {
            ++removeCalls;
            seen = token;
            correct = correct && Same(token, expected);
            return result;
        }
    };
    template <class Call>
    void Refuses(const char* operation, ClockResult status, Call call)
    {
        bool refused = false;
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            refused = error.what() == std::string("DE animation hierarchy owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(status));
        }
        Check(refused, "operation lost exact refusal/status or silently pretended success");
    }
}
int main()
{
    try
    {
        NativeAnimationHierarchyAccess         missing;
        NativeAnimationHierarchyReferenceToken token{0xfedcba9876543210ull, (std::numeric_limits<std::uintptr_t>::max)() - 2047u};
        const auto                             saved = token;
        Check(missing.RemovePrivateAnimationHierarchyFromUncompressedCache(token) == ClockResult::NotInitialized && Same(token, saved),
              "default cache service was an unconditional no-op");
        Check(missing.RemoveEmptyPrivateAnimationHierarchySequences(token) == ClockResult::NotInitialized && Same(token, saved),
              "default empty removal invented cleanup permission");
        Owner owner;
        owner.expected = token;
        {
            AnimationHierarchyReceiver receiver(owner, token);
            CAnimBlendHierarchy&       sdk = receiver;
            token = {};
            Check(!owner.Total(), "CPU facade construction queried or adopted native state");
            for (unsigned repeat = 0; repeat != 3; ++repeat)
            {
                const auto cacheBefore = owner.cacheCalls;
                const auto removeBefore = owner.removeCalls;
                sdk.RemoveFromUncompressedCache();
                Check(owner.cacheCalls == cacheBefore + 1 && owner.removeCalls == removeBefore && owner.correct && Same(owner.seen, saved),
                      "cache0 operation skipped current owner admission, changed token or called another channel");
                sdk.RemoveAnimSequences();
                Check(owner.cacheCalls == cacheBefore + 1 && owner.removeCalls == removeBefore + 1 && owner.correct,
                      "empty removal was cached or replaced by another cleanup service");
            }
            const std::array<ClockResult, 14> errors{
                {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                 ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
                 ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
            for (auto status : errors)
            {
                owner.result = status;
                auto       cacheBefore = owner.cacheCalls;
                const auto removeBefore = owner.removeCalls;
                Refuses("RemoveFromUncompressedCache", status, [&] { sdk.RemoveFromUncompressedCache(); });
                Check(owner.cacheCalls == cacheBefore + 1 && owner.removeCalls == removeBefore && owner.correct,
                      "cache refusal replayed, detached children or ignored owner status");
                cacheBefore = owner.cacheCalls;
                Refuses("RemoveAnimSequences", status, [&] { sdk.RemoveAnimSequences(); });
                Check(owner.removeCalls == removeBefore + 1 && owner.cacheCalls == cacheBefore && owner.correct,
                      "empty removal refusal retried or substituted a cache/free/detach service");
            }
            owner.result = ClockResult::Ok;
            NativeAnimationHierarchyReferenceToken other{7, 0x123400u};
            AnimationHierarchyReceiver             second(owner, other);
            owner.expected = other;
            second.RemoveFromUncompressedCache();
            second.RemoveAnimSequences();
            Check(Same(owner.seen, other) && owner.correct, "second facade reused first cleanup intent");
            owner.expected = saved;
            sdk.RemoveAnimSequences();
            Check(Same(owner.seen, saved) && owner.correct, "second facade mutated first immutable token");
        }
        const auto before = owner.Total();
        {
            AnimationHierarchyReceiver temporary(owner, saved);
        }
        Check(owner.Total() == before, "CPU facade destructor silently removed children or adopted cleanup");
        std::puts("Actual MTA private hierarchy empty operations SDK completed");
        std::printf("Passed %u private hierarchy empty operation SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Private hierarchy empty operation SDK check failed: %s\n", error.what());
        return 1;
    }
}
