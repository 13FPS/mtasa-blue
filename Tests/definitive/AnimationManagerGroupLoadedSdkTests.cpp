/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual manager loaded-query transport over existing same-owner reads
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "AnimationManagerGroupLoadedFixture.generated.h"
using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&AnimationManagerGroupLoadedReceiver::HasAnimGroupLoaded), decltype(&CAnimManagerDE::HasAnimGroupLoaded)>);
namespace
{
    using Token = NativeAnimationAssociationGroupReferenceToken;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    bool Same(const Token& a, const Token& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.index == b.index && a.locator == b.locator;
    }
    template <class F>
    void Refuses(F&& operation, ClockResult expected)
    {
        bool refused = false;
        try
        {
            operation();
        }
        catch (const std::runtime_error& e)
        {
            const std::string text = e.what(), tail = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.find("HasAnimGroupLoaded") != std::string::npos && text.size() >= tail.size() &&
                      text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
        }
        Check(refused, "actual SDK refusal preserves operation and exact existing-owner result");
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationAssociationGroupAccess
    {
        Token         reply{}, received{};
        ClockResult   resolveResult = ClockResult::Ok, readResult = ClockResult::Ok;
        std::uint64_t receivedGeneration = 0;
        std::uint32_t receivedIndex = 0;
        unsigned      resolves = 0, reads = 0, unrelated = 0, mutation = 0;
        bool          loadedReply = false;
        ClockResult   ResolveAnimationAssociationGroup(std::uint64_t generation, std::uint32_t index, Token& output) noexcept override
        {
            ++resolves;
            receivedGeneration = generation;
            receivedIndex = index;
            output = reply;
            return resolveResult;
        }
        ClockResult ReadAnimationAssociationGroupLoaded(const Token& token, bool& output) noexcept override
        {
            ++reads;
            received = token;
            // This fixture supplies only the existing admitted read result.
            // Source evidence separately qualifies canonical0/1/null policy;
            // no substitute native layout or group lifetime is supplied here.
            output = loadedReply;
            if (mutation == 1)
                ++const_cast<Token&>(token).ownerGeneration;
            if (mutation == 2)
                ++const_cast<Token&>(token).index;
            if (mutation == 3)
                ++const_cast<Token&>(token).locator;
            return readResult;
        }
        ClockResult ReadAnimationManagerCounts(std::uint64_t, NativeAnimationManagerCounts&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadAnimationGroupCreated(std::uint64_t, std::uint32_t, bool&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadAnimationBlockIdentity(std::uint64_t, std::uint32_t, NativeAnimationBlockIdentity&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadAnimationAssociationGroupBlock(const Token&, NativeAnimationAssociationGroupBlock&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadAnimationAssociationGroup(const Token&, NativeAnimationAssociationGroupValues&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult CreateAnimationAssociationGroup(const Token&, const std::string&, NativeAnimationAssociationGroupChildBorrowInvalidator) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
    };
    void Cases()
    {
        constexpr std::uint64_t             generation = 0xfedcba9876543210ull;
        Owner                               owner;
        AnimationManagerGroupLoadedReceiver manager(owner, generation);
        CAnimManager&                       sdk = manager;
        for (const auto index :
             {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{118}, std::uint32_t{4096}, std::uint32_t{0x80000000u}, std::uint32_t{0xffffffffu}})
        {
            owner.reply = {generation, index, (std::numeric_limits<std::uintptr_t>::max)() - 64};
            for (const bool loaded : {false, true})
            {
                owner.loadedReply = loaded;
                const auto resolves = owner.resolves, reads = owner.reads;
                Check(sdk.HasAnimGroupLoaded(static_cast<AssocGroupId>(index)) == loaded,
                      "actual SDK transports canonical loaded0/1 exactly without a frontend enum clamp");
                Check(owner.resolves == resolves + 1 && owner.reads == reads + 1 && owner.receivedGeneration == generation && owner.receivedIndex == index &&
                          Same(owner.received, owner.reply),
                      "fullUInt32 ID and full pointer/generation reach exactly the same existing group owner once");
            }
            owner.loadedReply = false;
            const auto resolves = owner.resolves, reads = owner.reads;
            Check(!sdk.HasAnimGroupLoaded(static_cast<AssocGroupId>(index)), "an admitted null-block false is transported without a block facade or fallback");
            Check(owner.resolves == resolves + 1 && owner.reads == reads + 1,
                  "false still performs both current-owner channels and never means missing admission");
        }
        const Token valid{generation, 17, (std::numeric_limits<std::uintptr_t>::max)() - 64};
        owner.reply = valid;
        for (const auto failure :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.resolveResult = failure;
            const auto resolves = owner.resolves, reads = owner.reads;
            Refuses([&] { sdk.HasAnimGroupLoaded(17); }, failure);
            Check(owner.resolves == resolves + 1 && owner.reads == reads,
                  "every current-group provenance/index/owner refusal stops before loaded read without retry");
            owner.resolveResult = ClockResult::Ok;
            owner.readResult = failure;
            Refuses([&] { sdk.HasAnimGroupLoaded(17); }, failure);
            Check(owner.resolves == resolves + 2 && owner.reads == reads + 1 && Same(owner.received, valid),
                  "every existing loaded-read refusal propagates instead of false true retry or replacement");
            owner.readResult = ClockResult::Ok;
        }
        for (const auto malformed :
             {Token{generation + 1, 17, valid.locator}, Token{0, 17, valid.locator}, Token{generation, 18, valid.locator}, Token{generation, 17, 0}})
        {
            owner.reply = malformed;
            const auto reads = owner.reads;
            Refuses([&] { sdk.HasAnimGroupLoaded(17); }, malformed.ownerGeneration != generation ? ClockResult::OwnerConflict : ClockResult::BindingChanged);
            Check(owner.reads == reads, "malformed or foreign resolved token cannot be consumed as current group affinity");
        }
        owner.reply = valid;
        for (unsigned mutation = 1; mutation != 4; ++mutation)
        {
            owner.mutation = mutation;
            const auto reads = owner.reads;
            Refuses([&] { sdk.HasAnimGroupLoaded(17); }, ClockResult::BindingChanged);
            Check(owner.reads == reads + 1 && Same(owner.received, valid), "a changed callback token cannot certify a loaded result for its original group");
        }
        owner.mutation = 0;
        struct Missing final : NativeAnimationManagerAccess
        {
        } missing;
        AnimationManagerGroupLoadedReceiver absent(missing, generation), zero(owner, 0);
        const auto                          resolves = owner.resolves, reads = owner.reads;
        Refuses([&] { absent.HasAnimGroupLoaded(17); }, ClockResult::NotInitialized);
        Refuses([&] { zero.HasAnimGroupLoaded(17); }, ClockResult::NotInitialized);
        Check(owner.resolves == resolves && owner.reads == reads, "missing group service or generation refuses before either native channel");
        struct Defaults final : NativeAnimationManagerAccess, NativeAnimationAssociationGroupAccess
        {
        } defaults;
        AnimationManagerGroupLoadedReceiver defaultManager(defaults, generation);
        Refuses([&] { defaultManager.HasAnimGroupLoaded(17); }, ClockResult::NotInitialized);
        Owner other;
        other.reply = {generation + 7, 0xffffffffu, static_cast<std::uintptr_t>(0x12345678)};
        other.loadedReply = true;
        AnimationManagerGroupLoadedReceiver otherManager(other, other.reply.ownerGeneration);
        Check(otherManager.HasAnimGroupLoaded(0xffffffffu) && other.resolves == 1 && other.reads == 1 && Same(other.received, other.reply) &&
                  owner.resolves == resolves && owner.reads == reads,
              "independent actual manager uses only its own supplied group owner and generation");
        Check(!owner.unrelated && !other.unrelated, "manager query never invokes Created counts block factory group facade cached block or creation paths");
        std::puts("Actual manager group loaded SDK fixture completed");
        std::puts("Actual same-owner group loaded dispatch SDK fixture completed");
        std::puts("Actual manager group loaded source refusal SDK fixture completed");
    }
}
int main()
{
    try
    {
        Cases();
        std::printf("Passed %u manager group-loaded SDK checks (%zu-bit; transport only, existing native reader retained)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL after %u manager group-loaded SDK checks: %s\n", checks, e.what());
        return 1;
    }
}
