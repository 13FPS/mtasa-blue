/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual whole payload hierarchy operation dispatch, not a state model
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
        unsigned                               quaternionCalls = 0, timeCalls = 0;
        bool                                   correct = true;
        unsigned                               Total() const noexcept { return quaternionCalls + timeCalls; }
        ClockResult RemovePrivateAnimationHierarchyQuaternionFlips(const NativeAnimationHierarchyReferenceToken& token) noexcept override
        {
            ++quaternionCalls;
            seen = token;
            correct = correct && Same(token, expected);
            return result;
        }
        ClockResult CalculatePrivateAnimationHierarchyTotalTime(const NativeAnimationHierarchyReferenceToken& token) noexcept override
        {
            ++timeCalls;
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
        static_assert(!std::is_abstract_v<CAnimBlendHierarchyDE>);
        static_assert(!std::is_constructible_v<CAnimBlendHierarchyDE, NativeAnimationHierarchyAccess&, const NativeAnimationHierarchyReferenceToken&>);
        static_assert(std::is_same_v<decltype(&AnimationHierarchyReceiver::RemoveQuaternionFlips), decltype(&CAnimBlendHierarchyDE::RemoveQuaternionFlips)>);
        static_assert(std::is_same_v<decltype(&AnimationHierarchyReceiver::CalculateTotalTime), decltype(&CAnimBlendHierarchyDE::CalculateTotalTime)>);
        NativeAnimationHierarchyAccess         missing;
        NativeAnimationHierarchyReferenceToken token{0xfedcba9876543210ull, (std::numeric_limits<std::uintptr_t>::max)() - 2047u};
        const auto                             saved = token;
        Check(missing.RemovePrivateAnimationHierarchyQuaternionFlips(token) == ClockResult::NotInitialized && Same(token, saved),
              "default cache service was an unconditional no-op");
        Check(missing.CalculatePrivateAnimationHierarchyTotalTime(token) == ClockResult::NotInitialized && Same(token, saved),
              "default time operation invented cleanup permission");
        Owner owner;
        owner.expected = token;
        {
            AnimationHierarchyReceiver receiver(owner, token);
            CAnimBlendHierarchy&       sdk = receiver;
            token = {};
            Check(!owner.Total(), "CPU facade construction queried or adopted native state");
            for (unsigned repeat = 0; repeat != 3; ++repeat)
            {
                const auto quaternionBefore = owner.quaternionCalls;
                const auto timeBefore = owner.timeCalls;
                sdk.RemoveQuaternionFlips();
                Check(owner.quaternionCalls == quaternionBefore + 1 && owner.timeCalls == timeBefore && owner.correct && Same(owner.seen, saved),
                      "quaternion operation skipped current owner admission, changed token or called another channel");
                sdk.CalculateTotalTime();
                Check(owner.quaternionCalls == quaternionBefore + 1 && owner.timeCalls == timeBefore + 1 && owner.correct,
                      "time operation was cached or replaced by another cleanup service");
            }
            const std::array<ClockResult, 14> errors{
                {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                 ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
                 ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
            for (auto status : errors)
            {
                owner.result = status;
                auto       quaternionBefore = owner.quaternionCalls;
                const auto timeBefore = owner.timeCalls;
                Refuses("RemoveQuaternionFlips", status, [&] { sdk.RemoveQuaternionFlips(); });
                Check(owner.quaternionCalls == quaternionBefore + 1 && owner.timeCalls == timeBefore && owner.correct,
                      "quaternion refusal replayed, detached children or ignored owner status");
                quaternionBefore = owner.quaternionCalls;
                Refuses("CalculateTotalTime", status, [&] { sdk.CalculateTotalTime(); });
                Check(owner.timeCalls == timeBefore + 1 && owner.quaternionCalls == quaternionBefore && owner.correct,
                      "time operation refusal retried or substituted a cache/free/detach service");
            }
            owner.result = ClockResult::Ok;
            NativeAnimationHierarchyReferenceToken other{7, 0x123400u};
            AnimationHierarchyReceiver             second(owner, other);
            owner.expected = other;
            second.RemoveQuaternionFlips();
            second.CalculateTotalTime();
            Check(Same(owner.seen, other) && owner.correct, "second facade reused first cleanup intent");
            owner.expected = saved;
            sdk.CalculateTotalTime();
            Check(Same(owner.seen, saved) && owner.correct, "second facade mutated first immutable token");
        }
        const auto before = owner.Total();
        {
            AnimationHierarchyReceiver temporary(owner, saved);
        }
        Check(owner.Total() == before, "CPU facade destructor silently removed children or adopted cleanup");
        std::puts("Actual MTA private hierarchy payload SDK completed");
        std::printf("Passed %u private hierarchy payload SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Private hierarchy payload SDK check failed: %s\n", error.what());
        return 1;
    }
}
