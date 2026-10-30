/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One source-progress transport, without a mirrored native helper
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendAssociationDE.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include "PrivateAnimationAssociationFixture.generated.h"
using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::SetCurrentProgress), decltype(&CAnimBlendAssociationDE::SetCurrentProgress)>);
static_assert(!noexcept(std::declval<CAnimBlendAssociation&>().SetCurrentProgress(0.0f)));
static_assert(!std::is_abstract_v<CAnimBlendAssociationDE> && !std::is_abstract_v<PrivateAnimationAssociationReceiver>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Association progress SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    float Float(std::uint32_t bits)
    {
        float value;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    bool Same(const NativeAnimationAssociationReferenceToken& a, const NativeAnimationAssociationReferenceToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.serial == b.serial;
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
            const std::string text = error.what();
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.find("SetCurrentProgress") != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationAssociationAccess
    {
        NativeAnimationAssociationReferenceToken received{}, *mutateToken = nullptr;
        float*                                   mutateFloat = nullptr;
        ClockResult                              status = ClockResult::Ok;
        std::uint32_t                            receivedBits = 0;
        unsigned                                 writes = 0, reads = 0, lengths = 0, creates = 0, retires = 0;
        ClockResult SetPrivateAnimationAssociationCurrentProgress(const NativeAnimationAssociationReferenceToken& token, std::uint32_t bits) noexcept override
        {
            ++writes;
            if (mutateToken)
            {
                *mutateToken = {0, 0};
                mutateToken = nullptr;
            }
            if (mutateFloat)
            {
                *mutateFloat = 0.5f;
                mutateFloat = nullptr;
            }
            received = token;
            receivedBits = bits;
            return status;
        }
        ClockResult ReadPrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken&, NativeAnimationAssociationValues&) noexcept override
        {
            ++reads;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadPrivateAnimationAssociationLength(const NativeAnimationAssociationReferenceToken&, std::uint32_t&) noexcept override
        {
            ++lengths;
            return ClockResult::NotInitialized;
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
    struct Defaults final : NativeAnimationAssociationAccess
    {
    };
}
int main()
{
    const NativeAnimationAssociationReferenceToken expected{0xfedcba9876543210ull, 0x8877665544332211ull};
    Owner                                          owner;
    {
        auto                                caller = expected;
        PrivateAnimationAssociationReceiver receiver(owner, caller);
        CAnimBlendAssociation&              association = receiver;
        for (const std::uint32_t bits : {std::uint32_t{0}, std::uint32_t{0x80000000u}, std::uint32_t{1}, std::uint32_t{0x3e800000u}, std::uint32_t{0x3f800000u},
                                         std::uint32_t{0x42c80000u}, std::uint32_t{0xc0a00000u}, std::uint32_t{0x7f800000u}, std::uint32_t{0xff800000u},
                                         std::uint32_t{0x7fc12345u}, std::uint32_t{0xffc54321u}})
        {
            float progress = Float(bits);
            owner.mutateToken = &caller;
            owner.mutateFloat = &progress;
            const auto before = owner.writes;
            association.SetCurrentProgress(progress);
            Check(owner.writes == before + 1 && Same(owner.received, expected) && owner.receivedBits == bits);
            Check(caller.ownerGeneration == 0 && caller.serial == 0 && Bits(progress) == 0x3f000000u);
            Check(owner.reads == 0 && owner.lengths == 0 && owner.creates == 0 && owner.retires == 0);
        }
        // SDK transports unsupported native domains too. Only the real owner
        // may refuse them; this spy certifies neither helper effects nor math.
        for (auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::Expired,
                             ClockResult::ReadFailure, ClockResult::WriteFailure, ClockResult::BindingChanged, ClockResult::OriginalException})
        {
            owner.status = failure;
            const auto before = owner.writes;
            Refuses([&] { association.SetCurrentProgress(Float(0x7fc12345u)); }, failure);
            Check(owner.writes == before + 1 && owner.receivedBits == 0x7fc12345u && Same(owner.received, expected));
        }
        owner.status = ClockResult::Ok;
        const NativeAnimationAssociationReferenceToken other{expected.ownerGeneration - 1, expected.serial + 1};
        PrivateAnimationAssociationReceiver            second(owner, other);
        second.SetCurrentProgress(1.0f);
        Check(Same(owner.received, other) && owner.receivedBits == 0x3f800000u);
        association.SetCurrentProgress(0.0f);
        Check(Same(owner.received, expected) && owner.receivedBits == 0);
        Check(owner.reads == 0 && owner.lengths == 0 && owner.creates == 0 && owner.retires == 0);
    }
    Check(owner.creates == 0 && owner.retires == 0);
    Defaults                            defaults;
    PrivateAnimationAssociationReceiver missing(defaults, expected);
    Refuses([&] { missing.SetCurrentProgress(0.0f); }, ClockResult::NotInitialized);
    Check(defaults.SetPrivateAnimationAssociationCurrentProgress(expected, 0x80000000u) == ClockResult::NotInitialized);
    std::puts("Actual animation association progress SDK fixture completed");
    std::printf("Passed %u association progress SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
