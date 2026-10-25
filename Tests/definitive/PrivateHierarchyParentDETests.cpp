/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual hierarchy parent setter dispatch without child authority
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
        unsigned                               countCalls = 0, sequenceCalls = 0;
        std::uint16_t                          count = 0;
        std::uintptr_t                         sequences = 0;
        bool                                   correct = true;
        unsigned                               Total() const noexcept { return countCalls + sequenceCalls; }
        ClockResult SetPrivateAnimationHierarchySequenceCount(const NativeAnimationHierarchyReferenceToken& token, std::uint16_t value) noexcept override
        {
            ++countCalls;
            seen = token;
            correct = correct && Same(token, expected);
            count = value;
            return result;
        }
        ClockResult SetPrivateAnimationHierarchySequences(const NativeAnimationHierarchyReferenceToken& token, std::uintptr_t value) noexcept override
        {
            ++sequenceCalls;
            seen = token;
            correct = correct && Same(token, expected);
            sequences = value;
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
        Check(refused, "parent setter lost exact refusal operation/status or pretended success");
    }
}
int main()
{
    try
    {
        const auto                             max = (std::numeric_limits<std::uintptr_t>::max)();
        NativeAnimationHierarchyAccess         missing;
        NativeAnimationHierarchyReferenceToken token{0xfedcba9876543210ull, max - 1023u};
        const auto                             saved = token;
        Check(missing.SetPrivateAnimationHierarchySequenceCount(token, 0xffffu) == ClockResult::NotInitialized && Same(token, saved),
              "default count setter pretended success");
        Check(missing.SetPrivateAnimationHierarchySequences(token, 0) == ClockResult::NotInitialized && Same(token, saved),
              "default null request became generic detach permission");
        Owner owner;
        owner.expected = token;
        {
            AnimationHierarchyReceiver receiver(owner, token);
            CAnimBlendHierarchy&       sdk = receiver;
            token = {};
            Check(!owner.Total(), "CPU facade construction queried or changed child state");
            for (std::uint16_t value : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0x8000}, std::uint16_t{0xffff}, std::uint16_t{0xffff}})
            {
                const auto before = owner.Total();
                sdk.SetNumSequences(value);
                Check(owner.Total() == before + 1 && owner.count == value && owner.correct && Same(owner.seen, saved),
                      "source WORD was clamped, cached or sent through another token");
            }
            const std::array<std::uintptr_t, 6> locators{{0, 1, 0x123400u, max - 511u, max, 0}};
            for (auto locator : locators)
            {
                const auto before = owner.Total();
                sdk.SetSequences(reinterpret_cast<CAnimBlendSequenceSAInterface*>(locator));
                Check(owner.Total() == before + 1 && owner.sequences == locator && owner.correct,
                      "opaque array tag was narrowed, dereferenced or null was skipped");
            }
            const std::array<ClockResult, 14> errors{
                {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                 ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
                 ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
            for (auto status : errors)
            {
                owner.result = status;
                auto       countBefore = owner.countCalls;
                const auto sequenceBefore = owner.sequenceCalls;
                Refuses("SetNumSequences", status, [&] { sdk.SetNumSequences(0xffffu); });
                Check(owner.countCalls == countBefore + 1 && owner.sequenceCalls == sequenceBefore && owner.count == 0xffffu && owner.correct,
                      "count refusal replayed or changed source WORD");
                countBefore = owner.countCalls;
                Refuses("SetSequences", status, [&] { sdk.SetSequences(nullptr); });
                Check(owner.sequenceCalls == sequenceBefore + 1 && owner.countCalls == countBefore && owner.sequences == 0 && owner.correct,
                      "null detach refusal replayed or mutated another channel");
            }
            owner.result = ClockResult::Ok;
            NativeAnimationHierarchyReferenceToken other{7, 0x123400u};
            AnimationHierarchyReceiver             second(owner, other);
            owner.expected = other;
            second.SetNumSequences(42);
            second.SetSequences(nullptr);
            Check(Same(owner.seen, other) && owner.correct, "another facade reused first parent intent");
            owner.expected = saved;
            sdk.SetNumSequences(0);
            Check(Same(owner.seen, saved) && owner.correct, "another facade mutated the first immutable parent token");
        }
        const auto before = owner.Total();
        {
            AnimationHierarchyReceiver temporary(owner, saved);
        }
        Check(owner.Total() == before, "CPU facade destruction silently detached children");
        std::puts("Actual MTA private hierarchy parent setters SDK completed");
        std::printf("Passed %u private hierarchy parent SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Private hierarchy parent SDK check failed: %s\n", error.what());
        return 1;
    }
}
