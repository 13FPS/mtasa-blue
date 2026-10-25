/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA private hierarchy scalar setters, not an engine model
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
        unsigned                               compressedCalls = 0, blockCalls = 0;
        bool                                   compressed = false, correct = true;
        std::int32_t                           block = 0;
        unsigned                               Total() const noexcept { return compressedCalls + blockCalls; }
        ClockResult SetPrivateAnimationHierarchyRunningCompressed(const NativeAnimationHierarchyReferenceToken& token, bool value) noexcept override
        {
            ++compressedCalls;
            seen = token;
            correct = correct && Same(token, expected);
            compressed = value;
            return result;
        }
        ClockResult SetPrivateAnimationHierarchyBlockID(const NativeAnimationHierarchyReferenceToken& token, std::int32_t value) noexcept override
        {
            ++blockCalls;
            seen = token;
            correct = correct && Same(token, expected);
            block = value;
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
        Check(refused, "actual scalar setter lost exact operation/status or pretended success");
    }
}
int main()
{
    try
    {
        NativeAnimationHierarchyAccess         missing;
        NativeAnimationHierarchyReferenceToken token{0xfedcba9876543210ull, (std::numeric_limits<std::uintptr_t>::max)() - 255u};
        const auto                             saved = token;
        Check(missing.SetPrivateAnimationHierarchyRunningCompressed(token, true) == ClockResult::NotInitialized && Same(token, saved),
              "default bool setter pretended success");
        Check(
            missing.SetPrivateAnimationHierarchyBlockID(token, (std::numeric_limits<std::int32_t>::min)()) == ClockResult::NotInitialized && Same(token, saved),
            "default block setter changed intent");
        Owner owner;
        owner.expected = token;
        {
            AnimationHierarchyReceiver receiver(owner, token);
            CAnimBlendHierarchy&       sdk = receiver;
            token = {};
            Check(!owner.Total(), "construction queried or mutated native storage");
            for (bool value : {false, true, false})
            {
                const auto before = owner.Total();
                sdk.SetRunningCompressed(value);
                Check(owner.Total() == before + 1 && owner.compressed == value && owner.correct && Same(owner.seen, saved),
                      "bool assignment narrowed intent or called owner twice");
            }
            for (int value : {(std::numeric_limits<int>::min)(), -65536, -1, 0, 65536, (std::numeric_limits<int>::max)()})
            {
                const auto before = owner.Total();
                sdk.SetAnimationBlockID(value);
                Check(owner.Total() == before + 1 && owner.block == value && owner.correct, "block assignment clamped, filtered or changed signed width");
            }
            const std::array<ClockResult, 14> errors{
                {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                 ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
                 ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
            for (auto status : errors)
            {
                owner.result = status;
                auto       compressedBefore = owner.compressedCalls;
                const auto blockBefore = owner.blockCalls;
                Refuses("SetRunningCompressed", status, [&] { sdk.SetRunningCompressed(true); });
                Check(owner.compressedCalls == compressedBefore + 1 && owner.blockCalls == blockBefore && owner.correct,
                      "bool refusal retried or used another channel");
                compressedBefore = owner.compressedCalls;
                Refuses("SetAnimationBlockID", status, [&] { sdk.SetAnimationBlockID(-1); });
                Check(owner.blockCalls == blockBefore + 1 && owner.compressedCalls == compressedBefore && owner.correct,
                      "block refusal retried or used another channel");
            }
            owner.result = ClockResult::Ok;
            NativeAnimationHierarchyReferenceToken other{7, 0x123400u};
            AnimationHierarchyReceiver             second(owner, other);
            owner.expected = other;
            second.SetRunningCompressed(false);
            second.SetAnimationBlockID(42);
            Check(Same(owner.seen, other) && owner.correct, "second facade reused first token");
            owner.expected = saved;
            sdk.SetAnimationBlockID(-1);
            Check(Same(owner.seen, saved) && owner.correct, "second facade mutated first immutable token");
        }
        const auto before = owner.Total();
        {
            AnimationHierarchyReceiver temporary(owner, saved);
        }
        Check(owner.Total() == before, "CPU facade destruction adopted mutation or native cleanup");
        std::puts("Actual MTA private hierarchy scalar setters SDK completed");
        std::printf("Passed %u private hierarchy scalar SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Private hierarchy scalar SDK check failed: %s\n", error.what());
        return 1;
    }
}
