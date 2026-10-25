/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual manager cache overloads keep current private owner admission
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include "../../Client/game_de/CAnimBlendSequenceDE.h"
#include <game/CAnimBlock.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <initializer_list>
using namespace mta::game_de;
#include "PrivateAnimationFactoryFixture.generated.h"
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Manager cache SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    template <class Action>
    void Refuses(Action action, ClockResult expected)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string message(error.what());
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = message.find("RemoveFromUncompressedCache") != std::string::npos && message.size() >= suffix.size() &&
                      message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    template <class Action>
    void Invalid(Action action)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::invalid_argument&)
        {
            refused = true;
        }
        Check(refused);
    }
    bool Equal(const NativeAnimationHierarchyReferenceToken& a, const NativeAnimationHierarchyReferenceToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.locator == b.locator;
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationHierarchyAccess, NativeAnimationSequenceAccess, NativeAnimationSequenceStorageAccess
    {
        const std::uint64_t                    generation = (std::numeric_limits<std::uint64_t>::max)() - 13;
        const std::uintptr_t                   address = (std::numeric_limits<std::uintptr_t>::max)() - 0x4000;
        NativeAnimationHierarchyReferenceToken resolved{generation, address}, received{};
        std::uint64_t                          requestedGeneration = 0;
        std::uintptr_t                         requestedAddress = 0;
        unsigned                               resolutions = 0, removals = 0, forbidden = 0;
        ClockResult                            resolveResult = ClockResult::Ok, removeResult = ClockResult::Ok;
        CAnimBlendHierarchySAInterface**       mutateRaw = nullptr;
        CAnimBlendHierarchy**                  mutateTyped = nullptr;
        ClockResult                            ResolvePrivateAnimationHierarchyReference(std::uint64_t gen, std::uintptr_t raw,
                                                                                         NativeAnimationHierarchyReferenceToken& output) noexcept override
        {
            ++resolutions;
            requestedGeneration = gen;
            requestedAddress = raw;
            if (mutateRaw)
                *mutateRaw = nullptr;
            if (resolveResult == ClockResult::Ok)
                output = resolved;
            return resolveResult;
        }
        ClockResult RemovePrivateAnimationHierarchyFromUncompressedCache(const NativeAnimationHierarchyReferenceToken& token) noexcept override
        {
            ++removals;
            received = token;
            if (mutateTyped)
                *mutateTyped = nullptr;
            return removeResult;
        }
        ClockResult ReadAnimationHierarchy(const NativeAnimationHierarchyReferenceToken&, NativeAnimationHierarchyValues&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult ResolveBankAnimationHierarchyReference(std::uint64_t, std::uintptr_t, NativeAnimationHierarchyReferenceToken&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult DestroyPrivateAnimationHierarchy(const NativeAnimationHierarchyReferenceToken&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult AllocateAnimationSequences(std::uint64_t, std::size_t, std::uintptr_t&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult AllocateAnimationKeyFrames(std::uint64_t, std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult FreeAnimationSequences(std::uint64_t, std::uintptr_t) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult FreeAnimationKeyFrames(std::uint64_t, std::uintptr_t) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
    };
    class Hierarchy final : public CAnimBlendHierarchyDE
    {
    public:
        Hierarchy(NativeAnimationHierarchyAccess& owner, const NativeAnimationHierarchyReferenceToken& token) noexcept : CAnimBlendHierarchyDE(owner, token) {}
        CAnimBlendHierarchySAInterface* GetInterface() override
        {
            ++interfaceReads;
            return nullptr;
        }
        unsigned interfaceReads = 0;
    };
}
int main()
{
    Owner                                  owner;
    PrivateAnimationFactoryManagerReceiver manager(owner, owner, owner, owner, owner.generation);
    Hierarchy                              hierarchy(owner, owner.resolved);
    CAnimBlendHierarchy*                   typed = &hierarchy;
    owner.mutateTyped = &typed;
    manager.RemoveFromUncompressedCache(typed);
    Check(!typed && owner.removals == 1 && Equal(owner.received, owner.resolved));
    Check(hierarchy.interfaceReads == 0 && owner.resolutions == 0 && owner.forbidden == 0);
    owner.mutateTyped = nullptr;
    auto* raw = reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.address);
    owner.mutateRaw = &raw;
    manager.RemoveFromUncompressedCache(raw);
    Check(!raw && owner.resolutions == 1 && owner.removals == 2);
    Check(owner.requestedGeneration == owner.generation && owner.requestedAddress == owner.address && Equal(owner.received, owner.resolved));
    owner.mutateRaw = nullptr;
    raw = reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.address);
    const auto beforeNull = owner.resolutions;
    Invalid([&] { manager.RemoveFromUncompressedCache(static_cast<CAnimBlendHierarchy*>(nullptr)); });
    Refuses([&] { manager.RemoveFromUncompressedCache(static_cast<CAnimBlendHierarchySAInterface*>(nullptr)); }, ClockResult::InvalidArgument);
    Check(owner.resolutions == beforeNull && owner.removals == 2);
    Owner     other;
    Hierarchy foreign(other, owner.resolved);
    Invalid([&] { manager.RemoveFromUncompressedCache(static_cast<CAnimBlendHierarchy*>(&foreign)); });
    for (unsigned invalid = 0; invalid != 2; ++invalid)
    {
        auto token = owner.resolved;
        if (invalid == 0)
            --token.ownerGeneration;
        else
            token.locator = 0;
        Hierarchy stale(owner, token);
        Refuses([&] { manager.RemoveFromUncompressedCache(static_cast<CAnimBlendHierarchy*>(&stale)); }, ClockResult::BindingChanged);
    }
    Check(owner.removals == 2 && owner.forbidden == 0);
    for (const auto failure :
         {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::BindingChanged, ClockResult::OwnerConflict})
    {
        owner.resolveResult = failure;
        const auto before = owner.removals;
        Refuses([&] { manager.RemoveFromUncompressedCache(raw); }, failure);
        Check(owner.removals == before);
    }
    owner.resolveResult = ClockResult::Ok;
    const auto valid = owner.resolved;
    for (unsigned invalid = 0; invalid != 2; ++invalid)
    {
        owner.resolved = valid;
        if (invalid == 0)
            --owner.resolved.ownerGeneration;
        else
            --owner.resolved.locator;
        const auto before = owner.removals;
        Refuses([&] { manager.RemoveFromUncompressedCache(raw); }, ClockResult::BindingChanged);
        Check(owner.removals == before);
    }
    owner.resolved = valid;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::BindingChanged, ClockResult::Busy,
                               ClockResult::OriginalException})
    {
        owner.removeResult = failure;
        const auto before = owner.removals;
        Refuses([&] { manager.RemoveFromUncompressedCache(static_cast<CAnimBlendHierarchy*>(&hierarchy)); }, failure);
        Refuses([&] { manager.RemoveFromUncompressedCache(raw); }, failure);
        Check(owner.removals == before + 2 && owner.forbidden == 0 && hierarchy.interfaceReads == 0);
    }
    PrivateAnimationFactoryManagerReceiver observer(owner, owner.generation);
    const auto                             beforeObserver = owner.resolutions;
    Refuses([&] { observer.RemoveFromUncompressedCache(static_cast<CAnimBlendHierarchy*>(&hierarchy)); }, ClockResult::NotInitialized);
    Refuses([&] { observer.RemoveFromUncompressedCache(raw); }, ClockResult::NotInitialized);
    Check(owner.resolutions == beforeObserver && owner.forbidden == 0);
    std::puts("Actual animation manager cache SDK fixture completed");
    std::printf("Passed %u manager cache SDK checks (%zu-bit; transport only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
