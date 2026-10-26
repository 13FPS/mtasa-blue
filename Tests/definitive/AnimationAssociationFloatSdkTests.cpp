/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Three actual float getters and explicit refusal contract
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
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::GetCurrentProgress), decltype(&CAnimBlendAssociationDE::GetCurrentProgress)>);
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::GetCurrentSpeed), decltype(&CAnimBlendAssociationDE::GetCurrentSpeed)>);
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::GetLength), decltype(&CAnimBlendAssociationDE::GetLength)>);
static_assert(!noexcept(std::declval<const CAnimBlendAssociation&>().GetCurrentProgress()));
static_assert(!noexcept(std::declval<const CAnimBlendAssociation&>().GetCurrentSpeed()));
static_assert(!noexcept(std::declval<const CAnimBlendAssociation&>().GetLength()));
static_assert(!noexcept(std::declval<const CAnimBlendAssociationDE&>().GetCurrentProgress()));
static_assert(!noexcept(std::declval<const CAnimBlendAssociationDE&>().GetCurrentSpeed()));
static_assert(!noexcept(std::declval<const CAnimBlendAssociationDE&>().GetLength()));
static_assert(std::is_abstract_v<CAnimBlendAssociationDE> && !std::is_abstract_v<PrivateAnimationAssociationReceiver>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Association float SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
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
    struct Owner final : NativeAnimationAssociationAccess
    {
        NativeAnimationAssociationValues         values{};
        NativeAnimationAssociationReferenceToken received{}, *mutateCaller = nullptr;
        ClockResult                              status = ClockResult::Ok;
        std::uint32_t                            lengthBits = 0;
        unsigned                                 reads = 0, lengths = 0, creates = 0, retires = 0;
        void                                     Capture(const NativeAnimationAssociationReferenceToken& value)
        {
            if (mutateCaller)
            {
                *mutateCaller = {0, 0};
                mutateCaller = nullptr;
            }
            received = value;
        }
        ClockResult ReadPrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken& value,
                                                    NativeAnimationAssociationValues&               output) noexcept override
        {
            ++reads;
            Capture(value);
            if (status == ClockResult::Ok)
                output = values;
            return status;
        }
        ClockResult ReadPrivateAnimationAssociationLength(const NativeAnimationAssociationReferenceToken& value, std::uint32_t& output) noexcept override
        {
            ++lengths;
            Capture(value);
            if (status == ClockResult::Ok)
                output = lengthBits;
            return status;
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
        const CAnimBlendAssociation&        association = receiver;
        caller = {1, 2};
        owner.mutateCaller = &caller;
        owner.values.currentTimeBits = 0x42c80000u;
        owner.values.speedBits = 0xbf800000u;
        owner.lengthBits = 0x40800000u;
        // Source progress is absolute100, not100/4. Length and speed use
        // their current channels, without requiring a fabricated H provider.
        Check(Bits(association.GetCurrentProgress()) == 0x42c80000u && owner.reads == 1 && owner.lengths == 0);
        Check(Same(owner.received, expected) && caller.ownerGeneration == 0 && caller.serial == 0);
        Check(Bits(association.GetCurrentSpeed()) == 0xbf800000u && owner.reads == 2 && owner.lengths == 0);
        Check(Bits(association.GetLength()) == 0x40800000u && owner.reads == 2 && owner.lengths == 1);
        for (auto bits : {std::uint32_t{0}, std::uint32_t{0x80000000u}, std::uint32_t{1}, std::uint32_t{0x80000001u}, std::uint32_t{0x7f7fffffu},
                          std::uint32_t{0x7f800000u}, std::uint32_t{0xff800000u}, std::uint32_t{0x7fc12345u}, std::uint32_t{0xffc54321u}})
        {
            owner.values.currentTimeBits = bits;
            owner.values.speedBits = bits;
            owner.lengthBits = bits;
            const auto beforeRead = owner.reads, beforeLength = owner.lengths;
            Check(Bits(association.GetCurrentProgress()) == bits && owner.reads == beforeRead + 1 && owner.lengths == beforeLength);
            Check(Bits(association.GetCurrentSpeed()) == bits && owner.reads == beforeRead + 2 && owner.lengths == beforeLength);
            Check(Bits(association.GetLength()) == bits && owner.reads == beforeRead + 2 && owner.lengths == beforeLength + 1);
            Check(Same(owner.received, expected));
        }
        for (auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::Expired,
                             ClockResult::ReadFailure, ClockResult::WriteFailure, ClockResult::BindingChanged})
        {
            owner.status = failure;
            const auto beforeRead = owner.reads, beforeLength = owner.lengths;
            Refuses([&] { (void)association.GetCurrentProgress(); }, failure, "GetCurrentProgress");
            Check(owner.reads == beforeRead + 1 && owner.lengths == beforeLength);
            Refuses([&] { (void)association.GetCurrentSpeed(); }, failure, "GetCurrentSpeed");
            Check(owner.reads == beforeRead + 2 && owner.lengths == beforeLength);
            Refuses([&] { (void)association.GetLength(); }, failure, "GetLength");
            Check(owner.reads == beforeRead + 2 && owner.lengths == beforeLength + 1);
        }
        owner.status = ClockResult::Ok;
        owner.values.currentTimeBits = 0x3f800000u;
        owner.values.speedBits = 0x40000000u;
        owner.lengthBits = 0;
        Check(Bits(association.GetCurrentProgress()) == 0x3f800000u && Bits(association.GetCurrentSpeed()) == 0x40000000u &&
              Bits(association.GetLength()) == 0);
        Check(owner.creates == 0 && owner.retires == 0);
    }
    Check(owner.creates == 0 && owner.retires == 0);
    Defaults                            defaults;
    PrivateAnimationAssociationReceiver missing(defaults, expected);
    const CAnimBlendAssociation&        unavailable = missing;
    Refuses([&] { (void)unavailable.GetCurrentProgress(); }, ClockResult::NotInitialized, "GetCurrentProgress");
    Refuses([&] { (void)unavailable.GetCurrentSpeed(); }, ClockResult::NotInitialized, "GetCurrentSpeed");
    Refuses([&] { (void)unavailable.GetLength(); }, ClockResult::NotInitialized, "GetLength");
    std::uint32_t unchanged = 0xaabbccddu;
    Check(defaults.ReadPrivateAnimationAssociationLength(expected, unchanged) == ClockResult::NotInitialized && unchanged == 0xaabbccddu);
    std::puts("Actual animation association float SDK fixture completed");
    std::printf("Passed %u association float SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
