/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CPed scalar and mutation SDK slices through one registered token
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedMutationDE.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>

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
#include "PedMutationFixture.generated.h"

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
    class MetadataOwner final : public NativePedMetadataAccess
    {
    public:
        NativePedReferenceToken token;
        NativePedMetadata       supplied;
        ClockResult             result = ClockResult::Ok;
        unsigned                reads = 0;
        ClockResult             ReadPedMetadata(const NativePedReferenceToken& requested, NativePedMetadata& output) noexcept override
        {
            ++reads;
            token = requested;
            output = supplied;
            return result;
        }
    };
    class RefusingMetadataOwner final : public NativePedMetadataAccess
    {
    };
    class MutationOwner final : public NativePedMutationAccess
    {
    public:
        NativePedReferenceToken      token;
        NativePedVelocityKind        kind = NativePedVelocityKind::Linear;
        std::array<std::uint32_t, 3> bits{};
        ClockResult                  result = ClockResult::Ok;
        unsigned                     writes = 0;
        ClockResult SetReferencedPedVelocity(const NativePedReferenceToken& requested, NativePedVelocityKind selected, const CVector& value) noexcept override
        {
            ++writes;
            token = requested;
            kind = selected;
            std::memcpy(bits.data(), &value, sizeof(value));
            return result;
        }
    };
    class RefusingMutationOwner final : public NativePedMutationAccess
    {
    };
    class PhysicalScalarsOwner final : public NativePedPhysicalScalarsAccess
    {
    public:
        NativePedReferenceToken      token;
        NativePedPhysicalScalarField field = NativePedPhysicalScalarField::Mass;
        ClockResult                  result = ClockResult::Ok;
        std::uint32_t                supplied = 0, bits = 0;
        unsigned                     reads = 0, writes = 0;
        std::array<std::uint32_t, 3> vectorSupplied{}, vectorBits{};
        unsigned                     vectorReads = 0, vectorWrites = 0;
        ClockResult ReadPedPhysicalScalar(const NativePedReferenceToken& requested, NativePedPhysicalScalarField selected, float& output) noexcept override
        {
            ++reads;
            token = requested;
            field = selected;
            std::memcpy(&output, &supplied, sizeof(output));
            return result;
        }
        ClockResult SetPedPhysicalScalar(const NativePedReferenceToken& requested, NativePedPhysicalScalarField selected, float value) noexcept override
        {
            ++writes;
            token = requested;
            field = selected;
            std::memcpy(&bits, &value, sizeof(bits));
            return result;
        }
        ClockResult ReadPedCenterOfMass(const NativePedReferenceToken& requested, CVector& output) noexcept override
        {
            ++vectorReads;
            token = requested;
            std::memcpy(&output, vectorSupplied.data(), sizeof(output));
            return result;
        }
        ClockResult SetPedCenterOfMass(const NativePedReferenceToken& requested, const CVector& input) noexcept override
        {
            ++vectorWrites;
            token = requested;
            std::memcpy(vectorBits.data(), &input, sizeof(input));
            return result;
        }
    };
    void DynamicsSdkChecks(CPed& ped, PhysicalScalarsOwner& owner, const NativePedReferenceToken& token)
    {
        const std::array<std::uint32_t, 7> values{0, 0x80000000, 0x3f800001, 0xc0200000, 0x7fc01234, 0x7fa54321, 0x7f800000};
        for (const auto bits : values)
        {
            owner.supplied = bits;
            auto calls = owner.reads;
            Check(Bits(ped.GetAirResistance()) == bits && owner.reads == calls + 1 && owner.field == NativePedPhysicalScalarField::AirResistance &&
                      SameToken(owner.token, token),
                  "actual air-resistance SDK getter selects its native scalar and preserves raw owner bits");
            calls = owner.reads;
            Check(Bits(ped.GetElasticity()) == bits && owner.reads == calls + 1 && owner.field == NativePedPhysicalScalarField::Elasticity &&
                      SameToken(owner.token, token),
                  "actual elasticity SDK getter uses the same immutable registered token without clamping");
            calls = owner.writes;
            ped.SetAirResistance(Float(bits));
            Check(
                owner.writes == calls + 1 && owner.bits == bits && owner.field == NativePedPhysicalScalarField::AirResistance && SameToken(owner.token, token),
                "actual air-resistance SDK setter forwards raw bits exactly once");
            calls = owner.writes;
            ped.SetElasticity(Float(bits));
            Check(owner.writes == calls + 1 && owner.bits == bits && owner.field == NativePedPhysicalScalarField::Elasticity && SameToken(owner.token, token),
                  "actual elasticity SDK setter keeps its independent field identity and raw intent");
            owner.vectorSupplied = {bits, bits ^ 0x80000000u, 0x80000000};
            CVector center;
            calls = owner.vectorReads;
            ped.GetCenterOfMass(center);
            Check(owner.vectorReads == calls + 1 && !std::memcmp(&center, owner.vectorSupplied.data(), sizeof(center)) && SameToken(owner.token, token),
                  "actual center-of-mass SDK getter publishes one acknowledged local-space vector with exact float bits");
            calls = owner.vectorWrites;
            ped.SetCenterOfMass(center);
            Check(owner.vectorWrites == calls + 1 && owner.vectorBits == owner.vectorSupplied && SameToken(owner.token, token),
                  "actual center-of-mass SDK setter forwards its three raw components without conversion or replay");
        }
        for (std::uint32_t value = static_cast<std::uint32_t>(ClockResult::InvalidArgument);
             value <= static_cast<std::uint32_t>(ClockResult::OriginalException); ++value)
        {
            owner.result = static_cast<ClockResult>(value);
            auto calls = owner.reads;
            Refuses([&]() { ped.GetAirResistance(); }, "each air-resistance read refusal remains an observable SDK failure");
            Refuses([&]() { ped.GetElasticity(); }, "each elasticity read refusal remains an observable SDK failure");
            Check(owner.reads == calls + 2, "refused dynamics scalar reads do not retry");
            calls = owner.writes;
            Refuses([&]() { ped.SetAirResistance(Float(0x7fc01234)); }, "air-resistance setter refuses without fabricated success");
            Refuses([&]() { ped.SetElasticity(Float(0x80000000)); }, "elasticity setter refuses without swallowing entered native effects");
            Check(owner.writes == calls + 2 && owner.bits == 0x80000000, "refused scalar intents preserve exact input and are never replayed");
            const std::array<std::uint32_t, 3> untouched{0x4a123456, 0x80000000, 0xffc01234};
            CVector                            center;
            std::memcpy(&center, untouched.data(), sizeof(center));
            calls = owner.vectorReads;
            Refuses([&]() { ped.GetCenterOfMass(center); }, "failed vector acknowledgement throws even if the owner touched its local output");
            Check(owner.vectorReads == calls + 1 && !std::memcmp(&center, untouched.data(), sizeof(center)),
                  "failed center-of-mass read preserves the caller's entire vector");
            calls = owner.vectorWrites;
            Refuses([&]() { ped.SetCenterOfMass(center); }, "failed center-of-mass intent is reported without rollback or replay");
            Check(owner.vectorWrites == calls + 1 && owner.vectorBits == untouched && SameToken(owner.token, token),
                  "refused vector intent retains exact raw arguments and immutable token once");
        }
        owner.result = ClockResult::Ok;
    }
}

