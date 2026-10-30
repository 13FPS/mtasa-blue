/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual manager uncompress dispatch with immutable same-owner H affinity
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "AnimationManagerUncompressFixture.generated.h"
using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&AnimationManagerUncompressReceiver::UncompressAnimation), decltype(&CAnimManagerDE::UncompressAnimation)>);
namespace
{
    using Token = NativeAnimationHierarchyReferenceToken;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    bool Same(const Token& a, const Token& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.locator == b.locator;
    }
    template <class F>
    void Refuses(F&& f, ClockResult expected)
    {
        bool refused = false;
        try
        {
            f();
        }
        catch (const std::runtime_error& e)
        {
            const std::string text = e.what(), tail = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.find("UncompressAnimation") != std::string::npos && text.size() >= tail.size() &&
                      text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
        }
        Check(refused, "actual manager refusal retains operation and owner result");
    }
    template <class F>
    void Invalid(F&& f)
    {
        bool refused = false;
        try
        {
            f();
        }
        catch (const std::invalid_argument& e)
        {
            refused = std::string(e.what()).find("UncompressAnimation") != std::string::npos;
        }
        Check(refused, "foreign/null facade cannot supply hierarchy authority");
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationHierarchyAccess
    {
        ClockResult  status = ClockResult::Ok;
        Token        received{};
        Token*       mutateCaller = nullptr;
        const Token* forbiddenCaller = nullptr;
        unsigned     calls = 0, scalarReads = 0, resolves = 0, calculates = 0, removes = 0, creates = 0;
        bool         aliased = false;
        ClockResult  UncompressAnimationHierarchy(const Token& token) noexcept override
        {
            ++calls;
            received = token;
            aliased = aliased || &token == forbiddenCaller;
            if (mutateCaller)
                *mutateCaller = {};
            // This is dispatch-only evidence. The real owner fixture separately
            // invokes original FF0D10 and verifies nonzero compressed effects.
            return status;
        }
        ClockResult ReadAnimationHierarchy(const Token&, NativeAnimationHierarchyValues&) noexcept override
        {
            ++scalarReads;
            return ClockResult::ReadFailure;
        }
        ClockResult ResolveBankAnimationHierarchyReference(std::uint64_t, std::uintptr_t, Token&) noexcept override
        {
            ++resolves;
            return ClockResult::ReadFailure;
        }
        ClockResult CalculatePrivateAnimationHierarchyTotalTime(const Token&) noexcept override
        {
            ++calculates;
            return ClockResult::ReadFailure;
        }
        ClockResult RemovePrivateAnimationHierarchyFromUncompressedCache(const Token&) noexcept override
        {
            ++removes;
            return ClockResult::ReadFailure;
        }
        ClockResult CreatePrivateAnimationHierarchy(std::uint64_t, Token&) noexcept override
        {
            ++creates;
            return ClockResult::ReadFailure;
        }
    };
    struct HierarchyProbe final : CAnimBlendHierarchyDE
    {
        unsigned& getters;
        HierarchyProbe(NativeAnimationHierarchyAccess& owner, const Token& token, unsigned& count) noexcept
            : CAnimBlendHierarchyDE(owner, token), getters(count)
        {
        }
        CAnimBlendHierarchySAInterface* GetInterface() override
        {
            ++getters;
            throw std::runtime_error("virtual raw getter must not replace immutable same-owner affinity");
        }
    };
    void Cases()
    {
        constexpr std::uint64_t            generation = 0xfedcba9876543210ull;
        Owner                              owner;
        AnimationManagerUncompressReceiver manager(owner, generation);
        CAnimManager&                      sdk = manager;
        unsigned                           getters = 0;
        for (const auto locator : {std::uintptr_t{1}, static_cast<std::uintptr_t>(0x12345678abcde000ull), (~std::uintptr_t{0}) - 64})
        {
            Token          caller{generation, locator};
            const auto     expected = caller;
            HierarchyProbe product(owner, caller, getters);
            owner.forbiddenCaller = &caller;
            owner.mutateCaller = &caller;
            const auto calls = owner.calls;
            sdk.UncompressAnimation(&product);
            Check(owner.calls == calls + 1 && Same(owner.received, expected), "full pointer/generation reaches exactly one owner call");
            Check(!caller.ownerGeneration && !caller.locator && !owner.aliased && !getters,
                  "callback cannot mutate copied facade affinity or trigger a virtual raw getter");
        }
        owner.forbiddenCaller = nullptr;
        owner.mutateCaller = nullptr;
        const Token    token{generation, (~std::uintptr_t{0}) - 64};
        HierarchyProbe product(owner, token, getters);
        for (const auto failure :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.status = failure;
            const auto calls = owner.calls;
            Refuses([&] { sdk.UncompressAnimation(&product); }, failure);
            Check(owner.calls == calls + 1 && Same(owner.received, token), "all owner refusals propagate without retry/default/fallback");
        }
        owner.status = ClockResult::Ok;
        const auto before = owner.calls;
        Invalid([&] { sdk.UncompressAnimation(nullptr); });
        Owner          foreign;
        HierarchyProbe foreignProduct(foreign, token, getters);
        Invalid([&] { sdk.UncompressAnimation(&foreignProduct); });
        Check(owner.calls == before && !foreign.calls, "null or foreign owner refuses before any native command");
        for (const auto malformed : {Token{generation + 1, token.locator}, Token{0, token.locator}, Token{generation, 0}})
        {
            HierarchyProbe wrong(owner, malformed, getters);
            Refuses([&] { sdk.UncompressAnimation(&wrong); }, ClockResult::BindingChanged);
            Check(owner.calls == before, "malformed generation/locator cannot issue a native command");
        }
        struct Missing final : NativeAnimationManagerAccess
        {
        } missing;
        AnimationManagerUncompressReceiver absent(missing, generation), zero(owner, 0);
        Refuses([&] { absent.UncompressAnimation(&product); }, ClockResult::NotInitialized);
        Refuses([&] { zero.UncompressAnimation(&product); }, ClockResult::NotInitialized);
        struct Defaults final : NativeAnimationManagerAccess, NativeAnimationHierarchyAccess
        {
        } defaults;
        AnimationManagerUncompressReceiver defaultManager(defaults, generation);
        HierarchyProbe                     defaultProduct(defaults, token, getters);
        Refuses([&] { defaultManager.UncompressAnimation(&defaultProduct); }, ClockResult::NotInitialized);
        const Token                        otherToken{generation + 7, std::uintptr_t{0x8800}};
        AnimationManagerUncompressReceiver otherManager(foreign, otherToken.ownerGeneration);
        HierarchyProbe                     otherProduct(foreign, otherToken, getters);
        otherManager.UncompressAnimation(&otherProduct);
        Check(foreign.calls == 1 && Same(foreign.received, otherToken) && owner.calls == before,
              "independent owner/context receives only its own immutable hierarchy token");
        Check(!getters && !owner.scalarReads && !owner.resolves && !owner.calculates && !owner.removes && !owner.creates,
              "uncompress frontend is not raw lookup, cached-time write, cache-removal or private allocation fallback");
        std::puts("Actual manager uncompress animation SDK fixture completed");
        std::puts("Actual same-owner hierarchy token dispatch SDK fixture completed");
    }
}
int main()
{
    try
    {
        Cases();
        std::printf("Passed %u manager uncompress SDK checks (%zu-bit; dispatch only, native effects use actual owner fixture)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL after %u manager uncompress SDK checks: %s\n", checks, e.what());
        return 1;
    }
}
