/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact raw native-copy return transport through the existing owner
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendAssocGroupDE.h"
#include "../../Client/game_de/NativeAnimationAssociationAccess.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
#include "AnimationAssociationGroupFixture.generated.h"
static_assert(std::is_same_v<decltype(&AnimationAssociationGroupReceiver::CopyAnimation), decltype(&CAnimBlendAssocGroupDE::CopyAnimation)>);
static_assert(std::is_same_v<decltype(&CAnimBlendAssocGroup::CopyAnimation), CAnimBlendAssociationSAInterface* (CAnimBlendAssocGroup::*)(unsigned int)>);
static_assert(std::is_abstract_v<CAnimBlendAssocGroupDE> && !std::is_abstract_v<AnimationAssociationGroupReceiver>);
static_assert(sizeof(unsigned int) == sizeof(std::uint32_t));
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Group copy animation SDK check %u failed\n", checks);
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
            refused = text.find("CopyAnimation") != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationAssociationGroupAccess, NativeAnimationAssociationAccess
    {
        NativeAnimationAssociationGroupReferenceToken  received{};
        NativeAnimationAssociationGroupReferenceToken* mutateToken = nullptr;
        unsigned int*                                  mutateId = nullptr;
        ClockResult                                    status = ClockResult::Ok;
        std::uint32_t                                  receivedId = 0;
        std::uintptr_t                                 returnedTag = 0;
        unsigned                                       copies = 0, scalarReads = 0, loadedReads = 0, resolutions = 0, blockReads = 0, staticResolutions = 0;
        unsigned                                       creates = 0, constructs = 0, nodeFrees = 0, retires = 0;
        bool                                           outputInitiallyZero = true;
        ClockResult CopyAnimationAssociationGroup(const NativeAnimationAssociationGroupReferenceToken& token, std::uint32_t id,
                                                  std::uintptr_t& output) noexcept override
        {
            ++copies;
            received = token;
            receivedId = id;
            outputInitiallyZero = outputInitiallyZero && output == 0;
            // On failure this spy still supplies a nonzero result. The SDK
            // must propagate the status, never adopt that tag as a fallback.
            output = returnedTag;
            if (mutateToken)
                *mutateToken = {};
            if (mutateId)
                *mutateId = 0;
            return status;
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
        ClockResult ResolveAnimationAssociationGroup(std::uint64_t, std::uint32_t, NativeAnimationAssociationGroupReferenceToken&) noexcept override
        {
            ++resolutions;
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
        ClockResult CreatePrivateAnimationAssociation(std::uint64_t, NativeAnimationAssociationReferenceToken&) noexcept override
        {
            ++creates;
            return ClockResult::WriteFailure;
        }
        ClockResult ConstructPrivateAnimationAssociationFromStatic(const NativeAnimationAssociationReferenceToken&, std::uintptr_t,
                                                                   std::uintptr_t&) noexcept override
        {
            ++constructs;
            return ClockResult::WriteFailure;
        }
        ClockResult FreePrivateAnimationAssociationNodeArray(const NativeAnimationAssociationReferenceToken&) noexcept override
        {
            ++nodeFrees;
            return ClockResult::WriteFailure;
        }
        ClockResult RetirePrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken&) noexcept override
        {
            ++retires;
            return ClockResult::WriteFailure;
        }
        bool NoSideChannels() const noexcept
        {
            return !scalarReads && !loadedReads && !resolutions && !blockReads && !staticResolutions && !creates && !constructs && !nodeFrees && !retires;
        }
    };
}
int main()
{
    const NativeAnimationAssociationGroupReferenceToken    original{(std::numeric_limits<std::uint64_t>::max)() - 71, 0xffffffffu,
                                                                 static_cast<std::uintptr_t>(0x12345678abcde000ull)};
    Owner                                                  owner;
    alignas(std::uintptr_t) std::array<unsigned char, 104> nativeA{}, nativeB{};
    nativeA.fill(0x31);
    nativeB.fill(0x42);
    const auto nativeABefore = nativeA, nativeBBefore = nativeB;
    auto       callerToken = original;
    {
        AnimationAssociationGroupReceiver receiver(owner, callerToken);
        CAnimBlendAssocGroup&             sdk = receiver;
        callerToken = {};
        const std::array<std::uint32_t, 6>  ids{0, 1, 0xffffffffu, 0x80000000u, 0x7fffffffu, 123456u};
        const std::array<std::uintptr_t, 6> tags{0, reinterpret_cast<std::uintptr_t>(nativeA.data()), reinterpret_cast<std::uintptr_t>(nativeB.data()),
                                                 1, (std::numeric_limits<std::uintptr_t>::max)(),     std::uintptr_t{1} << (sizeof(std::uintptr_t) * 8 - 1)};
        // These are opaque LEGACY NATIVE interface tags, never C++ SDK
        // objects to dereference/delete. Actual native admission is the one
        // owner command's responsibility; the SDK adds no enum/pointer clamp.
        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            owner.returnedTag = tags[i];
            const auto before = owner.copies;
            const auto returned = reinterpret_cast<std::uintptr_t>(sdk.CopyAnimation(ids[i]));
            Check(returned == tags[i] && owner.copies == before + 1 && owner.receivedId == ids[i] && Same(owner.received, original));
            Check(owner.NoSideChannels() && owner.outputInitiallyZero);
        }
        callerToken = original;
        unsigned int callerId = 0xffffffffu;
        owner.mutateToken = &callerToken;
        owner.mutateId = &callerId;
        owner.returnedTag = reinterpret_cast<std::uintptr_t>(nativeA.data());
        const auto beforeMutation = owner.copies;
        auto*      returned = sdk.CopyAnimation(callerId);
        Check(reinterpret_cast<std::uintptr_t>(returned) == owner.returnedTag && owner.copies == beforeMutation + 1 && Same(owner.received, original) &&
              owner.receivedId == 0xffffffffu && !callerToken.locator && !callerId);
        owner.mutateToken = nullptr;
        owner.mutateId = nullptr;
        const auto beforeRepeatedId = owner.copies;
        owner.returnedTag = reinterpret_cast<std::uintptr_t>(nativeA.data());
        auto* first = sdk.CopyAnimation(5);
        owner.returnedTag = reinterpret_cast<std::uintptr_t>(nativeB.data());
        auto* second = sdk.CopyAnimation(5);
        Check(first != second && reinterpret_cast<std::uintptr_t>(first) == reinterpret_cast<std::uintptr_t>(nativeA.data()) &&
              reinterpret_cast<std::uintptr_t>(second) == reinterpret_cast<std::uintptr_t>(nativeB.data()) && owner.copies == beforeRepeatedId + 2);
        for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::WriteFailure,
                                   ClockResult::BindingChanged, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
        {
            owner.status = failure;
            const auto before = owner.copies;
            Refuses([&] { (void)sdk.CopyAnimation(0xffffffffu); }, failure);
            Check(owner.copies == before + 1 && owner.receivedId == 0xffffffffu && Same(owner.received, original) && owner.NoSideChannels());
        }
        NativeAnimationAssociationGroupAccess absent;
        AnimationAssociationGroupReceiver     unconfigured(absent, original);
        Refuses([&] { (void)unconfigured.CopyAnimation(0); }, ClockResult::NotInitialized);
        Owner other;
        auto  otherToken = original;
        otherToken.ownerGeneration = 0x1020304050607080ull;
        otherToken.index = 3;
        otherToken.locator = static_cast<std::uintptr_t>(0x56781234ffffd000ull);
        AnimationAssociationGroupReceiver isolated(other, otherToken);
        const auto                        firstOwnerCalls = owner.copies;
        Check(!isolated.CopyAnimation(0xffffffffu) && other.copies == 1 && other.receivedId == 0xffffffffu && Same(other.received, otherToken) &&
              owner.copies == firstOwnerCalls && other.NoSideChannels());
    }
    // The actual Group receiver destructor owns only its CPU child facades.
    // It must not retire/free copies returned as native runtime-owned tags.
    Check(owner.NoSideChannels() && nativeA == nativeABefore && nativeB == nativeBBefore);
    std::puts("Actual group copy animation SDK fixture completed");
    std::printf("Passed %u group copy animation SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