int main()
{
    using namespace mta::game_de;
    static_assert(sizeof(CVector) == sizeof(std::array<std::uint32_t, 3>));
    static_assert(noexcept(std::declval<CPed&>().SetMoveSpeed(std::declval<const CVector&>())));
    static_assert(noexcept(std::declval<CPedMutationDE&>().TrySetMoveSpeed(std::declval<const CVector&>())));
    static_assert(std::is_abstract_v<mta::game_de::CPedMutationDE>);
    static_assert(std::is_base_of_v<CPed, mta::game_de::CPedMutationDE>);
    static_assert(std::is_base_of_v<mta::game_de::CPedVitalsDE, mta::game_de::CPedMutationDE>);
    static_assert(!std::is_abstract_v<fixture::PedMutationFixture>);
    Check(fixture::RejectingMethods > 100, "the generated concrete test type rejects the remaining actual inherited SDK surface");
    NativePedReferenceToken token;
    token.version = 0x123456789;
    token.ownerGeneration = 0xfedcba987;
    token.slot = 23;
    const auto           original = token;
    Owner                owner;
    MetadataOwner        metadata;
    MutationOwner        mutation;
    PhysicalScalarsOwner scalars;
    int                  clientIdentity = 0x1234, replacementIdentity = 0x5678;
    {
        fixture::PedMutationFixture instance(owner, metadata, mutation, token, &scalars);
        CPed&                       ped = instance;
        const CPed&                 constant = instance;
        CEntity&                    clientEntity = instance;
        Check(clientEntity.GetStoredPointer() == nullptr, "actual inherited SDK client association starts null without native access");
        clientEntity.SetStoredPointer(&clientIdentity);
        Check(clientEntity.GetStoredPointer() == &clientIdentity && !owner.reads && !owner.writes && !metadata.reads && !mutation.writes,
              "MTA local-model startup can store and retrieve its exact client identity without an engine lease");
        {
            fixture::PedMutationFixture second(owner, metadata, mutation, original);
            Check(second.GetStoredPointer() == nullptr, "another SDK instance has an independent empty client association");
            second.SetStoredPointer(&replacementIdentity);
            Check(second.GetStoredPointer() == &replacementIdentity && clientEntity.GetStoredPointer() == &clientIdentity,
                  "per-instance stored client identities neither share storage nor replace each other");
            second.SetStoredPointer(nullptr);
            Check(second.GetStoredPointer() == nullptr && clientEntity.GetStoredPointer() == &clientIdentity,
                  "null explicitly clears only the selected SDK association");
        }
        clientEntity.SetStoredPointer(&replacementIdentity);
        Check(clientEntity.GetStoredPointer() == &replacementIdentity, "client reassociation stores the replacement opaque identity exactly");
        clientEntity.SetStoredPointer(&clientIdentity);
        // The layer copies only CPU token identity. Mutating the caller's
        // original metadata must not redirect the already-bound SDK instance.
        token.version = 7;
        token.ownerGeneration = 8;
        token.slot = 9;
        Check(instance.LastMoveSpeedResult() == ClockResult::NotInitialized && !mutation.writes,
              "the abstract SDK mutation layer starts without fabricating an accepted native setter result");
        const std::array<std::uint32_t, 3> rawSpeed{0x7fc01234, 0x80000000, 0x7f800000};
        CVector                            speed;
        std::memcpy(&speed, rawSpeed.data(), sizeof(speed));
        auto mutationCalls = mutation.writes;
        ped.SetMoveSpeed(speed);
        Check(mutation.writes == mutationCalls + 1 && mutation.kind == NativePedVelocityKind::Linear && mutation.bits == rawSpeed &&
                  SameToken(mutation.token, original) && instance.LastMoveSpeedResult() == ClockResult::Ok,
              "actual noexcept CPed linear setter forwards raw vector and token once and records its owner result");
        mutationCalls = mutation.writes;
        Check(instance.TrySetMoveSpeed(speed) == ClockResult::Ok && mutation.writes == mutationCalls + 1 && instance.LastMoveSpeedResult() == ClockResult::Ok,
              "the explicit Try path returns the same single native-owner result without changing the SDK noexcept contract");
        mutationCalls = mutation.writes;
        ped.SetTurnSpeed(&speed);
        Check(mutation.writes == mutationCalls + 1 && mutation.kind == NativePedVelocityKind::Angular && mutation.bits == rawSpeed &&
                  SameToken(mutation.token, original),
              "actual CPed angular setter consumes the supplied vector through its typed borrowed owner");
        mutationCalls = mutation.writes;
        Refuses([&] { ped.SetTurnSpeed(nullptr); }, "null angular vector refuses before dereference or native owner entry");
        Check(mutation.writes == mutationCalls, "null angular input never reaches the typed owner or retries another intent");
        for (const auto model : {std::uint16_t{0}, std::uint16_t{1234}, std::uint16_t{0xffff}})
        {
            metadata.supplied.modelIndex = model;
            const auto calls = metadata.reads;
            Check(ped.GetModelIndex() == model && metadata.reads == calls + 1 && SameToken(metadata.token, original) && !owner.reads && !owner.writes,
                  "actual inherited model getter forwards exact token and native WORD without invoking vitals");
        }
        for (const auto type : {std::uint8_t{0}, std::uint8_t{3}, std::uint8_t{6}})
        {
            metadata.supplied.entityType = type;
            const auto calls = metadata.reads;
            CEntity&   entity = instance;
            Check(static_cast<unsigned>(entity.GetEntityType()) == type && metadata.reads == calls + 1 && SameToken(metadata.token, original),
                  "actual CEntity virtual dispatch consumes the typed owner's entity value instead of a guessed constant");
        }
        DynamicsSdkChecks(ped, scalars, original);
        const std::array<std::uint32_t, 7> values{0, 0x80000000, 0x3f800001, 0xc0200000, 0x7fc01234, 0xffc07654, 0x7f800000};
        for (const auto bits : values)
        {
            scalars.supplied = bits;
            auto scalarCalls = scalars.reads;
            Check(Bits(ped.GetMass()) == bits && scalars.reads == scalarCalls + 1 && scalars.field == NativePedPhysicalScalarField::Mass &&
                      SameToken(scalars.token, original),
                  "actual inherited mass getter preserves raw owner float bits through the same immutable registered token");
            scalarCalls = scalars.reads;
            Check(Bits(ped.GetTurnMass()) == bits && scalars.reads == scalarCalls + 1 && scalars.field == NativePedPhysicalScalarField::TurnMass &&
                      SameToken(scalars.token, original),
                  "actual turn-mass getter selects the separate qualified native scalar without arithmetic");
            scalarCalls = scalars.writes;
            ped.SetMass(Float(bits));
            Check(scalars.writes == scalarCalls + 1 && scalars.bits == bits && scalars.field == NativePedPhysicalScalarField::Mass &&
                      SameToken(scalars.token, original),
                  "actual mass setter forwards one raw native intent without clamping or snapshot repair");
            scalarCalls = scalars.writes;
            ped.SetTurnMass(Float(bits));
            Check(scalars.writes == scalarCalls + 1 && scalars.bits == bits && scalars.field == NativePedPhysicalScalarField::TurnMass &&
                      SameToken(scalars.token, original),
                  "actual turn-mass setter preserves NaN payloads and signed zero through the typed owner");
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
            metadata.result = static_cast<ClockResult>(value);
            auto metadataReads = metadata.reads;
            Refuses([&] { ped.GetModelIndex(); }, "every model owner refusal throws instead of reporting unacknowledged metadata");
            Check(metadata.reads == metadataReads + 1 && SameToken(metadata.token, original), "failed model read is attempted once with the immutable token");
            metadataReads = metadata.reads;
            Refuses([&] { ped.GetEntityType(); }, "every entity-type owner refusal throws instead of manufacturing an enum");
            Check(metadata.reads == metadataReads + 1 && SameToken(metadata.token, original), "failed type read is attempted once without retry");
            owner.result = static_cast<ClockResult>(value);
            scalars.result = static_cast<ClockResult>(value);
            auto scalarReads = scalars.reads;
            Refuses([&] { ped.GetMass(); }, "every explicit mass owner refusal throws instead of manufacturing a float");
            Check(scalars.reads == scalarReads + 1, "failed mass observation is attempted once without retry");
            scalarReads = scalars.reads;
            Refuses([&] { ped.GetTurnMass(); }, "every explicit turn-mass owner refusal throws");
            Check(scalars.reads == scalarReads + 1, "failed turn-mass observation is attempted once");
            auto scalarWrites = scalars.writes;
            Refuses([&] { ped.SetMass(Float(0x7fc01234)); }, "every explicit mass setter refusal remains an observable SDK failure");
            Check(scalars.writes == scalarWrites + 1 && scalars.bits == 0x7fc01234, "failed mass intent preserves raw arguments without retry");
            scalarWrites = scalars.writes;
            Refuses([&] { ped.SetTurnMass(Float(0x80000000)); }, "every explicit turn-mass setter refusal remains observable");
            Check(scalars.writes == scalarWrites + 1 && scalars.bits == 0x80000000, "failed turn-mass intent preserves signed zero without replay");
            mutation.result = static_cast<ClockResult>(value);
            mutationCalls = mutation.writes;
            ped.SetMoveSpeed(speed);
            Check(mutation.writes == mutationCalls + 1 && mutation.bits == rawSpeed && mutation.kind == NativePedVelocityKind::Linear &&
                      instance.LastMoveSpeedResult() == mutation.result && SameToken(mutation.token, original),
                  "every noexcept linear failure is recorded without throwing, clamping or replaying the accepted intent");
            mutationCalls = mutation.writes;
            Check(
                instance.TrySetMoveSpeed(speed) == mutation.result && mutation.writes == mutationCalls + 1 && instance.LastMoveSpeedResult() == mutation.result,
                "explicit Try reports each refusing owner status exactly once");
            mutationCalls = mutation.writes;
            Refuses([&] { ped.SetTurnSpeed(&speed); }, "every angular owner refusal propagates as an explicit SDK failure");
            Check(mutation.writes == mutationCalls + 1 && mutation.bits == rawSpeed && mutation.kind == NativePedVelocityKind::Angular &&
                      SameToken(mutation.token, original),
                  "a failed angular intent preserves raw input and immutable token without retry");
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
        const auto storedReads = owner.reads, storedWrites = owner.writes, storedMetadata = metadata.reads, storedMutation = mutation.writes;
        Check(clientEntity.GetStoredPointer() == &clientIdentity, "native setter refusals do not discard independent MTA client bookkeeping");
        clientEntity.SetStoredPointer(nullptr);
        Check(clientEntity.GetStoredPointer() == nullptr && owner.reads == storedReads && owner.writes == storedWrites && metadata.reads == storedMetadata &&
                  mutation.writes == storedMutation,
              "clearing client association invokes no native or typed owner after a refused native operation");
        clientEntity.SetStoredPointer(&clientIdentity);
    }
    Check(clientIdentity == 0x1234 && replacementIdentity == 0x5678, "SDK destruction never dereferences, deletes or rewrites client pointees");
    const auto reads = owner.reads, writes = owner.writes, metadataReads = metadata.reads, mutationWrites = mutation.writes;
    const auto scalarReads = scalars.reads, scalarWrites = scalars.writes, vectorReads = scalars.vectorReads, vectorWrites = scalars.vectorWrites;
    {
        fixture::PedMutationFixture unused(owner, metadata, mutation, original, &scalars);
    }
    Check(owner.reads == reads && owner.writes == writes && metadata.reads == metadataReads && mutation.writes == mutationWrites,
          "SDK construction and destruction perform no native registration, cleanup or field operation");
    Check(scalars.reads == scalarReads && scalars.writes == scalarWrites && scalars.vectorReads == vectorReads && scalars.vectorWrites == vectorWrites,
          "optional scalar-owner lifetime is borrowed without constructor/destructor native work");
    RefusingOwner               refusing;
    RefusingMetadataOwner       refusingMetadata;
    RefusingMutationOwner       refusingMutation;
    fixture::PedMutationFixture unavailable(refusing, refusingMetadata, refusingMutation, original);
    Refuses([&] { unavailable.GetHealth(); }, "default optional owner explicitly refuses health access");
    Refuses([&] { unavailable.GetArmor(); }, "default optional owner explicitly refuses armour access");
    Refuses([&] { unavailable.SetHealth(1); }, "default optional owner explicitly refuses health mutation");
    Refuses([&] { unavailable.SetArmor(1); }, "default optional owner explicitly refuses armour mutation");
    Refuses([&] { unavailable.GetMass(); }, "missing optional scalar owner explicitly refuses mass instead of returning a cached default");
    Refuses([&] { unavailable.GetTurnMass(); }, "missing optional scalar owner explicitly refuses turn mass");
    Refuses([&] { unavailable.SetMass(1); }, "missing optional scalar owner explicitly refuses mass mutation");
    Refuses([&] { unavailable.SetTurnMass(1); }, "missing optional scalar owner explicitly refuses turn-mass mutation");
    Refuses([&] { unavailable.GetModelIndex(); }, "default metadata owner explicitly refuses the model field");
    Refuses([&] { unavailable.GetEntityType(); }, "default metadata owner explicitly refuses the entity type");
    Refuses([&] { unavailable.GetAirResistance(); }, "missing scalar owner refuses air resistance");
    Refuses([&] { unavailable.SetAirResistance(1); }, "missing scalar owner refuses air-resistance intent");
    Refuses([&] { unavailable.GetElasticity(); }, "missing scalar owner refuses elasticity");
    Refuses([&] { unavailable.SetElasticity(1); }, "missing scalar owner refuses elasticity intent");
    CVector center(1, 2, 3);
    Refuses([&] { unavailable.GetCenterOfMass(center); }, "missing scalar owner refuses center-of-mass observation");
    Check(center.fX == 1 && center.fY == 2 && center.fZ == 3, "missing optional owner preserves the caller's vector");
    Refuses([&] { unavailable.SetCenterOfMass(center); }, "missing scalar owner refuses center-of-mass intent");
    const CVector zero(0, 0, 0);
    unavailable.SetMoveSpeed(zero);
    Check(unavailable.LastMoveSpeedResult() == ClockResult::NotInitialized, "default linear owner refusal remains observable through the noexcept SDK setter");
    Check(unavailable.TrySetMoveSpeed(zero) == ClockResult::NotInitialized, "default explicit Try path refuses without fabricating native readiness");
    CVector angular(1, 2, 3);
    Refuses([&] { unavailable.SetTurnSpeed(&angular); }, "default angular owner refuses explicitly");
    std::puts("Production MTA ped stored pointer SDK fixture completed");
    std::puts("Production MTA ped physical scalar SDK fixture completed");
    std::puts("Production MTA ped physical dynamics SDK fixture completed");
    std::printf("Passed %u actual CPed mutation SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
}
