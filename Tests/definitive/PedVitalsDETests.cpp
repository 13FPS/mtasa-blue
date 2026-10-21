/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Four actual CPed methods with explicit borrowed-owner refusals
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedVitalsDE.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated CPed SDK method reached the vitals fixture\n", stderr);
        std::abort();
    }
}
// Remaining real CPed/CPhysical/CEntity methods reject only in this generated
// test subclass. The shipping layer remains abstract and has no ped factory.
#include "PedVitalsFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }
    template <class Operation>
    void Refuses(Operation operation, const char* description)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, description);
            return;
        }
        catch (...)
        {
        }
        Check(false, description);
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    float Float(std::uint32_t bits)
    {
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    bool SameToken(const NativePedReferenceToken& first, const NativePedReferenceToken& second)
    {
        return first.version == second.version && first.ownerGeneration == second.ownerGeneration && first.slot == second.slot;
    }
    class Owner final : public NativePedVitalsAccess
    {
    public:
        NativePedReferenceToken token;
        NativePedVitalField     field = NativePedVitalField::Health;
        std::uint32_t           bits = 0, supplied = 0;
        unsigned                reads = 0, writes = 0;
        ClockResult             result = ClockResult::Ok;
        ClockResult             ReadPedVital(const NativePedReferenceToken& requested, NativePedVitalField kind, float& output) noexcept override
        {
            ++reads;
            token = requested;
            field = kind;
            // Even a refusing owner may have touched its local output. The
            // SDK must throw rather than report that unacknowledged value.
            std::memcpy(&output, &supplied, sizeof(output));
            return result;
        }
        ClockResult SetPedVital(const NativePedReferenceToken& requested, NativePedVitalField kind, float value) noexcept override
        {
            ++writes;
            token = requested;
            field = kind;
            std::memcpy(&bits, &value, sizeof(bits));
            return result;
        }
    };
    class RefusingOwner final : public NativePedVitalsAccess
    {
    };
}

int main()
{
    using namespace mta::game_de;
    static_assert(std::is_abstract_v<mta::game_de::CPedVitalsDE>);
    static_assert(std::is_base_of_v<CPed, mta::game_de::CPedVitalsDE>);
    static_assert(!std::is_abstract_v<fixture::PedVitalsFixture>);
    Check(fixture::RejectingMethods > 100, "the generated concrete test type rejects the remaining actual inherited SDK surface");
    NativePedReferenceToken token;
    token.version = 0x123456789;
    token.ownerGeneration = 0xfedcba987;
    token.slot = 23;
    const auto original = token;
    Owner      owner;
    {
        fixture::PedVitalsFixture instance(owner, token);
        CPed&                     ped = instance;
        const CPed&               constant = instance;
        // The layer copies only CPU token identity. Mutating the caller's
        // original metadata must not redirect the already-bound SDK instance.
        token.version = 7;
        token.ownerGeneration = 8;
        token.slot = 9;
        const std::array<std::uint32_t, 7> values{0, 0x80000000, 0x3f800001, 0xc0200000, 0x7fc01234, 0xffc07654, 0x7f800000};
        for (const auto bits : values)
        {
            owner.supplied = bits;
            auto calls = owner.reads;
            Check(Bits(constant.GetHealth()) == bits && owner.reads == calls + 1 && owner.field == NativePedVitalField::Health &&
                      SameToken(owner.token, original),
                  "actual const CPed health dispatch forwards exact token and raw owner bits once");
            calls = owner.reads;
            Check(
                Bits(constant.GetArmor()) == bits && owner.reads == calls + 1 && owner.field == NativePedVitalField::Armor && SameToken(owner.token, original),
                "actual const CPed armour dispatch selects the separate typed native field");
            calls = owner.writes;
            ped.SetHealth(Float(bits));
            Check(owner.writes == calls + 1 && owner.bits == bits && owner.field == NativePedVitalField::Health && SameToken(owner.token, original),
                  "actual CPed health setter forwards raw float bits without clamping or native snapshot repair");
            calls = owner.writes;
            ped.SetArmor(Float(bits));
            Check(owner.writes == calls + 1 && owner.bits == bits && owner.field == NativePedVitalField::Armor && SameToken(owner.token, original),
                  "actual CPed armour setter forwards exact field and token through the same borrowed owner");
        }
        for (std::uint32_t value = static_cast<std::uint32_t>(ClockResult::InvalidArgument);
             value <= static_cast<std::uint32_t>(ClockResult::OriginalException); ++value)
        {
            owner.result = static_cast<ClockResult>(value);
            auto reads = owner.reads;
            Refuses([&] { constant.GetHealth(); }, "every explicit health owner refusal throws instead of manufacturing a float");
            Check(owner.reads == reads + 1, "failed health read is attempted once without retry");
            reads = owner.reads;
            Refuses([&] { constant.GetArmor(); }, "every explicit armour owner refusal throws instead of manufacturing a float");
            Check(owner.reads == reads + 1, "failed armour read is attempted once without retry");
            auto writes = owner.writes;
            Refuses([&] { ped.SetHealth(Float(0x80000000)); }, "every explicit health setter failure propagates from its real owner");
            Check(owner.writes == writes + 1 && owner.bits == 0x80000000, "failed health intent preserves argument and is not replayed");
            writes = owner.writes;
            Refuses([&] { ped.SetArmor(Float(0x7fc01234)); }, "every explicit armour setter failure propagates from its real owner");
            Check(owner.writes == writes + 1 && owner.bits == 0x7fc01234, "failed armour intent preserves argument and is not replayed");
        }
        owner.result = ClockResult::Ok;
    }
    const auto reads = owner.reads, writes = owner.writes;
    {
        fixture::PedVitalsFixture unused(owner, original);
    }
    Check(owner.reads == reads && owner.writes == writes, "SDK construction and destruction perform no native registration, cleanup or vitals operation");
    RefusingOwner             refusing;
    fixture::PedVitalsFixture unavailable(refusing, original);
    Refuses([&] { unavailable.GetHealth(); }, "default optional owner explicitly refuses health access");
    Refuses([&] { unavailable.GetArmor(); }, "default optional owner explicitly refuses armour access");
    Refuses([&] { unavailable.SetHealth(1); }, "default optional owner explicitly refuses health mutation");
    Refuses([&] { unavailable.SetArmor(1); }, "default optional owner explicitly refuses armour mutation");
    std::printf("Passed %u actual CPed vitals SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
}
