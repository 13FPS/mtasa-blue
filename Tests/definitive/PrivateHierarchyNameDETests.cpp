/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual hierarchy SetName source hash and copied owner dispatch
 ****************************************************************************/
#include "AnimationHierarchyDEFixture.h"
#include "../../Client/game_de/NativeClassicAnimationNameHash.h"
#include "../../Client/game_de/CKeyGenDE.h"
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
        std::uint32_t                          seenHash = 0;
        unsigned                               calls = 0;
        bool                                   correct = true;
        char*                                  rewrite = nullptr;
        ClockResult SetPrivateAnimationHierarchyNameHash(const NativeAnimationHierarchyReferenceToken& token, std::uint32_t hash) noexcept override
        {
            ++calls;
            seen = token;
            seenHash = hash;
            correct = correct && Same(token, expected);
            if (rewrite)
                *rewrite = '\0';
            return result;
        }
    };
    template <class Call>
    void Refuses(ClockResult status, Call call)
    {
        bool refused = false;
        try
        {
            call();
        }
        catch (const std::runtime_error& error)
        {
            refused = error.what() == std::string("DE animation hierarchy owner refused SetName: ") + std::to_string(static_cast<unsigned>(status));
        }
        Check(refused, "SetName lost exact owner refusal or pretended success");
    }
}
int main()
{
    try
    {
        NativeAnimationHierarchyAccess         missing;
        NativeAnimationHierarchyReferenceToken token{0xfedcba9876543210ull, (std::numeric_limits<std::uintptr_t>::max)() - 511u};
        const auto                             saved = token;
        Check(missing.SetPrivateAnimationHierarchyNameHash(token, 0xffffffffu) == ClockResult::NotInitialized && Same(token, saved),
              "optional name setter pretended success or changed caller intent");
        CKeyGenDE keys;
        Owner     owner;
        owner.expected = token;
        {
            AnimationHierarchyReceiver receiver(owner, token);
            CAnimBlendHierarchy&       sdk = receiver;
            token = {};
            Check(!owner.calls, "constructing the facade queried or adopted native state");
            const std::array<const char*, 5> inputs{{"", "mix", "MiX", "Already_UPPER", "az09_-"}};
            const std::array<const char*, 5> normalized{{"", "MIX", "MIX", "ALREADY_UPPER", "AZ09_-"}};
            for (std::size_t index = 0; index != inputs.size(); ++index)
            {
                const auto expected = keys.GetKey(normalized[index]);
                Check(NativeClassicAnimationNamePolicy::Hash(inputs[index]) == expected, "classic name policy changed raw key or ASCII fold");
                const auto before = owner.calls;
                sdk.SetName(inputs[index]);
                Check(owner.calls == before + 1 && owner.seenHash == expected && owner.correct && Same(owner.seen, saved),
                      "actual SetName did not send one copied token/hash");
            }
            const char bytes[]{'a', '[', '{', '`', 'z', static_cast<char>(0x80), static_cast<char>(0xff), 0};
            const char folded[]{'A', '[', '{', '`', 'Z', static_cast<char>(0x80), static_cast<char>(0xff), 0};
            sdk.SetName(bytes);
            Check(owner.seenHash == keys.GetKey(folded), "name policy used CRT locale, folded punctuation or refused source highbytes");
            std::string longName(4097, 'a'), longFolded(4097, 'A');
            longName[128] = longFolded[128] = static_cast<char>(0xe9);
            longName.back() = 'z';
            longFolded.back() = 'Z';
            sdk.SetName(longName.c_str());
            Check(owner.seenHash == keys.GetKey(longFolded.c_str()) &&
                      owner.seenHash != keys.GetKey("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"),
                  "hierarchy name was truncated to sequence64 storage or narrowed through sized GetKey");
            const char embedded[]{'m', 'i', 'x', 0, 'q', 'q', 0};
            sdk.SetName(embedded);
            Check(owner.seenHash == keys.GetKey("MIX"), "NUL-string policy consumed the source tail beyond its terminator");
            char       mutableInput[]{'m', 'i', 'x', static_cast<char>(0xe9), 0};
            const char mutableFolded[]{'M', 'I', 'X', static_cast<char>(0xe9), 0};
            owner.rewrite = mutableInput;
            sdk.SetName(mutableInput);
            owner.rewrite = nullptr;
            Check(!mutableInput[0] && owner.seenHash == keys.GetKey(mutableFolded),
                  "name bytes were consulted after owner entry instead of copied into a hash");
            auto before = owner.calls;
            bool nullRefused = false;
            try
            {
                sdk.SetName(nullptr);
            }
            catch (const std::invalid_argument& error)
            {
                nullRefused = std::string(error.what()) == "DE animation hierarchy SetName requires a nonnull name";
            }
            Check(nullRefused && owner.calls == before, "null name entered the owner or lost its safe SDK refusal");
            const std::array<ClockResult, 14> errors{
                {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                 ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
                 ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
            for (auto status : errors)
            {
                owner.result = status;
                before = owner.calls;
                Refuses(status, [&] { sdk.SetName("mix"); });
                Check(owner.calls == before + 1 && owner.seenHash == keys.GetKey("MIX") && owner.correct, "name refusal retried or changed source intent");
            }
            owner.result = ClockResult::Ok;
            NativeAnimationHierarchyReferenceToken other{7, 0x123400u};
            AnimationHierarchyReceiver             second(owner, other);
            owner.expected = other;
            second.SetName("other");
            Check(Same(owner.seen, other) && owner.correct, "second facade reused first name token");
            owner.expected = saved;
            sdk.SetName("mix");
            Check(Same(owner.seen, saved) && owner.correct, "second facade mutated first immutable token");
        }
        const auto before = owner.calls;
        {
            AnimationHierarchyReceiver temporary(owner, saved);
        }
        Check(owner.calls == before, "CPU facade destructor adopted native name work");
        std::puts("Actual MTA private hierarchy SetName SDK completed");
        std::printf("Passed %u private hierarchy name SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8u);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Private hierarchy name SDK check failed: %s\n", error.what());
        return 1;
    }
}
