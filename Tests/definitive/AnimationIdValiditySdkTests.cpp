/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real animation-ID dispatch and exact classic source itinerary
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include "../../Client/game_de/CAnimBlendSequenceDE.h"
#include <game/CAnimValidityPolicy.h>
#include <game/CAnimGroupValidationPolicy.h>
#include <game/CAnimBlock.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "PrivateAnimationFactoryFixture.generated.h"
static_assert(std::is_same_v<decltype(&PrivateAnimationFactoryManagerReceiver::IsValidAnim), decltype(&CAnimManagerDE::IsValidAnim)>);

// Only dependency shapes are supplied here. The generated bodies are the real
// SA IsValidAnim, GetAnimStaticAssociation and IsValidGroup after formatting.
// The concrete CPU shell avoids deleting an SDK base with no virtual dtor and
// makes no SA/DE native interface layout, facade factory or engine claim.
struct ClassicStaticAssociationInterface
{
    short sAnimID = -1;
};
class ClassicStaticAssociationFacade
{
public:
    explicit ClassicStaticAssociationFacade(ClassicStaticAssociationInterface* value) : value(value) {}
    ClassicStaticAssociationInterface* GetInterface() const { return value; }

private:
    ClassicStaticAssociationInterface* value;
};
class ClassicAnimationIdValidityReceiver
{
public:
    using StaticAssocIntface_type = std::unique_ptr<ClassicStaticAssociationFacade>;
    struct Group
    {
        const bool* value;
        unsigned*   calls;
        bool        IsCreated() const
        {
            ++*calls;
            return *value;
        }
    };
    struct GroupBorrow
    {
        Group*   value;
        explicit operator bool() const { return value != nullptr; }
        Group*   operator->() const { return value; }
    };
    bool                    IsValidAnim(std::uint32_t, std::uint32_t) const;
    bool                    IsValidGroup(std::uint32_t) const;
    StaticAssocIntface_type GetAnimStaticAssociation(eAnimGroup, eAnimID) const;
    GroupBorrow             GetAnimBlendAssoc(AssocGroupId id) const
    {
        ++groupCalls;
        receivedGroup = static_cast<std::uint32_t>(id);
        return {groupPresent ? &group : nullptr};
    }
    ClassicStaticAssociationInterface* ReadStaticAssociationEntry(eAnimGroup groupId, eAnimID animationId, DWORD entry) const
    {
        ++entryCalls;
        receivedGroup = static_cast<std::uint32_t>(groupId);
        receivedAnimation = static_cast<std::uint32_t>(animationId);
        receivedEntry = entry;
        return associationPresent ? &selected : nullptr;
    }
    bool                                      groupPresent = true, created = false, associationPresent = true;
    mutable unsigned                          groupCalls = 0, createdCalls = 0, entryCalls = 0;
    mutable std::uint32_t                     receivedGroup = 0, receivedAnimation = 0;
    mutable DWORD                             receivedEntry = 0;
    mutable ClassicStaticAssociationInterface selected{};

private:
    mutable Group group{&created, &createdCalls};
};
#include "ClassicAnimationIdValidity.generated.inc"
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Animation ID validity SDK check %u failed\n", checks);
            std::exit(1);
        }
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
    struct Owner final : NativeAnimationManagerAccess
    {
        explicit Owner(std::uint64_t value = (std::numeric_limits<std::uint64_t>::max)() - 43) : generation(value) {}
        const std::uint64_t generation;
        ClockResult         groupStatus = ClockResult::Ok, idStatus = ClockResult::Ok;
        bool                created = false;
        std::int16_t        selected = -1;
        unsigned            groups = 0, ids = 0, forbidden = 0;
        std::uint64_t       receivedGeneration = 0;
        std::uint32_t       receivedGroup = 0, receivedId = 0;
        std::uint32_t*      mutateGroup = nullptr;
        std::uint32_t*      mutateId = nullptr;
        ClockResult         ReadAnimationGroupCreated(std::uint64_t gen, std::uint32_t group, bool& output) noexcept override
        {
            ++groups;
            receivedGeneration = gen;
            receivedGroup = group;
            if (mutateGroup)
                *mutateGroup = 0xffffffffu;
            if (mutateId)
                *mutateId = 0xffffffffu;
            output = created;
            return groupStatus;
        }
        ClockResult ReadAnimationStaticAssociationAnimID(std::uint64_t gen, std::uint32_t group, std::uint32_t id, std::int16_t& output) noexcept override
        {
            ++ids;
            receivedGeneration = gen;
            receivedGroup = group;
            receivedId = id;
            output = selected;
            return idStatus;
        }
        ClockResult ReadAnimationGroupName(std::uint64_t, std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
    };
    struct CreatedOnly final : NativeAnimationManagerAccess
    {
        ClockResult ReadAnimationGroupCreated(std::uint64_t, std::uint32_t, bool& value) noexcept override
        {
            value = true;
            return ClockResult::Ok;
        }
    };
}
int main()
{
    Owner                                  owner;
    PrivateAnimationFactoryManagerReceiver manager(owner, owner.generation);
    const CAnimManager&                    sdk = manager;
    ClassicAnimationIdValidityReceiver     classic;
    const auto                             animationLimit = static_cast<std::uint32_t>(eAnimID::ANIM_ID_MAX);
    const auto                             groupLimit = static_cast<std::uint32_t>(eAnimGroup::ANIM_TOTAL_GROUPS);
    for (const auto invalid : {animationLimit, std::uint32_t{0x7fffffffu}, std::uint32_t{0x80000000u}, std::uint32_t{0xffffffffu}})
    {
        owner.groupStatus = ClockResult::ReadFailure;
        const auto groups = owner.groups, ids = owner.ids, classicGroups = classic.groupCalls, entries = classic.entryCalls;
        Check(!sdk.IsValidAnim(0, invalid) && owner.groups == groups && owner.ids == ids);
        Check(!classic.IsValidAnim(0, invalid) && classic.groupCalls == classicGroups && classic.entryCalls == entries);
    }
    owner.groupStatus = ClockResult::Ok;
    for (const auto invalidGroup : {groupLimit, std::uint32_t{0xffffffffu}})
    {
        const auto groups = owner.groups, ids = owner.ids, entries = classic.entryCalls;
        Check(!sdk.IsValidAnim(invalidGroup, 0) && owner.groups == groups && owner.ids == ids);
        Check(!classic.IsValidAnim(invalidGroup, 0) && classic.entryCalls == entries);
    }
    owner.created = false;
    classic.created = false;
    const auto noId = owner.ids, noEntry = classic.entryCalls;
    Check(!sdk.IsValidAnim(0, 1) && owner.ids == noId);
    Check(!classic.IsValidAnim(0, 1) && classic.entryCalls == noEntry);
    classic.groupPresent = false;
    classic.created = true;
    Check(!classic.IsValidAnim(0, 1) && classic.entryCalls == noEntry);
    classic.groupPresent = true;
    owner.created = true;
    classic.created = true;
    for (const auto animation : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{73}, animationLimit - 1})
    {
        for (const auto selected :
             {static_cast<std::int16_t>(animation), std::int16_t{-1}, (std::numeric_limits<std::int16_t>::min)(), (std::numeric_limits<std::int16_t>::max)()})
        {
            owner.selected = selected;
            classic.selected.sAnimID = selected;
            const bool expected = static_cast<std::int32_t>(selected) == static_cast<std::int32_t>(animation);
            const auto groups = owner.groups, ids = owner.ids, classicGroups = classic.groupCalls, entries = classic.entryCalls;
            Check(sdk.IsValidAnim(groupLimit - 1, animation) == expected);
            Check(owner.groups == groups + 1 && owner.ids == ids + 1);
            Check(owner.receivedGroup == groupLimit - 1 && owner.receivedId == animation && owner.receivedGeneration == owner.generation);
            Check(classic.IsValidAnim(groupLimit - 1, animation) == expected);
            Check(classic.groupCalls == classicGroups + 1 && classic.entryCalls == entries + 1);
            Check(classic.receivedGroup == groupLimit - 1 && classic.receivedAnimation == animation && classic.receivedEntry == 0x4d3a60);
        }
    }
    classic.associationPresent = false;
    Check(!classic.IsValidAnim(0, 1));
    std::uint32_t callerGroup = groupLimit - 1, callerId = 73;
    owner.mutateGroup = &callerGroup;
    owner.mutateId = &callerId;
    owner.selected = 73;
    Check(sdk.IsValidAnim(callerGroup, callerId));
    Check(callerGroup == 0xffffffffu && callerId == 0xffffffffu && owner.receivedGroup == groupLimit - 1 && owner.receivedId == 73);
    owner.mutateGroup = nullptr;
    owner.mutateId = nullptr;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::BindingChanged,
                               ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
    {
        owner.idStatus = failure;
        Refuses([&] { sdk.IsValidAnim(0, 73); }, failure, "IsValidAnim");
        owner.idStatus = ClockResult::Ok;
        owner.groupStatus = failure;
        const auto ids = owner.ids;
        Refuses([&] { sdk.IsValidAnim(0, 73); }, failure, "IsValidGroup");
        Check(owner.ids == ids);
        owner.groupStatus = ClockResult::Ok;
    }
    CreatedOnly                            absentId;
    PrivateAnimationFactoryManagerReceiver missingId(absentId, owner.generation);
    Refuses([&] { missingId.IsValidAnim(0, 0); }, ClockResult::NotInitialized, "IsValidAnim");
    Check(!missingId.IsValidAnim(0, animationLimit));
    Owner other(0x123456789abcdef0ull);
    other.created = true;
    other.selected = 1;
    PrivateAnimationFactoryManagerReceiver isolated(other, other.generation);
    Check(isolated.IsValidAnim(0, 1) && other.ids == 1 && other.receivedGeneration == other.generation);
    Check(owner.forbidden == 0);
    std::puts("Actual classic animation ID validity callsite completed");
    std::puts("Actual animation ID validity SDK fixture completed");
    std::printf("Passed %u animation ID validity SDK checks (%zu-bit; source-callsite/transport only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
