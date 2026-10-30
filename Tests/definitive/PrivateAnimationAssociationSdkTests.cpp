/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual private constructor/read/scalar SDK dispatch, no fake lifetime
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlendAssociationDE.h"
#include <game/CAnimBlendAssocGroup.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mta::game_de;
// The SDK transports only this type's address. This test-only opaque object
// supplies a valid C++ reference and asserts no classic/native structure layout.
class CAnimBlendStaticAssociationSAInterface final
{
};
#include "PrivateAnimationAssociationFixture.generated.h"
using StaticConstructor = CAnimBlendAssociationSAInterface* (CAnimBlendAssociationDE::*)(CAnimBlendStaticAssociationSAInterface&);
static_assert(std::is_same_v<decltype(static_cast<StaticConstructor>(&PrivateAnimationAssociationReceiver::Constructor)), StaticConstructor>);
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::GetInterface), decltype(&CAnimBlendAssociationDE::GetInterface)>);
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::IsPartial), decltype(&CAnimBlendAssociationDE::IsPartial)>);
static_assert(std::is_same_v<decltype(&PrivateAnimationAssociationReceiver::SetCurrentSpeed), decltype(&CAnimBlendAssociationDE::SetCurrentSpeed)>);
static_assert(!std::is_abstract_v<CAnimBlendAssociationDE> && !std::is_abstract_v<PrivateAnimationAssociationReceiver>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Private association SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    bool Same(const NativeAnimationAssociationReferenceToken& a, const NativeAnimationAssociationReferenceToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.serial == b.serial;
    }
    float Float(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    std::int16_t Signed(std::uint16_t bits)
    {
        std::int16_t value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
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
    enum class Field
    {
        None,
        Blend,
        Speed,
        Id,
        Group,
        Flags
    };
    struct Owner final : NativeAnimationAssociationAccess
    {
        NativeAnimationAssociationValues                value{};
        NativeAnimationAssociationReferenceToken        received{};
        NativeAnimationAssociationReferenceToken*       mutateCaller = nullptr;
        const NativeAnimationAssociationReferenceToken* forbiddenCaller = nullptr;
        ClockResult                                     status = ClockResult::Ok;
        std::uintptr_t                                  source = 0, returned = 0;
        std::uint32_t                                   bits = 0;
        std::int16_t                                    word = 0;
        Field                                           field = Field::None;
        unsigned                                        creates = 0, constructs = 0, reads = 0, writes = 0, retires = 0;
        bool                                            aliased = false;
        void                                            Capture(const NativeAnimationAssociationReferenceToken& token) noexcept
        {
            received = token;
            aliased = aliased || &token == forbiddenCaller;
            if (mutateCaller)
                *mutateCaller = {};
        }
        ClockResult CreatePrivateAnimationAssociation(std::uint64_t, NativeAnimationAssociationReferenceToken&) noexcept override
        {
            ++creates;
            return ClockResult::NotInitialized;
        }
        ClockResult ConstructPrivateAnimationAssociationFromStatic(const NativeAnimationAssociationReferenceToken& token, std::uintptr_t locator,
                                                                   std::uintptr_t& output) noexcept override
        {
            ++constructs;
            Capture(token);
            source = locator;
            output = returned;
            return status;
        }
        ClockResult ReadPrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken& token,
                                                    NativeAnimationAssociationValues&               output) noexcept override
        {
            ++reads;
            Capture(token);
            output = value;
            return status;
        }
        ClockResult SetPrivateAnimationAssociationBlendAmount(const NativeAnimationAssociationReferenceToken& token, std::uint32_t supplied) noexcept override
        {
            ++writes;
            Capture(token);
            field = Field::Blend;
            bits = supplied;
            return status;
        }
        ClockResult SetPrivateAnimationAssociationCurrentSpeed(const NativeAnimationAssociationReferenceToken& token, std::uint32_t supplied) noexcept override
        {
            ++writes;
            Capture(token);
            field = Field::Speed;
            bits = supplied;
            return status;
        }
        ClockResult SetPrivateAnimationAssociationAnimID(const NativeAnimationAssociationReferenceToken& token, std::int16_t supplied) noexcept override
        {
            ++writes;
            Capture(token);
            field = Field::Id;
            word = supplied;
            return status;
        }
        ClockResult SetPrivateAnimationAssociationAnimGroup(const NativeAnimationAssociationReferenceToken& token, std::int16_t supplied) noexcept override
        {
            ++writes;
            Capture(token);
            field = Field::Group;
            word = supplied;
            return status;
        }
        ClockResult SetPrivateAnimationAssociationFlags(const NativeAnimationAssociationReferenceToken& token, std::int16_t supplied) noexcept override
        {
            ++writes;
            Capture(token);
            field = Field::Flags;
            word = supplied;
            return status;
        }
        ClockResult RetirePrivateAnimationAssociation(const NativeAnimationAssociationReferenceToken&) noexcept override
        {
            ++retires;
            return ClockResult::NotInitialized;
        }
    };
    struct Missing final : NativeAnimationAssociationAccess
    {
    };
}
int main()
{
    NativeAnimationAssociationReferenceToken caller{(std::numeric_limits<std::uint64_t>::max)() - 11, (std::numeric_limits<std::uint64_t>::max)() - 17};
    const auto                               original = caller;
    Owner                                    owner;
    owner.forbiddenCaller = &caller;
    owner.returned = static_cast<std::uintptr_t>(0x12345678abcde000ull);
    owner.value.interfaceAddress = owner.returned;
    PrivateAnimationAssociationReceiver receiver(owner, caller);
    CAnimBlendAssociation&              sdk = receiver;
    const CAnimBlendAssociation&        constant = receiver;
    caller = {};
    CAnimBlendStaticAssociationSAInterface source;
    Check(reinterpret_cast<std::uintptr_t>(sdk.Constructor(source)) == owner.returned);
    Check(owner.constructs == 1 && owner.reads == 0 && owner.source == reinterpret_cast<std::uintptr_t>(&source) && Same(owner.received, original));
    Check(reinterpret_cast<std::uintptr_t>(sdk.GetInterface()) == owner.value.interfaceAddress);
    for (const auto value :
         {std::int16_t{0}, std::int16_t{-1}, std::int16_t{12345}, (std::numeric_limits<std::int16_t>::min)(), (std::numeric_limits<std::int16_t>::max)()})
    {
        owner.value.animId = value;
        owner.value.animGroup = value;
        const auto reads = owner.reads;
        Check(static_cast<int>(sdk.GetAnimID()) == value && static_cast<int>(sdk.GetAnimGroup()) == value);
        Check(owner.reads == reads + 2 && Same(owner.received, original));
        auto writes = owner.writes;
        sdk.SetAnimID(value);
        Check(owner.writes == writes + 1 && owner.field == Field::Id && owner.word == value);
        writes = owner.writes;
        sdk.SetAnimGroup(value);
        Check(owner.writes == writes + 1 && owner.field == Field::Group && owner.word == value);
    }
    for (const auto flags : {std::uint16_t{0}, std::uint16_t{0x10}, std::uint16_t{0x4000}, std::uint16_t{0x4010}, std::uint16_t{0x8010}, std::uint16_t{0xffff}})
    {
        owner.value.flags = Signed(flags);
        const auto before = owner.reads;
        Check(constant.IsPartial() == ((flags & 0x10u) != 0) && owner.reads == before + 1);
        // Transport is raw. The real owner separately refuses unsupported
        // reference-bit changes; this fixture does not manufacture that ACK.
        const auto writes = owner.writes;
        sdk.SetFlags(Signed(flags));
        Check(owner.writes == writes + 1 && owner.field == Field::Flags && owner.word == Signed(flags));
    }
    for (const auto bits : {std::uint32_t{0}, std::uint32_t{0x80000000}, std::uint32_t{0x3f800001}, std::uint32_t{0xc0200000}, std::uint32_t{0x7fc01234},
                            std::uint32_t{0xffc07654}, std::uint32_t{0x7f800000}})
    {
        owner.value.blendAmountBits = bits;
        Check(Bits(sdk.GetBlendAmount()) == bits);
        auto writes = owner.writes;
        sdk.SetBlendAmount(Float(bits));
        Check(owner.writes == writes + 1 && owner.field == Field::Blend && owner.bits == bits);
        writes = owner.writes;
        sdk.SetCurrentSpeed(Float(bits));
        Check(owner.writes == writes + 1 && owner.field == Field::Speed && owner.bits == bits);
    }
    caller = original;
    owner.mutateCaller = &caller;
    sdk.SetCurrentSpeed(Float(0x80000000));
    Check(caller.serial == 0 && Same(owner.received, original) && owner.bits == 0x80000000 && !owner.aliased);
    owner.mutateCaller = nullptr;
    for (const auto failure : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::ReadFailure, ClockResult::WriteFailure,
                               ClockResult::BindingChanged, ClockResult::OwnerConflict, ClockResult::Busy, ClockResult::InvalidArgument})
    {
        owner.status = failure;
        const auto constructs = owner.constructs, reads = owner.reads, writes = owner.writes;
        Refuses([&] { sdk.Constructor(source); }, failure, "Constructor");
        Refuses([&] { sdk.GetInterface(); }, failure, "GetInterface");
        Refuses([&] { sdk.GetAnimID(); }, failure, "GetAnimID");
        Refuses([&] { sdk.GetAnimGroup(); }, failure, "GetAnimGroup");
        Refuses([&] { sdk.GetBlendAmount(); }, failure, "GetBlendAmount");
        Refuses([&] { constant.IsPartial(); }, failure, "IsPartial");
        Refuses([&] { sdk.SetBlendAmount(Float(0x7fc01234)); }, failure, "SetBlendAmount");
        Refuses([&] { sdk.SetCurrentSpeed(Float(0x80000000)); }, failure, "SetCurrentSpeed");
        Refuses([&] { sdk.SetAnimID(-1); }, failure, "SetAnimID");
        Refuses([&] { sdk.SetAnimGroup(-32768); }, failure, "SetAnimGroup");
        Refuses([&] { sdk.SetFlags(0); }, failure, "SetFlags");
        Check(owner.constructs == constructs + 1 && owner.reads == reads + 5 && owner.writes == writes + 5 && Same(owner.received, original));
    }
    Missing                             missing;
    PrivateAnimationAssociationReceiver unavailable(missing, original);
    Refuses([&] { unavailable.Constructor(source); }, ClockResult::NotInitialized, "Constructor");
    Refuses([&] { unavailable.GetInterface(); }, ClockResult::NotInitialized, "GetInterface");
    Refuses([&] { unavailable.GetAnimID(); }, ClockResult::NotInitialized, "GetAnimID");
    Refuses([&] { unavailable.GetAnimGroup(); }, ClockResult::NotInitialized, "GetAnimGroup");
    Refuses([&] { unavailable.GetBlendAmount(); }, ClockResult::NotInitialized, "GetBlendAmount");
    Refuses([&] { unavailable.IsPartial(); }, ClockResult::NotInitialized, "IsPartial");
    Refuses([&] { unavailable.SetBlendAmount(1); }, ClockResult::NotInitialized, "SetBlendAmount");
    Refuses([&] { unavailable.SetCurrentSpeed(1); }, ClockResult::NotInitialized, "SetCurrentSpeed");
    Refuses([&] { unavailable.SetAnimID(1); }, ClockResult::NotInitialized, "SetAnimID");
    Refuses([&] { unavailable.SetAnimGroup(1); }, ClockResult::NotInitialized, "SetAnimGroup");
    Refuses([&] { unavailable.SetFlags(1); }, ClockResult::NotInitialized, "SetFlags");
    auto untouched = original;
    Check(missing.CreatePrivateAnimationAssociation(original.ownerGeneration, untouched) == ClockResult::NotInitialized && Same(untouched, original));
    Check(missing.RetirePrivateAnimationAssociation(original) == ClockResult::NotInitialized && missing.HasPendingPrivateAnimationAssociations());
    Owner                                          other;
    const NativeAnimationAssociationReferenceToken otherToken{0x1020304050607080ull, 7};
    other.value.animId = 73;
    PrivateAnimationAssociationReceiver isolated(other, otherToken);
    Check(static_cast<int>(isolated.GetAnimID()) == 73 && Same(other.received, otherToken));
    const auto lastReads = owner.reads, lastWrites = owner.writes, lastConstructs = owner.constructs;
    {
        PrivateAnimationAssociationReceiver unused(owner, original);
    }
    Check(owner.reads == lastReads && owner.writes == lastWrites && owner.constructs == lastConstructs && owner.creates == 0 && owner.retires == 0);
    std::puts("Actual private animation association SDK fixture completed");
    std::printf("Passed %u private association SDK checks (%zu-bit; dispatch only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
