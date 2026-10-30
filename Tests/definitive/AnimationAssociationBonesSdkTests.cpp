/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Three real SDK dispatch routes without a native ownership model
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendAssociationDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include "PrivateAnimationAssociationFixture.generated.h"
using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::GetAnimHierarchy), decltype(&CAnimBlendAssociationDE::GetAnimHierarchy)>);
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::RestrictToBones), decltype(&CAnimBlendAssociationDE::RestrictToBones)>);
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::RestrictToBonesOf), decltype(&CAnimBlendAssociationDE::RestrictToBonesOf)>);
static_assert(!std::is_abstract_v<CAnimBlendAssociationDE> && !std::is_abstract_v<PrivateAnimationAssociationReceiver>);
static_assert(!noexcept(std::declval<const CAnimBlendAssociation&>().GetCurrentProgress()));
static_assert(!noexcept(std::declval<const CAnimBlendAssociation&>().GetCurrentSpeed()));
static_assert(!noexcept(std::declval<const CAnimBlendAssociation&>().GetLength()));
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Association bones SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    bool Same(const NativeAnimationAssociationReferenceToken& a, const NativeAnimationAssociationReferenceToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.serial == b.serial;
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
            const std::string message = error.what();
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = message.find(operation) != std::string::npos && message.size() >= suffix.size() &&
                      message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationAssociationAccess, NativeAnimationHierarchyAccess
    {
        NativeAnimationAssociationReferenceToken received{}, *mutateCaller = nullptr;
        NativeAnimationHierarchyReferenceToken   hierarchy{}, readHierarchy{};
        ClockResult                              status = ClockResult::Ok, readStatus = ClockResult::Ok;
        std::uint64_t                            mask = 0;
        std::uintptr_t                           source = 0;
        std::bitset<64>*                         mutateMask = nullptr;
        unsigned                                 resolves = 0, masks = 0, originals = 0, hierarchyReads = 0, creates = 0, retires = 0;
        void                                     Capture(const NativeAnimationAssociationReferenceToken& value)
        {
            if (mutateCaller)
            {
                *mutateCaller = {0, 0};
                mutateCaller = nullptr;
            }
            if (mutateMask)
            {
                mutateMask->flip();
                mutateMask = nullptr;
            }
            received = value;
        }
        ClockResult ResolvePrivateAnimationAssociationHierarchy(const NativeAnimationAssociationReferenceToken& value,
                                                                NativeAnimationHierarchyReferenceToken&         output) noexcept override
        {
            ++resolves;
            Capture(value);
            if (status == ClockResult::Ok)
                output = hierarchy;
            return status;
        }
        ClockResult RestrictPrivateAnimationAssociationToBones(const NativeAnimationAssociationReferenceToken& value, std::uint64_t bits) noexcept override
        {
            ++masks;
            Capture(value);
            mask = bits;
            return status;
        }
        ClockResult RestrictPrivateAnimationAssociationToBonesOf(const NativeAnimationAssociationReferenceToken& value,
                                                                 std::uintptr_t                                  locator) noexcept override
        {
            ++originals;
            Capture(value);
            source = locator;
            return status;
        }
        ClockResult ReadAnimationHierarchy(const NativeAnimationHierarchyReferenceToken& value, NativeAnimationHierarchyValues& output) noexcept override
        {
            ++hierarchyReads;
            readHierarchy = value;
            if (readStatus == ClockResult::Ok)
            {
                output = {};
                output.interfaceAddress = value.locator;
                output.hash = 0xabcdef01u;
            }
            return readStatus;
        }
        ClockResult CreatePrivateAnimationAssociation(std::uint64_t, NativeAnimationAssociationReferenceToken&) noexcept override
        {
            ++creates;
            return ClockResult::NotInitialized;
        }
        ClockResult RetirePrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken&) noexcept override
        {
            ++retires;
            return ClockResult::NotInitialized;
        }
    };
    struct MissingHierarchy final : NativeAnimationAssociationAccess
    {
        unsigned    resolves = 0;
        ClockResult ResolvePrivateAnimationAssociationHierarchy(const NativeAnimationAssociationReferenceToken&,
                                                                NativeAnimationHierarchyReferenceToken&) noexcept override
        {
            ++resolves;
            return ClockResult::Ok;
        }
    };
    struct Defaults final : NativeAnimationAssociationAccess, NativeAnimationHierarchyAccess
    {
    };
}
int main()
{
    const NativeAnimationAssociationReferenceToken expected{0xfedcba9876543210ull, 0x8899aabbccddeeffull};
    const auto                                     raw = std::numeric_limits<std::uintptr_t>::max() - std::uintptr_t{0x207f};
    Owner                                          owner;
    owner.hierarchy = {expected.ownerGeneration, raw};
    {
        auto                                caller = expected;
        PrivateAnimationAssociationReceiver receiver(owner, caller);
        CAnimBlendAssociation&              association = receiver;
        caller = {1, 2};
        owner.mutateCaller = &caller;
        auto hierarchy = association.GetAnimHierarchy();
        Check(hierarchy && owner.resolves == 1 && owner.hierarchyReads == 0 && Same(owner.received, expected));
        Check(caller.ownerGeneration == 0 && caller.serial == 0);
        Check(reinterpret_cast<std::uintptr_t>(hierarchy->GetInterface()) == raw);
        Check(owner.hierarchyReads == 1 && owner.readHierarchy.ownerGeneration == expected.ownerGeneration && owner.readHierarchy.locator == raw);
        Check(hierarchy->GetNameHashKey() == 0xabcdef01u && owner.hierarchyReads == 2);
        owner.readStatus = ClockResult::OwnerConflict;
        Refuses([&] { (void)hierarchy->GetInterface(); }, ClockResult::OwnerConflict, "GetInterface");
        const auto oldResolves = owner.resolves, oldReads = owner.hierarchyReads;
        hierarchy.reset();
        Check(owner.resolves == oldResolves && owner.hierarchyReads == oldReads && owner.creates == 0 && owner.retires == 0);
        owner.readStatus = ClockResult::Ok;
        for (auto bits : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{1} << 63, std::uint64_t{0x0123456789abcdefull}, ~std::uint64_t{0}})
        {
            std::bitset<64> mask(bits);
            owner.mutateMask = &mask;
            const auto before = owner.masks;
            association.RestrictToBones(mask);
            Check(owner.masks == before + 1 && owner.mask == bits && Same(owner.received, expected));
            Check(mask.to_ullong() == ~bits);
        }
        // Opaque locators are never dereferenced by this SDK fixture. Null
        // still reaches the owner, which owns the source no-map/no-op policy.
        for (auto locator : {std::uintptr_t{0}, std::uintptr_t{1}, raw})
        {
            const auto before = owner.originals;
            association.RestrictToBonesOf(reinterpret_cast<const CAnimBlendStaticAssociationSAInterface*>(locator));
            Check(owner.originals == before + 1 && owner.source == locator && Same(owner.received, expected));
        }
        for (auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::Expired,
                             ClockResult::ReadFailure, ClockResult::WriteFailure, ClockResult::BindingChanged})
        {
            owner.status = failure;
            auto before = owner.resolves;
            Refuses([&] { (void)association.GetAnimHierarchy(); }, failure, "GetAnimHierarchy");
            Check(owner.resolves == before + 1);
            before = owner.masks;
            Refuses([&] { association.RestrictToBones(std::bitset<64>(~std::uint64_t{0})); }, failure, "RestrictToBones");
            Check(owner.masks == before + 1 && owner.mask == ~std::uint64_t{0});
            before = owner.originals;
            Refuses([&] { association.RestrictToBonesOf(nullptr); }, failure, "RestrictToBonesOf");
            Check(owner.originals == before + 1 && owner.source == 0);
        }
        owner.status = ClockResult::Ok;
        owner.hierarchy.ownerGeneration = expected.ownerGeneration - 1;
        Refuses([&] { (void)association.GetAnimHierarchy(); }, ClockResult::BindingChanged, "GetAnimHierarchy");
        owner.hierarchy = {expected.ownerGeneration, 0};
        Refuses([&] { (void)association.GetAnimHierarchy(); }, ClockResult::BindingChanged, "GetAnimHierarchy");
        Check(owner.creates == 0 && owner.retires == 0);
    }
    Check(owner.creates == 0 && owner.retires == 0);
    MissingHierarchy                    missing;
    PrivateAnimationAssociationReceiver withoutHierarchy(missing, expected);
    Refuses([&] { (void)withoutHierarchy.GetAnimHierarchy(); }, ClockResult::NotInitialized, "GetAnimHierarchy");
    Check(missing.resolves == 0);
    Defaults                            defaults;
    PrivateAnimationAssociationReceiver unavailable(defaults, expected);
    Refuses([&] { (void)unavailable.GetAnimHierarchy(); }, ClockResult::NotInitialized, "GetAnimHierarchy");
    Refuses([&] { unavailable.RestrictToBones({}); }, ClockResult::NotInitialized, "RestrictToBones");
    Refuses([&] { unavailable.RestrictToBonesOf(nullptr); }, ClockResult::NotInitialized, "RestrictToBonesOf");
    NativeAnimationHierarchyReferenceToken unchanged{123, 456};
    Check(defaults.ResolvePrivateAnimationAssociationHierarchy(expected, unchanged) == ClockResult::NotInitialized && unchanged.ownerGeneration == 123 &&
          unchanged.locator == 456);
    std::puts("Actual animation association bones SDK fixture completed");
    std::printf("Passed %u association bones SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
