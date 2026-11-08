/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual extended property SDK policy, native writes and retained debt
 ****************************************************************************/
#include "NativeObjectPropertiesExtendedFixture.h"
#include "ObjectPropertiesExtendedFixture.generated.h"
namespace ObjectPropertiesExtendedOwnerFixture
{
    namespace P = NativeObjectGroupPropertiesProfile;
    namespace M = NativeObjectGroupPropertiesMemory;
    namespace F = NativeObjectPropertiesExtendedFixture;
    namespace Fault = ClockObjectPropertiesExtendedTest;
    using Field = NativeObjectGroupProperty;
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite extended property lifetime is selected");
        const bool retained = scenario == 1, unknown = scenario == 2;
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes writes{fixture, {}};
        const auto                      apply = [&](std::uintptr_t rva, const void* value, std::size_t bytes, DWORD protection)
        { WorldGroundOwnerFixture::Writes::Apply(&writes, rva, value, bytes, protection); };
        const auto code = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        const auto typed = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                apply(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        code(NativePedPoolProfile::Code);
        code(NativeModelBankProfile::Code);
        code(NativeStreamingLoadAllProfile::Code);
        typed(NativeStartupOwnerProfile::pins);
        typed(NativeStartupWorldProfile::pins);
        typed(NativeGameStateProfile::pins);
        code(NativeUObjectMembershipProfile::pins);
        code(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                apply(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        typed(P::Pins);
        auto graph = std::make_unique<NativeStreamingLoadAllFixture::Graph>(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                                            &WorldGroundOwnerFixture::Writes::Apply, &writes);
        std::array<unsigned char, P::TableBytes> table{};
        table.fill(0xa5);
        for (std::size_t row = 0; row < P::BuiltinRows; ++row)
            for (std::size_t index = 0; index < P::PropertyCount; ++index)
            {
                const auto    field = static_cast<Field>(index);
                const auto    descriptor = P::Descriptors[index];
                std::uint32_t bits = descriptor.bytes == 1 ? std::uint32_t{1} : Bits(static_cast<float>(index + 1));
                if (field == Field::CollisionSpecialResponseCase)
                    bits = 2;
                if (field == Field::FxType)
                    bits = 3;
                if (field == Field::CameraAvoidObject || field == Field::SparksOnImpact)
                    bits = 0x80;
                if (field == Field::CausesExplosion)
                    bits = 0xff;
                if (field == Field::BreakMode)
                    bits = 0x11223344;
                std::memcpy(table.data() + row * P::RowBytes + descriptor.offset, &bits, descriptor.bytes);
            }
        // The opaque FX word and every padding byte are deliberately invalid
        // as live pointers. No inherited method may dereference or copy them.
        apply(P::TableRva, table.data(), table.size(), PAGE_READWRITE);
        const auto row = fixture.image + P::TableRva;
        const auto original = F::Raw(row);
        const auto raw = [&](Field field) { return F::Read(row, field); };
        const auto put = [&](Field field, std::uint32_t bits) { F::Store(row, field, bits); };
        const auto unowned = [&] { return F::UnownedSame(original, F::Raw(row)); };
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Fault::row = row;
        Fault::attempts = {};
        Fault::accepted = {};
        Fault::raceWord = Fault::raceByte = Fault::unknownWord = P::RowBytes;
        Fault::injected = false;
        NativeClockService                service;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceRequestV1 request;
        request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, state;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild,
              "builtin properties preserve normal shipping refusal of the owned test executable");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, state, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && state.bound && runtime.frameHooked.load(), "extended properties use the actual existing post-application Runtime");
        const auto generation = state.generation;
        const auto pulse = [&](const auto& action, bool active = true)
        {
            unsigned called = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++called;
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "extended property work executes on the actual distinct admitted engine callback");
                action(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!active || !runtime.stopping.load()), "one requested property action consumes one current callback");
        };
        float output = 123;
        Check(runtime.access.ReadObjectGroupProperty(0, Field::UprootLimit, output) != ClockResult::Ok && output == 123 &&
                  !runtime.objectGroupProperties.Pending(),
              "producer-thread refusal preserves the extended scalar output without capturing a default");
        if (retained || unknown)
        {
            bool modified = false;
            Fault::unknownWord = unknown ? 0x14 : P::RowBytes;
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    const auto result = owner.SetObjectGroupProperty(0, Field::UprootLimit, 99.0f, modified);
                    Check(
                        (unknown ? result != ClockResult::Ok : result == ClockResult::Ok) && modified && raw(Field::UprootLimit) == Bits(99.0f) &&
                            runtime.objectGroupProperties.Pending() && runtime.objectGroupProperties.Uncertain() == unknown &&
                            runtime.stopping.load() == unknown && Fault::injected == unknown,
                        "the actual new scalar CAS retains its facade flag and original through either known acknowledgement or full-row post-ACK uncertainty");
                    if (retained)
                        Check(owner.RestoreObjectGroupProperties(0, modified) == ClockResult::Ok && !modified && F::Raw(row) == original &&
                                  runtime.objectGroupProperties.Pending(),
                              "single restore clears this facade while the physically clean first-original registry remains retained");
                },
                !unknown);
            if (unknown)
            {
                // Restore only this test's independent opaque source mutation.
                // The real unknown effect remains sticky and cannot be replayed.
                std::memcpy(reinterpret_cast<void*>(row + 0x30), original.data() + 0x30, 8);
                const auto counts = Fault::attempts;
                Check(runtime.objectGroupProperties.RestoreAll(fixture.image, generation, &CheckTimingOwner, &CheckApplicationTimingOwner) ==
                              ClockResult::RestoreFailure &&
                          Fault::attempts == counts && runtime.objectGroupProperties.Pending(),
                      "restoring independent FX representation cannot erase an entered-unknown property intent or replay any CAS");
            }
            const auto counts = Fault::attempts;
            Check(!service.End(generation), "actual End refuses retained extended property originals");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(!service.End(generation) && !restarted && runtime.objectGroupProperties.Pending() && unowned() && Fault::attempts == counts &&
                      raw(Field::UprootLimit) == Bits(unknown ? 99.0f : 6.0f),
                  "stopped cleanup and a new Begin retain extended property debt without automatic restoration or CAS replay");
            engine.Stop();
            std::puts(unknown ? "Production extended object properties unknown completed" : "Production extended object properties retained completed");
            return;
        }
        pulse(
            [&](NativeGameTimingAccess& owner)
            {
                const auto word = [](Field field) { return P::Descriptors[static_cast<std::size_t>(field)].offset / 4; };
                const auto drain = [&]
                {
                    Check(owner.RestoreAllObjectGroupProperties() == ClockResult::Ok && !runtime.objectGroupProperties.Pending() && unowned(),
                          "explicit admitted-field cleanup drains the registry while preserving all FX and padding bytes");
                };
                {
                    ObjectPropertiesExtendedObserver a(0, owner);
                    Check(a.GetGroup() == 0 && a.GetMass() == 1 && a.GetTurnMass() == 2 && a.GetAirResistance() == 3 && a.GetElasticity() == 4 &&
                              a.GetBuoyancy() == 5 && a.GetUprootLimit() == 6 && a.GetCollisionDamageMultiplier() == 7 && a.GetSmashMultiplier() == 16 &&
                              a.GetBreakVelocityRandomness() == 20,
                          "all nine actual scalar SDK getters read their independently mapped native Float32 fields");
                    bool       hasOriginal = true;
                    const auto noops = Fault::attempts;
                    a.SetMass(1.00001f);
                    a.SetFxOffset(CVector(13.00001f, 14.0f, 15.0f));
                    a.SetCameraAvoidObject(true);
                    a.SetCausesExplosion(true);
                    a.SetSparksOnImpact(true);
                    Check(owner.HasOriginalObjectGroupProperties(0, hasOriginal) == ClockResult::Ok && !hasOriginal && Fault::attempts == noops &&
                              a.GetCameraAvoidObject() && a.GetCausesExplosion() && a.GetSparksOnImpact(),
                          "actual scalar/vector epsilon and opaque nonzero boolean equality skip defaults and all native CAS instructions");
                    a.SetCollisionDamageEffect(static_cast<ObjectGroupPhysicalProperties::DamageEffect>(257));
                    Check(raw(Field::CollisionDamageEffect) == 1 && static_cast<unsigned>(a.GetCollisionDamageEffect()) == 1 &&
                              owner.HasOriginalObjectGroupProperties(0, hasOriginal) == ClockResult::Ok && hasOriginal &&
                              Fault::attempts[word(Field::CollisionDamageEffect)] == noops[word(Field::CollisionDamageEffect)] + 1,
                          "full enum257 differs from currentBYTE1 before genuine narrowing, so classic first-change state and a real same-byte CAS occur");
                    a.RestoreDefault();
                    Check(F::Raw(row) == original && runtime.objectGroupProperties.Pending(),
                          "actual single-facade restore preserves its independently retained original registry");
                }
                drain();
                {
                    ObjectPropertiesExtendedObserver a(0, owner);
                    a.SetMass(10);
                    a.SetTurnMass(20);
                    a.SetAirResistance(30);
                    a.SetElasticity(40);
                    a.SetBuoyancy(50);
                    Check(a.GetMass() == 10 && a.GetTurnMass() == 20 && a.GetAirResistance() == 30 && a.GetElasticity() == 40 && a.GetBuoyancy() == 50,
                          "each of the original five actual SDK setters changes its corresponding real native field");
                    ObjectPropertiesExtendedObserver b(0, owner);
                    a.RestoreDefault();
                    put(Field::TurnMass, Bits(55.0f));
                    b.RestoreDefault();
                    Check(raw(Field::TurnMass) == Bits(2.0f),
                          "a second wrapper retains its own dirty flag and restores an unacknowledged foreign TurnMass55 to the first original");
                    a.SetAirResistance(30);
                    b.RestoreDefault();
                    Check(raw(Field::AirResistance) == Bits(30.0f), "a clean second wrapper cannot inherit another wrapper's later modified flag");
                    ObjectPropertiesExtendedObserver c(0, owner);
                    c.RestoreDefault();
                    Check(raw(Field::AirResistance) == Bits(3.0f), "a new wrapper reads original-map membership and restores the independent first original");
                    a.RestoreDefault();
                }
                drain();
                {
                    ObjectPropertiesExtendedObserver a(0, owner);
                    a.SetUprootLimit(-17.0f);
                    a.SetCollisionDamageMultiplier(F::Float(0x7fc12345u));
                    a.SetSmashMultiplier(F::Float(0x80000000u));
                    a.SetBreakVelocityRandomness(2.5f);
                    a.SetCollisionSpecialResponseCase(static_cast<ObjectGroupPhysicalProperties::CollisionResponse>(0x102));
                    a.SetFxType(static_cast<ObjectGroupPhysicalProperties::FxType>(0x103));
                    a.SetBreakMode(static_cast<ObjectGroupPhysicalProperties::BreakMode>(0x7fffffff));
                    a.SetCameraAvoidObject(false);
                    a.SetCausesExplosion(false);
                    a.SetSparksOnImpact(false);
                    Check(raw(Field::UprootLimit) == Bits(-17.0f) && raw(Field::CollisionDamageMultiplier) == 0x7fc12345u &&
                              raw(Field::SmashMultiplier) == 0x80000000u && a.GetBreakVelocityRandomness() == 2.5f &&
                              static_cast<unsigned>(a.GetCollisionSpecialResponseCase()) == 2 && static_cast<unsigned>(a.GetFxType()) == 3 &&
                              static_cast<unsigned>(a.GetBreakMode()) == 0x7fffffffu && !a.GetCameraAvoidObject() && !a.GetCausesExplosion() &&
                              !a.GetSparksOnImpact() && unowned(),
                          "the actual extended SDK preserves float bits, narrows enum bytes, uses native DWORD break mode, and modifies only the six proven "
                          "bytes");
                    a.SetFxOffset(CVector(F::Float(0x7fc54321u), 0.0f, F::Float(0x80000000u)));
                    const auto nanCounts = Fault::attempts;
                    a.SetFxOffset(CVector(F::Float(0x7fc54321u), 0.0f, F::Float(0x80000000u)));
                    const auto vector = a.GetFxOffset();
                    Check(Bits(vector.fX) == 0x7fc54321u && Bits(vector.fY) == 0 && Bits(vector.fZ) == 0x80000000u &&
                              Fault::attempts[0x24 / 4] == nanCounts[0x24 / 4] + 1 && Fault::attempts[0x28 / 4] == nanCounts[0x28 / 4] + 1 &&
                              Fault::attempts[0x2c / 4] == nanCounts[0x2c / 4] + 1,
                          "actual CVector NaN inequality causes another three genuine writes while preserving payload and signed-zero components");
                    a.SetBreakVelocity(CVector(71, 72, 73));
                    const auto velocity = a.GetBreakVelocity();
                    Check(velocity == CVector(71, 72, 73), "the actual second vector getter/setter maps its three distinct native DWORDs");
                    a.RestoreDefault();
                    Check(F::Raw(row) == original && raw(Field::CameraAvoidObject) == 0x80 && raw(Field::CausesExplosion) == 0xff &&
                              raw(Field::SparksOnImpact) == 0x80,
                          "restoring admitted fields retains original opaque boolean bytes rather than normalizing them or transferring FX representation");
                }
                drain();
                {
                    M::Value value;
                    Check(M::Capture(fixture.image, generation, 0, &CheckTimingOwner, &CheckApplicationTimingOwner, value) == ClockResult::Ok,
                          "memory refusal cases borrow the current genuine typed world and complete row");
                    const auto counts = Fault::attempts;
                    auto       rawSeal = value.raw;
                    Check(!M::Patch(rawSeal, Field::CameraAvoidObject, 256) && rawSeal == value.raw,
                          "the pure CPU patch refuses a too-wide byte without changing any raw seal bytes");
                    *reinterpret_cast<unsigned char*>(row + 0x21) ^= 1;
                    M::Receipt receipt;
                    Check(M::Write(fixture.image, value, Field::UprootLimit, Bits(88.0f), &CheckTimingOwner, &CheckApplicationTimingOwner, receipt) !=
                                  ClockResult::Ok &&
                              !receipt.entered && Fault::attempts == counts,
                          "a stale full-row padding preimage refuses native entry even when all22 logical fields still match");
                    *reinterpret_cast<unsigned char*>(row + 0x21) ^= 1;
                    Check(M::Capture(fixture.image, generation, 0, &CheckTimingOwner, &CheckApplicationTimingOwner, value) == ClockResult::Ok,
                          "a new current row capture follows explicit source restoration");
                    Fault::raceWord = 0x1c;
                    Fault::raceByte = 0x1d;
                    Fault::injected = false;
                    Check(M::Write(fixture.image, value, Field::CameraAvoidObject, 0, &CheckTimingOwner, &CheckApplicationTimingOwner, receipt) ==
                                  ClockResult::OwnerConflict &&
                              receipt.entered && !receipt.accepted && !receipt.uncertain && receipt.observed == 0x80 &&
                              receipt.wordObserved != receipt.wordBefore && raw(Field::CameraAvoidObject) == 0x80 &&
                              raw(Field::CollisionSpecialResponseCase) == 3,
                          "the real masked CAS refuses a competing sibling BYTE even though its selected logical boolean still has the expected value");
                    put(Field::CollisionSpecialResponseCase, 2);
                    Check(F::Raw(row) == original, "the fixture's competing source is explicitly repaired without replaying a refused write");
                }
                {
                    ObjectPropertiesExtendedObserver a(0, owner);
                    const auto                       counts = Fault::attempts;
                    Fault::raceWord = Fault::raceByte = 0x28;
                    Fault::injected = false;
                    bool threw = false;
                    try
                    {
                        a.SetFxOffset(CVector(100, 200, 300));
                    }
                    catch (const std::runtime_error&)
                    {
                        threw = true;
                    }
                    Check(threw && Fault::injected && raw(Field::FxOffsetX) == Bits(100.0f) && raw(Field::FxOffsetZ) == Bits(15.0f) &&
                              Fault::attempts[0x24 / 4] == counts[0x24 / 4] + 1 && Fault::attempts[0x2c / 4] == counts[0x2c / 4] &&
                              runtime.objectGroupProperties.Pending() && !runtime.objectGroupProperties.Uncertain(),
                          "the real SDK throws after an acknowledged first vector axis and a genuine competing-preimage failure on the second");
                    a.RestoreDefault();
                    Check(F::Raw(row) == original && runtime.objectGroupProperties.Pending(),
                          "RestoreDefault immediately after that throw proves the SDK's own modified flag was set before native entry failed");
                }
                drain();
                {
                    bool                        modified = false;
                    NativeObjectGroupVectorBits intent{Bits(100.0f), Bits(200.0f), Bits(300.0f)};
                    Fault::raceWord = Fault::raceByte = 0x28;
                    Fault::injected = false;
                    Check(owner.SetObjectGroupVector(0, Field::FxOffsetX, intent, modified) == ClockResult::OwnerConflict && modified && Fault::injected,
                          "the actual native owner reports an accepted vector prefix while publishing the caller's dirty flag before refusal");
                    const auto counts = Fault::attempts;
                    auto       different = intent;
                    different[2] = Bits(301.0f);
                    Check(owner.SetObjectGroupVector(0, Field::FxOffsetX, different, modified) == ClockResult::RestoreFailure && Fault::attempts == counts,
                          "a different vector intent cannot borrow or overwrite a retained accepted prefix");
                    put(Field::FxOffsetX, Bits(101.0f));
                    Check(owner.SetObjectGroupVector(0, Field::FxOffsetX, intent, modified) == ClockResult::OwnerConflict && Fault::attempts == counts,
                          "foreign drift in an already accepted vector axis refuses same-intent retry without replay");
                    put(Field::FxOffsetX, Bits(100.0f));
                    put(Field::FxOffsetY, Bits(14.0f));
                    ObjectPropertiesExtendedObserver retry(0, owner);
                    retry.SetFxOffset(CVector(100, 200, 300));
                    Check(Fault::attempts[0x24 / 4] == counts[0x24 / 4] && Fault::attempts[0x28 / 4] == counts[0x28 / 4] + 1 &&
                              Fault::attempts[0x2c / 4] == counts[0x2c / 4] + 1 && retry.GetFxOffset() == CVector(100, 200, 300),
                          "an explicit exact SDK intent resumes only unacknowledged axes after the recorded accepted preimage is restored");
                    retry.RestoreDefault();
                    Check(F::Raw(row) == original, "the resumed vector still restores its independent first original");
                }
                drain();
                {
                    bool modified = false;
                    Check(owner.SetObjectGroupProperty(0, Field::Mass, 10, modified) == ClockResult::Ok &&
                              owner.SetObjectGroupProperty(0, Field::TurnMass, 20, modified) == ClockResult::Ok,
                          "the unchanged first-five semantics prepare a genuine ordered restore prefix");
                    Fault::raceWord = Fault::raceByte = 4;
                    Check(owner.RestoreObjectGroupProperties(0, modified) == ClockResult::OwnerConflict && modified && raw(Field::Mass) == Bits(1.0f) &&
                              runtime.objectGroupProperties.Pending() && !runtime.objectGroupProperties.Uncertain(),
                          "an acknowledged Mass restore remains retained when the next real CAS loses its competing preimage");
                    const auto counts = Fault::attempts;
                    put(Field::Mass, Bits(77.0f));
                    put(Field::TurnMass, Bits(20.0f));
                    Check(owner.RestoreObjectGroupProperties(0, modified) == ClockResult::Ok && !modified && raw(Field::Mass) == Bits(77.0f) &&
                              raw(Field::TurnMass) == Bits(2.0f) && Fault::attempts[0] == counts[0],
                          "classic suffix retry preserves externally changed acknowledged Mass77 and never replays its accepted CAS");
                }
                drain();
                Check(F::Raw(row) == original, "separate bulk cleanup starts its own restore cycle and restores all admitted first originals");
                output = 123;
                Check(owner.ReadObjectGroupProperty(5, Field::UprootLimit, output) != ClockResult::Ok && output == 123,
                      "the known five builtin rows cannot become an invented sixth allocation or full dynamic table extent");
            });
        output = 123;
        ClockResult foreignResult = ClockResult::Ok;
        std::thread outsider([&] { foreignResult = runtime.access.ReadObjectGroupProperty(0, Field::UprootLimit, output); });
        outsider.join();
        Check(foreignResult != ClockResult::Ok && output == 123 && !runtime.objectGroupProperties.Pending(),
              "a foreign thread cannot renew the extended property borrow");
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.objectGroupProperties.CanDiscard(),
              "explicit healthy cleanup permits actual extended-property owner retirement");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        Fault::row = 0;
        writes.Restore();
        std::puts("Production extended object properties healthy completed");
    }
}
