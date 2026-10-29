/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real four-setter transport without inventing node/free authority
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
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::SetNumBlendNodes), decltype(&CAnimBlendStaticAssociationDE::SetNumBlendNodes)>);
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::SetAnimID), decltype(&CAnimBlendStaticAssociationDE::SetAnimID)>);
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::SetAnimGroup), decltype(&CAnimBlendStaticAssociationDE::SetAnimGroup)>);
static_assert(std::is_same_v<decltype(&AnimationStaticAssociationReceiver::SetFlags), decltype(&CAnimBlendStaticAssociationDE::SetFlags)>);
static_assert(!std::is_abstract_v<CAnimBlendStaticAssociationDE> && !std::is_abstract_v<AnimationStaticAssociationReceiver>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Static association write SDK check %u failed\n", checks);
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
    enum class Method
    {
        None,
        Nodes,
        Id,
        Group,
        Flags
    };
    struct Owner final : NativeAnimationStaticAssociationAccess
    {
        NativeAnimationStaticAssociationReferenceToken  received{};
        NativeAnimationStaticAssociationReferenceToken* mutateToken = nullptr;
        unsigned short*                                 mutateCount = nullptr;
        short*                                          mutateSigned = nullptr;
        ClockResult                                     status = ClockResult::Ok;
        Method                                          method = Method::None;
        std::uint16_t                                   count = 0;
        std::int16_t                                    signedValue = 0;
        unsigned                                        writes = 0, reads = 0, resolutions = 0;
        void                                            Capture(const NativeAnimationStaticAssociationReferenceToken& token, Method which) noexcept
        {
            ++writes;
            received = token;
            method = which;
            if (mutateToken)
                *mutateToken = {};
            if (mutateCount)
                *mutateCount = 0;
            if (mutateSigned)
                *mutateSigned = 0;
        }
        ClockResult ResolveAnimationStaticAssociation(std::uint64_t, std::uint32_t, std::size_t,
                                                      NativeAnimationStaticAssociationReferenceToken&) noexcept override
        {
            ++resolutions;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadAnimationStaticAssociation(const NativeAnimationStaticAssociationReferenceToken&,
                                                   NativeAnimationStaticAssociationValues&) noexcept override
        {
            ++reads;
            return ClockResult::NotInitialized;
        }
        ClockResult SetAnimationStaticAssociationNumBlendNodes(const NativeAnimationStaticAssociationReferenceToken& token,
                                                               std::uint16_t                                         value) noexcept override
        {
            Capture(token, Method::Nodes);
            count = value;
            return status;
        }
        ClockResult SetAnimationStaticAssociationAnimID(const NativeAnimationStaticAssociationReferenceToken& token, std::int16_t value) noexcept override
        {
            Capture(token, Method::Id);
            signedValue = value;
            return status;
        }
        ClockResult SetAnimationStaticAssociationAnimGroup(const NativeAnimationStaticAssociationReferenceToken& token, std::int16_t value) noexcept override
        {
            Capture(token, Method::Group);
            signedValue = value;
            return status;
        }
        ClockResult SetAnimationStaticAssociationFlags(const NativeAnimationStaticAssociationReferenceToken& token, std::int16_t value) noexcept override
        {
            Capture(token, Method::Flags);
            signedValue = value;
            return status;
        }
    };
}
int main()
{
    const NativeAnimationStaticAssociationReferenceToken original{(std::numeric_limits<std::uint64_t>::max)() - 71, 0xffffffffu,
                                                                  (std::numeric_limits<std::size_t>::max)(),
                                                                  static_cast<std::uintptr_t>(0x23456789abcde000ull)};
    auto                                                 callerToken = original;
    Owner                                                owner;
    AnimationStaticAssociationReceiver                   receiver(owner, callerToken);
    CAnimBlendStaticAssociation&                         sdk = receiver;
    callerToken = {};
    // Transport every source WORD intact. The native owner, separately tested,
    // refuses unsupported count growth; this spy asserts no SDK clamp, read,
    // allocation or invented payload-capacity admission.
    for (const auto count : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0x7fff}, std::uint16_t{0x8000}, std::uint16_t{0xffff}})
    {
        const auto before = owner.writes;
        sdk.SetNumBlendNodes(count);
        Check(owner.writes == before + 1 && owner.method == Method::Nodes && owner.count == count && Same(owner.received, original));
    }
    for (const auto value : {std::int16_t{0}, std::int16_t{1}, std::int16_t{-1}, std::int16_t{12345}, (std::numeric_limits<std::int16_t>::min)(),
                             (std::numeric_limits<std::int16_t>::max)()})
    {
        auto before = owner.writes;
        sdk.SetAnimID(value);
        Check(owner.writes == before + 1 && owner.method == Method::Id && owner.signedValue == value && Same(owner.received, original));
        before = owner.writes;
        sdk.SetAnimGroup(value);
        Check(owner.writes == before + 1 && owner.method == Method::Group && owner.signedValue == value && Same(owner.received, original));
        before = owner.writes;
        sdk.SetFlags(value);
        Check(owner.writes == before + 1 && owner.method == Method::Flags && owner.signedValue == value && Same(owner.received, original));
    }
    unsigned short countCaller = 0xffffu;
    callerToken = original;
    owner.mutateCount = &countCaller;
    owner.mutateToken = &callerToken;
    sdk.SetNumBlendNodes(countCaller);
    Check(owner.count == 0xffffu && Same(owner.received, original) && countCaller == 0 && callerToken.locator == 0);
    owner.mutateCount = nullptr;
    short signedCaller = (std::numeric_limits<short>::min)();
    owner.mutateSigned = &signedCaller;
    sdk.SetFlags(signedCaller);
    Check(owner.signedValue == (std::numeric_limits<std::int16_t>::min)() && signedCaller == 0 && Same(owner.received, original));
    owner.mutateSigned = nullptr;
    owner.mutateToken = nullptr;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::WriteFailure,
                               ClockResult::BindingChanged, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
    {
        owner.status = failure;
        const auto before = owner.writes;
        Refuses([&] { sdk.SetNumBlendNodes(31); }, failure, "SetNumBlendNodes");
        Refuses([&] { sdk.SetAnimID(-1); }, failure, "SetAnimID");
        Refuses([&] { sdk.SetAnimGroup(-32768); }, failure, "SetAnimGroup");
        Refuses([&] { sdk.SetFlags(32767); }, failure, "SetFlags");
        Check(owner.writes == before + 4 && Same(owner.received, original));
    }
    NativeAnimationStaticAssociationAccess absent;
    AnimationStaticAssociationReceiver     unconfigured(absent, original);
    Refuses([&] { unconfigured.SetNumBlendNodes(0); }, ClockResult::NotInitialized, "SetNumBlendNodes");
    Refuses([&] { unconfigured.SetAnimID(0); }, ClockResult::NotInitialized, "SetAnimID");
    Refuses([&] { unconfigured.SetAnimGroup(0); }, ClockResult::NotInitialized, "SetAnimGroup");
    Refuses([&] { unconfigured.SetFlags(0); }, ClockResult::NotInitialized, "SetFlags");
    Owner other;
    auto  otherToken = original;
    otherToken.ownerGeneration = 0x1020304050607080ull;
    otherToken.index = 3;
    otherToken.locator = static_cast<std::uintptr_t>(0x56781234ffffd000ull);
    AnimationStaticAssociationReceiver isolated(other, otherToken);
    isolated.SetAnimID(7);
    Check(other.method == Method::Id && other.signedValue == 7 && Same(other.received, otherToken));
    Check(owner.reads == 0 && other.reads == 0 && owner.resolutions == 0 && other.resolutions == 0);
    std::puts("Actual animation static association write SDK fixture completed");
    std::printf("Passed %u static association write SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
