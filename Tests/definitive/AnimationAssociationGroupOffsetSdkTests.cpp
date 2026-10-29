/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact source ID-offset setter transport through the borrowed owner
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
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::SetIDOffset), decltype(&CAnimBlendAssocGroupDE::SetIDOffset)>);
static_assert(std::is_abstract_v<CAnimBlendAssocGroupDE> && !std::is_abstract_v<AnimationAssociationGroupReceiver>);
static_assert(sizeof(int) == sizeof(std::int32_t));
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Association group offset SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    bool Same(const NativeAnimationAssociationGroupReferenceToken& a, const NativeAnimationAssociationGroupReferenceToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.index == b.index && a.locator == b.locator;
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
            refused = text.find("SetIDOffset") != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationAssociationGroupAccess
    {
        NativeAnimationAssociationGroupReferenceToken  received{};
        NativeAnimationAssociationGroupReferenceToken* mutateToken = nullptr;
        int*                                           mutateValue = nullptr;
        ClockResult                                    status = ClockResult::Ok;
        std::int32_t                                   receivedOffset = 0, offset = 0;
        // These stand for unrelated source fields. A setter must make only
        // its one owner call; it cannot derive a new ID/count through reads.
        NativeAnimationAssociationGroupValues values{static_cast<std::uintptr_t>(0x1234567800001000ull), -91, 37};
        unsigned                              writes = 0, reads = 0, loadedReads = 0, resolutions = 0;
        ClockResult ResolveAnimationAssociationGroup(std::uint64_t, std::uint32_t, NativeAnimationAssociationGroupReferenceToken&) noexcept override
        {
            ++resolutions;
            return ClockResult::BindingChanged;
        }
        ClockResult ReadAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken&,
                                                  NativeAnimationAssociationGroupValues&) noexcept override
        {
            ++reads;
            return ClockResult::BindingChanged;
        }
        ClockResult ReadAnimationAssociationGroupLoaded(const NativeAnimationAssociationGroupReferenceToken&, bool&) noexcept override
        {
            ++loadedReads;
            return ClockResult::BindingChanged;
        }
        ClockResult SetAnimationAssociationGroupIdOffset(const NativeAnimationAssociationGroupReferenceToken& token, std::int32_t value) noexcept override
        {
            ++writes;
            received = token;
            receivedOffset = value;
            if (mutateToken)
                *mutateToken = {};
            if (mutateValue)
                *mutateValue = 0;
            if (status == ClockResult::Ok)
                offset = value;
            return status;
        }
    };
}
int main()
{
    const NativeAnimationAssociationGroupReferenceToken original{(std::numeric_limits<std::uint64_t>::max)() - 43, 0xffffffffu,
                                                                 static_cast<std::uintptr_t>(0x12345678abcde000ull)};
    auto                                                caller = original;
    Owner                                               owner;
    AnimationAssociationGroupReceiver                   receiver(owner, caller);
    CAnimBlendAssocGroup&                               sdk = receiver;
    caller = {};
    // Source SetIDOffset assigns the complete signed DWORD. Preserve raw
    // limits instead of adding an enum/range gate or pre-reading a group.
    for (const auto value : {std::int32_t{0}, std::int32_t{1}, std::int32_t{-1}, std::int32_t{0x12345678}, (std::numeric_limits<std::int32_t>::min)(),
                             (std::numeric_limits<std::int32_t>::max)()})
    {
        const auto before = owner.writes;
        sdk.SetIDOffset(value);
        Check(owner.writes == before + 1 && owner.receivedOffset == value && owner.offset == value && Same(owner.received, original));
        Check(owner.values.interfaceAddress == static_cast<std::uintptr_t>(0x1234567800001000ull) && owner.values.numAnimations == -91 &&
              owner.values.groupId == 37 && owner.reads == 0 && owner.loadedReads == 0 && owner.resolutions == 0);
    }
    caller = original;
    int callerValue = (std::numeric_limits<int>::min)();
    owner.mutateToken = &caller;
    owner.mutateValue = &callerValue;
    const auto beforeMutation = owner.writes;
    sdk.SetIDOffset(callerValue);
    Check(owner.writes == beforeMutation + 1 && owner.receivedOffset == (std::numeric_limits<std::int32_t>::min)() && Same(owner.received, original) &&
          caller.locator == 0 && callerValue == 0);
    owner.mutateToken = nullptr;
    owner.mutateValue = nullptr;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::WriteFailure,
                               ClockResult::BindingChanged, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
    {
        owner.status = failure;
        const auto before = owner.writes;
        const auto oldOffset = owner.offset;
        Refuses([&] { sdk.SetIDOffset(91); }, failure);
        Check(owner.writes == before + 1 && owner.receivedOffset == 91 && owner.offset == oldOffset && Same(owner.received, original));
    }
    NativeAnimationAssociationGroupAccess absent;
    AnimationAssociationGroupReceiver     unconfigured(absent, original);
    Refuses([&] { unconfigured.SetIDOffset(0); }, ClockResult::NotInitialized);
    Owner other;
    auto  otherToken = original;
    otherToken.ownerGeneration = 0x1020304050607080ull;
    otherToken.index = 3;
    otherToken.locator = static_cast<std::uintptr_t>(0x56781234ffffd000ull);
    AnimationAssociationGroupReceiver isolated(other, otherToken);
    CAnimBlendAssocGroup&             otherSdk = isolated;
    const auto                        firstOwnerWrites = owner.writes;
    otherSdk.SetIDOffset(-7);
    Check(other.writes == 1 && other.offset == -7 && Same(other.received, otherToken) && owner.writes == firstOwnerWrites);
    Check(owner.reads == 0 && owner.loadedReads == 0 && owner.resolutions == 0 && other.reads == 0 && other.loadedReads == 0 && other.resolutions == 0);
    std::puts("Actual animation association group offset SDK fixture completed");
    std::printf("Passed %u association group offset SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
