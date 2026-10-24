/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual private hierarchy initialization and unknown-name dispatch
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
        NativeAnimationHierarchyValues         value{};
        ClockResult                            result = ClockResult::Ok;
        unsigned                               initializes = 0, reads = 0;
        bool                                   correct = true;
        ClockResult                            InitializePrivateAnimationHierarchy(const NativeAnimationHierarchyReferenceToken& token) noexcept override
        {
            ++initializes;
            seen = token;
            correct = correct && Same(token, expected);
            return result;
        }
        ClockResult ReadAnimationHierarchy(const NativeAnimationHierarchyReferenceToken& token, NativeAnimationHierarchyValues& output) noexcept override
        {
            ++reads;
            seen = token;
            correct = correct && Same(token, expected);
            output = value;
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
        Check(refused, "SDK lost exact refusal operation/status or exposed failed output");
    }
}
int main()
{
    try
    {
        NativeAnimationHierarchyAccess         missing;
        NativeAnimationHierarchyReferenceToken token{0xfedcba9876543210ull, (std::numeric_limits<std::uintptr_t>::max)() - 127u};
        const auto                             saved = token;
        Check(missing.CreatePrivateAnimationHierarchy(token.ownerGeneration, token) == ClockResult::NotInitialized && Same(token, saved),
              "optional create invented an output or resource");
        Check(missing.InitializePrivateAnimationHierarchy(token) == ClockResult::NotInitialized && Same(token, saved), "optional initialize pretended success");
        Check(missing.DestroyPrivateAnimationHierarchy(token) == ClockResult::NotInitialized && Same(token, saved), "optional destroy pretended success");
        NativeAnimationHierarchyValues bank;
        Check(bank.hashDefined, "existing bank values lost their source-defined hash default");
        Owner owner;
        owner.expected = token;
        {
            AnimationHierarchyReceiver receiver(owner, token);
            CAnimBlendHierarchy&       sdk = receiver;
            token = {};
            Check(!owner.initializes && !owner.reads, "CPU facade construction adopted storage or queried the owner");
            sdk.Initialize();
            Check(owner.initializes == 1 && !owner.reads && owner.correct && Same(owner.seen, saved),
                  "actual SDK initialization did not copy/forward exactly once");
            const std::array<ClockResult, 14> errors{
                {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                 ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
                 ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
            for (auto error : errors)
            {
                owner.result = error;
                const auto before = owner.initializes;
                Refuses("Initialize", error, [&] { sdk.Initialize(); });
                Check(owner.initializes == before + 1 && !owner.reads && owner.correct, "initialization retried, cached or read another service");
            }
            owner.result = ClockResult::Ok;
            owner.value.hashDefined = false;
            owner.value.hash = 0xdeadbeefu;  // Failed field output must never escape as a defined hash.
            const auto before = owner.reads;
            Refuses("GetNameHashKey", ClockResult::NotInitialized, [&] { sdk.GetNameHashKey(); });
            Check(owner.reads == before + 1 && owner.correct, "unknown hash retried or skipped current qualification");
            owner.value.totalTimeBits = 0;
            owner.value.sequenceCount = 7;
            owner.value.blockId = -1;
            owner.value.interfaceAddress = saved.locator;
            Check(sdk.GetTotalTime() == 0.0f && sdk.GetNumSequences() == 7 && sdk.GetAnimBlockID() == -1 && sdk.IsCustom(),
                  "unknown name hash incorrectly blocked independent initialized fields");
            Check(reinterpret_cast<std::uintptr_t>(sdk.GetInterface()) == saved.locator, "unknown hash blocked or narrowed the admitted opaque locator");
            owner.result = ClockResult::BindingChanged;
            Refuses("GetNameHashKey", ClockResult::BindingChanged, [&] { sdk.GetNameHashKey(); });
            owner.result = ClockResult::Ok;
            owner.value.hashDefined = true;
            for (auto hash : {0u, 0xffffffffu})
            {
                owner.value.hash = hash;
                Check(sdk.GetNameHashKey() == hash, "defined raw hash bits changed or became a special missing-name value");
            }
            NativeAnimationHierarchyReferenceToken other{1, 0x2000u};
            AnimationHierarchyReceiver             second(owner, other);
            owner.expected = other;
            second.Initialize();
            Check(Same(owner.seen, other), "second facade reused first initialization intent");
            owner.expected = saved;
            sdk.Initialize();
            Check(Same(owner.seen, saved) && owner.correct, "second facade mutated first copied intent");
        }
        const auto count = owner.initializes + owner.reads;
        {
            AnimationHierarchyReceiver temporary(owner, saved);
        }
        Check(owner.initializes + owner.reads == count, "CPU-only facade destruction adopted native retirement");
        std::puts("Actual MTA private hierarchy SDK initialization and unknown-hash refusal completed");
        std::printf("Passed %u private hierarchy SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Private hierarchy SDK check failed: %s\n", error.what());
        return 1;
    }
}
