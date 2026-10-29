/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact empty-group initializer transport without SDK row iteration
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendAssocGroupDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "AnimationAssociationGroupFixture.generated.h"
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::InitEmptyAssociations), decltype(&CAnimBlendAssocGroupDE::InitEmptyAssociations)>);
static_assert(std::is_abstract_v<CAnimBlendAssocGroupDE> && !std::is_abstract_v<AnimationAssociationGroupReceiver>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Association group empty initialization SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    bool Same(const NativeAnimationAssociationGroupReferenceToken& a, const NativeAnimationAssociationGroupReferenceToken& b) noexcept
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
            refused = text.find("InitEmptyAssociations") != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationAssociationGroupAccess
    {
        NativeAnimationAssociationGroupReferenceToken  received{};
        NativeAnimationAssociationGroupReferenceToken* mutateToken = nullptr;
        RpClump**                                      mutateClump = nullptr;
        ClockResult                                    status = ClockResult::Ok;
        std::uintptr_t                                 receivedClump = 0;
        unsigned    initializations = 0, scalarReads = 0, loadedReads = 0, groupResolutions = 0, blockReads = 0, staticResolutions = 0;
        ClockResult InitializeEmptyAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken& token, std::uintptr_t clump) noexcept override
        {
            ++initializations;
            received = token;
            receivedClump = clump;
            if (mutateToken)
                *mutateToken = {};
            if (mutateClump)
                *mutateClump = nullptr;
            return status;
        }
        ClockResult ResolveAnimationAssociationGroup(std::uint64_t, std::uint32_t, NativeAnimationAssociationGroupReferenceToken&) noexcept override
        {
            ++groupResolutions;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken&,
                                                  NativeAnimationAssociationGroupValues&) noexcept override
        {
            ++scalarReads;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadAnimationAssociationGroupLoaded(const NativeAnimationAssociationGroupReferenceToken&, bool&) noexcept override
        {
            ++loadedReads;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadAnimationAssociationGroupBlock(const NativeAnimationAssociationGroupReferenceToken&,
                                                       NativeAnimationAssociationGroupBlock&) noexcept override
        {
            ++blockReads;
            return ClockResult::ReadFailure;
        }
        ClockResult ResolveAnimationAssociationGroupStaticById(const NativeAnimationAssociationGroupReferenceToken&, std::uint32_t,
                                                               NativeAnimationStaticAssociationReferenceToken&) noexcept override
        {
            ++staticResolutions;
            return ClockResult::ReadFailure;
        }
        bool NoSideChannels() const noexcept { return !scalarReads && !loadedReads && !groupResolutions && !blockReads && !staticResolutions; }
    };
}
int main()
{
    const NativeAnimationAssociationGroupReferenceToken original{(std::numeric_limits<std::uint64_t>::max)() - 31, 0xffffffffu,
                                                                 static_cast<std::uintptr_t>(0x12345678abcde000ull)};
    auto                                                callerToken = original;
    Owner                                               owner;
    AnimationAssociationGroupReceiver                   receiver(owner, callerToken);
    CAnimBlendAssocGroup&                               sdk = receiver;
    callerToken = {};
    // Source zero-count and all-valid itineraries never consume the clump.
    // The SDK therefore forwards even null without inventing a clump gate;
    // native entry, row ordering and journals belong to the owner command.
    const std::array<std::uintptr_t, 6> clumps{
        0, 1, 0x1234u, 0x7fffffffu, std::uintptr_t{1} << (sizeof(std::uintptr_t) * 8 - 1), (std::numeric_limits<std::uintptr_t>::max)()};
    for (const auto clump : clumps)
    {
        const auto before = owner.initializations;
        sdk.InitEmptyAssociations(reinterpret_cast<RpClump*>(clump));
        Check(owner.initializations == before + 1 && owner.receivedClump == clump && Same(owner.received, original));
        Check(owner.NoSideChannels());
    }
    callerToken = original;
    auto* callerClump = reinterpret_cast<RpClump*>((std::numeric_limits<std::uintptr_t>::max)());
    owner.mutateToken = &callerToken;
    owner.mutateClump = &callerClump;
    const auto beforeMutation = owner.initializations;
    sdk.InitEmptyAssociations(callerClump);
    Check(owner.initializations == beforeMutation + 1 && Same(owner.received, original) &&
          owner.receivedClump == (std::numeric_limits<std::uintptr_t>::max)() && !callerToken.locator && !callerClump);
    owner.mutateToken = nullptr;
    owner.mutateClump = nullptr;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::WriteFailure,
                               ClockResult::BindingChanged, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
    {
        owner.status = failure;
        const auto before = owner.initializations;
        Refuses([&] { sdk.InitEmptyAssociations(reinterpret_cast<RpClump*>(std::uintptr_t{0x1234})); }, failure);
        Check(owner.initializations == before + 1 && owner.receivedClump == 0x1234u && Same(owner.received, original));
    }
    NativeAnimationAssociationGroupAccess absent;
    AnimationAssociationGroupReceiver     unconfigured(absent, original);
    Refuses([&] { unconfigured.InitEmptyAssociations(nullptr); }, ClockResult::NotInitialized);
    Owner other;
    auto  otherToken = original;
    otherToken.ownerGeneration = 0x1020304050607080ull;
    otherToken.index = 3;
    otherToken.locator = static_cast<std::uintptr_t>(0x56781234ffffd000ull);
    AnimationAssociationGroupReceiver isolated(other, otherToken);
    CAnimBlendAssocGroup&             otherSdk = isolated;
    const auto                        firstOwnerCalls = owner.initializations;
    otherSdk.InitEmptyAssociations(nullptr);
    Check(other.initializations == 1 && !other.receivedClump && Same(other.received, otherToken) && owner.initializations == firstOwnerCalls);
    Check(owner.NoSideChannels() && other.NoSideChannels());
    std::puts("Actual animation association group empty initialization SDK fixture completed");
    std::printf("Passed %u association group empty initialization SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
