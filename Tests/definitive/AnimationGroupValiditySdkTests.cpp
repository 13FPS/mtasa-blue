/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual shared group gate plus real DE dispatch and classic callsite
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include "../../Client/game_de/CAnimBlendSequenceDE.h"
#include <game/CAnimGroupValidationPolicy.h>
#include <game/CAnimBlock.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
#include <stdexcept>
#include <string>
using namespace mta::game_de;
#include "PrivateAnimationFactoryFixture.generated.h"
// The actual SA method body is generated unchanged except its class qualifier.
// This nonowning dependency shell observes calls, not a second policy/model or
// a claimed SA group factory. Actual normal SA TU compilation is separate.
class ClassicAnimationGroupValidityReceiver
{
public:
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
    bool        IsValidGroup(std::uint32_t uiAnimGroup) const;
    GroupBorrow GetAnimBlendAssoc(AssocGroupId id) const
    {
        ++groupCalls;
        received = static_cast<std::uint32_t>(id);
        return {present ? &group : nullptr};
    }
    bool                  present = true;
    bool                  created = false;
    mutable unsigned      groupCalls = 0, createdCalls = 0;
    mutable std::uint32_t received = 0;

private:
    mutable Group group{&created, &createdCalls};
};
#include "ClassicAnimationGroupValidity.generated.inc"
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Animation group validity SDK check %u failed\n", checks);
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
            const std::string text(error.what());
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.find("IsValidGroup") != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationManagerAccess
    {
        const std::uint64_t generation = (std::numeric_limits<std::uint64_t>::max)() - 43;
        ClockResult         status = ClockResult::Ok;
        bool                created = false;
        unsigned            reads = 0, forbidden = 0;
        std::uint64_t       receivedGeneration = 0;
        std::uint32_t       receivedId = 0;
        std::uint32_t*      mutateCaller = nullptr;
        ClockResult         ReadAnimationGroupCreated(std::uint64_t gen, std::uint32_t id, bool& output) noexcept override
        {
            ++reads;
            receivedGeneration = gen;
            receivedId = id;
            if (mutateCaller)
                *mutateCaller = 0;
            output = created;
            return status;
        }
        ClockResult ReadAnimationAssocDefinitionCount(std::uint64_t, std::int32_t&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadAnimationGroupName(std::uint64_t, std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
    };
}
int main()
{
    Owner                                  owner;
    PrivateAnimationFactoryManagerReceiver manager(owner, owner.generation);
    const CAnimManager&                    sdk = manager;
    ClassicAnimationGroupValidityReceiver  classic;
    const auto                             upper = static_cast<std::uint32_t>(eAnimGroup::ANIM_TOTAL_GROUPS);
    for (const auto invalid : {upper, std::uint32_t{200}, std::uint32_t{0x7fffffffu}, std::uint32_t{0x80000000u}, std::uint32_t{0xffffffffu}})
    {
        const auto before = owner.reads, oldGroups = classic.groupCalls;
        owner.status = ClockResult::ReadFailure;
        Check(!sdk.IsValidGroup(invalid) && owner.reads == before);
        Check(!classic.IsValidGroup(invalid) && classic.groupCalls == oldGroups);
    }
    owner.status = ClockResult::Ok;
    for (const auto id : {std::uint32_t{0}, std::uint32_t{1}, upper - 1})
    {
        for (const bool value : {false, true})
        {
            owner.created = value;
            classic.created = value;
            classic.present = true;
            Check(sdk.IsValidGroup(id) == value);
            Check(owner.receivedId == id && owner.receivedGeneration == owner.generation);
            const auto groupCalls = classic.groupCalls, createdCalls = classic.createdCalls;
            Check(classic.IsValidGroup(id) == value);
            Check(classic.groupCalls == groupCalls + 1 && classic.createdCalls == createdCalls + 1 && classic.received == id);
        }
    }
    classic.present = false;
    const auto beforeMissing = classic.createdCalls;
    Check(!classic.IsValidGroup(0) && classic.createdCalls == beforeMissing);
    std::uint32_t caller = upper - 1;
    owner.mutateCaller = &caller;
    owner.created = true;
    Check(sdk.IsValidGroup(caller) && caller == 0 && owner.receivedId == upper - 1);
    owner.mutateCaller = nullptr;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::BindingChanged,
                               ClockResult::OwnerConflict, ClockResult::Busy})
    {
        owner.status = failure;
        Refuses([&] { sdk.IsValidGroup(0); }, failure);
    }
    NativeAnimationManagerAccess           absent;
    PrivateAnimationFactoryManagerReceiver unconfigured(absent, owner.generation);
    Refuses([&] { unconfigured.IsValidGroup(0); }, ClockResult::NotInitialized);
    Check(!unconfigured.IsValidGroup(upper) && owner.forbidden == 0);
    std::puts("Actual classic animation group validity callsite completed");
    std::puts("Actual animation group validity SDK fixture completed");
    std::printf("Passed %u animation group validity SDK checks (%zu-bit; source-callsite/transport only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
