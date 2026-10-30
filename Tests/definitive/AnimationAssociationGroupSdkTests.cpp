/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual abstract group read dispatch through one copied owner token
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendAssocGroupDE.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "AnimationAssociationGroupFixture.generated.h"
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::IsCreated), decltype(&CAnimBlendAssocGroupDE::IsCreated)>);
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::GetNumAnimations), decltype(&CAnimBlendAssocGroupDE::GetNumAnimations)>);
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::GetGroupID), decltype(&CAnimBlendAssocGroupDE::GetGroupID)>);
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::IsLoaded), decltype(&CAnimBlendAssocGroupDE::IsLoaded)>);
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::GetInterface), decltype(&CAnimBlendAssocGroupDE::GetInterface)>);
static_assert(!std::is_abstract_v<CAnimBlendAssocGroupDE> && !std::is_abstract_v<AnimationAssociationGroupReceiver>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Association group SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    bool Same(const NativeAnimationAssociationGroupReferenceToken& a, const NativeAnimationAssociationGroupReferenceToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.index == b.index && a.locator == b.locator;
    }
    template <class Action>
    void Refuses(Action action, ClockResult expected, const char* operation)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.find(operation) != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationAssociationGroupAccess
    {
        NativeAnimationAssociationGroupValues          value{};
        NativeAnimationAssociationGroupReferenceToken  received{};
        NativeAnimationAssociationGroupReferenceToken* mutateCaller = nullptr;
        ClockResult                                    scalarStatus = ClockResult::Ok, loadedStatus = ClockResult::Ok;
        bool                                           loaded = false;
        unsigned                                       scalarReads = 0, loadedReads = 0, resolutions = 0;
        ClockResult ResolveAnimationAssociationGroup(std::uint64_t, std::uint32_t, NativeAnimationAssociationGroupReferenceToken&) noexcept override
        {
            ++resolutions;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken& token,
                                                  NativeAnimationAssociationGroupValues&               output) noexcept override
        {
            ++scalarReads;
            received = token;
            if (mutateCaller)
                *mutateCaller = {};
            output = value;
            return scalarStatus;
        }
        ClockResult ReadAnimationAssociationGroupLoaded(const NativeAnimationAssociationGroupReferenceToken& token, bool& output) noexcept override
        {
            ++loadedReads;
            received = token;
            if (mutateCaller)
                *mutateCaller = {};
            output = loaded;
            return loadedStatus;
        }
    };
}
int main()
{
    const NativeAnimationAssociationGroupReferenceToken original{(std::numeric_limits<std::uint64_t>::max)() - 19, 0xffffffffu,
                                                                 static_cast<std::uintptr_t>(0x12345678abcde000ull)};
    auto                                                caller = original;
    Owner                                               owner;
    owner.value.interfaceAddress = original.locator;
    AnimationAssociationGroupReceiver receiver(owner, caller);
    CAnimBlendAssocGroup&             sdk = receiver;
    caller = {};
    // An existing qualified group read has no manager enum gate: all raw
    // count/ID bits and full token index must reach the exact borrowed owner.
    for (const auto count :
         {std::int32_t{0}, std::int32_t{1}, std::int32_t{-1}, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()})
    {
        owner.value.numAnimations = count;
        const auto before = owner.scalarReads, loaded = owner.loadedReads;
        Check(sdk.IsCreated() == (count != 0));
        Check(sdk.GetNumAnimations() == count);
        Check(owner.scalarReads == before + 2 && owner.loadedReads == loaded && Same(owner.received, original));
    }
    for (const auto id :
         {std::int32_t{-1}, std::int32_t{0}, std::int32_t{139}, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()})
    {
        owner.value.groupId = id;
        Check(static_cast<int>(sdk.GetGroupID()) == id && Same(owner.received, original));
    }
    Check(reinterpret_cast<std::uintptr_t>(sdk.GetInterface()) == original.locator && Same(owner.received, original));
    // Every call obtains current source values rather than caching the first
    // snapshot or deriving Loaded from Created. The channel for Loaded alone
    // admits the optional current block; a null-block false is a real result.
    owner.value.numAnimations = 0;
    owner.scalarStatus = ClockResult::ReadFailure;
    for (const bool loaded : {false, true})
    {
        owner.loaded = loaded;
        const auto reads = owner.scalarReads, before = owner.loadedReads;
        Check(sdk.IsLoaded() == loaded);
        Check(owner.scalarReads == reads && owner.loadedReads == before + 1 && Same(owner.received, original));
    }
    owner.scalarStatus = ClockResult::Ok;
    owner.value.numAnimations = 11;
    Check(sdk.GetNumAnimations() == 11);
    owner.value.numAnimations = 27;
    Check(sdk.GetNumAnimations() == 27);
    caller = original;
    owner.mutateCaller = &caller;
    Check(sdk.GetNumAnimations() == 27 && Same(owner.received, original) && caller.locator == 0);
    caller = original;
    Check(sdk.IsLoaded() && Same(owner.received, original) && caller.ownerGeneration == 0);
    owner.mutateCaller = nullptr;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::BindingChanged,
                               ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
    {
        owner.scalarStatus = failure;
        Refuses([&] { sdk.IsCreated(); }, failure, "IsCreated");
        Refuses([&] { sdk.GetNumAnimations(); }, failure, "GetNumAnimations");
        Refuses([&] { sdk.GetGroupID(); }, failure, "GetGroupID");
        Refuses([&] { sdk.GetInterface(); }, failure, "GetInterface");
        owner.loadedStatus = failure;
        Refuses([&] { sdk.IsLoaded(); }, failure, "IsLoaded");
    }
    NativeAnimationAssociationGroupAccess absent;
    AnimationAssociationGroupReceiver     unconfigured(absent, original);
    Refuses([&] { unconfigured.IsCreated(); }, ClockResult::NotInitialized, "IsCreated");
    Refuses([&] { unconfigured.GetNumAnimations(); }, ClockResult::NotInitialized, "GetNumAnimations");
    Refuses([&] { unconfigured.GetGroupID(); }, ClockResult::NotInitialized, "GetGroupID");
    Refuses([&] { unconfigured.GetInterface(); }, ClockResult::NotInitialized, "GetInterface");
    Refuses([&] { unconfigured.IsLoaded(); }, ClockResult::NotInitialized, "IsLoaded");
    auto untouched = original;
    Check(absent.ResolveAnimationAssociationGroup(original.ownerGeneration, original.index, untouched) == ClockResult::NotInitialized &&
          Same(untouched, original));
    Owner other;
    auto  otherToken = original;
    otherToken.ownerGeneration = 0x1020304050607080ull;
    otherToken.index = 3;
    otherToken.locator = static_cast<std::uintptr_t>(0x56781234ffffd000ull);
    other.value.interfaceAddress = otherToken.locator;
    other.value.numAnimations = 3;
    AnimationAssociationGroupReceiver isolated(other, otherToken);
    Check(isolated.GetNumAnimations() == 3 && Same(other.received, otherToken));
    Check(reinterpret_cast<std::uintptr_t>(isolated.GetInterface()) == otherToken.locator);
    Check(owner.resolutions == 0 && other.resolutions == 0);
    std::puts("Actual animation association group SDK fixture completed");
    std::printf("Passed %u association group SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
