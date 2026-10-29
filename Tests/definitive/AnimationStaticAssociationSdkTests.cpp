/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real six-getter static-association dispatch, no native factory
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendStaticAssociationDE.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "AnimationStaticAssociationFixture.generated.h"
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::GetNumBlendNodes), decltype(&CAnimBlendStaticAssociationDE::GetNumBlendNodes)>);
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::GetAnimID), decltype(&CAnimBlendStaticAssociationDE::GetAnimID)>);
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::GetAnimGroup), decltype(&CAnimBlendStaticAssociationDE::GetAnimGroup)>);
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::GetFlags), decltype(&CAnimBlendStaticAssociationDE::GetFlags)>);
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::GetAnimHierarchyInterface),
                             decltype(&CAnimBlendStaticAssociationDE::GetAnimHierarchyInterface)>);
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::GetInterface), decltype(&CAnimBlendStaticAssociationDE::GetInterface)>);
static_assert(!std::is_abstract_v<CAnimBlendStaticAssociationDE> && !std::is_abstract_v<AnimationStaticAssociationReceiver>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Static association SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    bool Same(const NativeAnimationStaticAssociationReferenceToken& a, const NativeAnimationStaticAssociationReferenceToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.groupIndex == b.groupIndex && a.index == b.index && a.locator == b.locator;
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
    struct Owner final : NativeAnimationStaticAssociationAccess
    {
        NativeAnimationStaticAssociationValues          value{};
        NativeAnimationStaticAssociationReferenceToken  received{};
        NativeAnimationStaticAssociationReferenceToken* mutateCaller = nullptr;
        ClockResult                                     status = ClockResult::Ok;
        unsigned                                        reads = 0, resolutions = 0;
        ClockResult                                     ResolveAnimationStaticAssociation(std::uint64_t, std::uint32_t, std::size_t,
                                                                                          NativeAnimationStaticAssociationReferenceToken&) noexcept override
        {
            ++resolutions;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadAnimationStaticAssociation(const NativeAnimationStaticAssociationReferenceToken& token,
                                                   NativeAnimationStaticAssociationValues&               output) noexcept override
        {
            ++reads;
            received = token;
            if (mutateCaller)
                *mutateCaller = {};
            output = value;
            return status;
        }
    };
}
int main()
{
    const NativeAnimationStaticAssociationReferenceToken original{(std::numeric_limits<std::uint64_t>::max)() - 53, 0xffffffffu,
                                                                  (std::numeric_limits<std::size_t>::max)(),
                                                                  static_cast<std::uintptr_t>(0x12345678abcde000ull)};
    auto                                                 caller = original;
    Owner                                                owner;
    owner.value.interfaceAddress = original.locator;
    AnimationStaticAssociationReceiver receiver(owner, caller);
    CAnimBlendStaticAssociation&       sdk = receiver;
    caller = {};
    for (const auto count : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0x7fff}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
    {
        owner.value.numBlendNodes = count;
        const auto before = owner.reads;
        Check(sdk.GetNumBlendNodes() == count && owner.reads == before + 1 && Same(owner.received, original));
    }
    for (const auto value :
         {std::int16_t{0}, std::int16_t{1}, std::int16_t{-1}, (std::numeric_limits<std::int16_t>::min)(), (std::numeric_limits<std::int16_t>::max)()})
    {
        owner.value.animId = value;
        owner.value.animGroup = value;
        owner.value.flags = value;
        const auto before = owner.reads;
        Check(sdk.GetAnimID() == value);
        Check(sdk.GetAnimGroup() == value);
        Check(sdk.GetFlags() == value);
        Check(owner.reads == before + 3 && Same(owner.received, original));
    }
    owner.value.animId = -1;
    owner.value.animGroup = (std::numeric_limits<std::int16_t>::min)();
    owner.value.flags = (std::numeric_limits<std::int16_t>::max)();
    Check(sdk.GetAnimID() == -1 && sdk.GetAnimGroup() == (std::numeric_limits<std::int16_t>::min)() &&
          sdk.GetFlags() == (std::numeric_limits<std::int16_t>::max)());
    // Hierarchy is only the raw observed +18 tag. Null or an unqualified target
    // cannot be hidden by adding a child-node/Created/hierarchy validity gate.
    for (const auto address : {std::uintptr_t{0}, std::uintptr_t{3}, static_cast<std::uintptr_t>(0x456789abcdef1000ull)})
    {
        owner.value.hierarchyAddress = address;
        const auto before = owner.reads;
        Check(reinterpret_cast<std::uintptr_t>(sdk.GetAnimHierarchyInterface()) == address && owner.reads == before + 1);
        Check(reinterpret_cast<std::uintptr_t>(sdk.GetInterface()) == original.locator && Same(owner.received, original));
    }
    owner.value.animId = 21;
    Check(sdk.GetAnimID() == 21);
    owner.value.animId = 39;
    Check(sdk.GetAnimID() == 39);
    caller = original;
    owner.mutateCaller = &caller;
    Check(sdk.GetAnimID() == 39 && Same(owner.received, original) && caller.index == 0);
    owner.mutateCaller = nullptr;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::BindingChanged,
                               ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
    {
        owner.status = failure;
        Refuses([&] { sdk.GetNumBlendNodes(); }, failure, "GetNumBlendNodes");
        Refuses([&] { sdk.GetAnimID(); }, failure, "GetAnimID");
        Refuses([&] { sdk.GetAnimGroup(); }, failure, "GetAnimGroup");
        Refuses([&] { sdk.GetFlags(); }, failure, "GetFlags");
        Refuses([&] { sdk.GetAnimHierarchyInterface(); }, failure, "GetAnimHierarchyInterface");
        Refuses([&] { sdk.GetInterface(); }, failure, "GetInterface");
    }
    NativeAnimationStaticAssociationAccess absent;
    AnimationStaticAssociationReceiver     unconfigured(absent, original);
    Refuses([&] { unconfigured.GetNumBlendNodes(); }, ClockResult::NotInitialized, "GetNumBlendNodes");
    Refuses([&] { unconfigured.GetAnimID(); }, ClockResult::NotInitialized, "GetAnimID");
    Refuses([&] { unconfigured.GetAnimGroup(); }, ClockResult::NotInitialized, "GetAnimGroup");
    Refuses([&] { unconfigured.GetFlags(); }, ClockResult::NotInitialized, "GetFlags");
    Refuses([&] { unconfigured.GetAnimHierarchyInterface(); }, ClockResult::NotInitialized, "GetAnimHierarchyInterface");
    Refuses([&] { unconfigured.GetInterface(); }, ClockResult::NotInitialized, "GetInterface");
    auto untouched = original;
    Check(absent.ResolveAnimationStaticAssociation(original.ownerGeneration, original.groupIndex, original.index, untouched) == ClockResult::NotInitialized &&
          Same(untouched, original));
    Owner other;
    auto  otherToken = original;
    otherToken.ownerGeneration = 0x1020304050607080ull;
    otherToken.index = 3;
    otherToken.locator = static_cast<std::uintptr_t>(0x56781234ffffd000ull);
    other.value.interfaceAddress = otherToken.locator;
    other.value.numBlendNodes = 7;
    AnimationStaticAssociationReceiver isolated(other, otherToken);
    Check(isolated.GetNumBlendNodes() == 7 && Same(other.received, otherToken));
    Check(reinterpret_cast<std::uintptr_t>(isolated.GetInterface()) == otherToken.locator);
    Check(owner.resolutions == 0 && other.resolutions == 0);
    std::puts("Actual animation static association SDK fixture completed");
    std::printf("Passed %u static association SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
