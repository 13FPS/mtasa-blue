/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual builtin scalar SDK/memory/domain/Runtime with separate flags
 ****************************************************************************/
namespace ObjectPropertiesOwnerFixture
{
    namespace P = NativeObjectGroupPropertiesProfile;
    using Field = NativeObjectGroupProperty;
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 3, "one finite builtin scalar-property lifetime is selected");
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
        NativeStreamingLoadAllFixture::Graph     graph(fixture.image, reinterpret_cast<std::uintptr_t>(fixture.frameOwner.data()),
                                                       &WorldGroundOwnerFixture::Writes::Apply, &writes);
        std::array<unsigned char, P::TableBytes> table{};
        table.fill(0xa5);
        for (std::size_t row = 0; row < P::BuiltinRows; ++row)
            for (std::size_t field = 0; field < P::Fields; ++field)
            {
                const auto value = static_cast<float>(field + 1);
                std::memcpy(table.data() + row * P::RowBytes + field * sizeof(float), &value, sizeof(value));
            }
        // The tail deliberately includes an unqualified/noncanonical FX word.
        // Supported SDK methods must neither dereference nor restore it.
        apply(P::TableRva, table.data(), table.size(), PAGE_READWRITE);
        const auto row = fixture.image + P::TableRva;
        const auto raw = [&](std::size_t field)
        {
            std::uint32_t value = 0;
            std::memcpy(&value, reinterpret_cast<void*>(row + field * sizeof(value)), sizeof(value));
            return value;
        };
        const auto tailUnchanged = [&]
        {
            for (std::size_t index = 0; index < P::BuiltinRows; ++index)
                if (std::memcmp(reinterpret_cast<void*>(row + index * P::RowBytes + P::OwnedBytes), table.data() + index * P::RowBytes + P::OwnedBytes,
                                P::RowBytes - P::OwnedBytes))
                    return false;
            return true;
        };
        const auto put = [&](std::size_t field, float value) { std::memcpy(reinterpret_cast<void*>(row + field * sizeof(value)), &value, sizeof(value)); };
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        CameraRowBorrowOwnerFixture::EnginePump engine(fixture);
        writes.Global(NativeStartGameProfile::ThreadIdRva, engine.thread.load());
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        ClockObjectPropertiesOwnerTest::row = row;
        ClockObjectPropertiesOwnerTest::worldMode = graph.World() + NativeStreamingLoadAllProfile::WorldModeOffset;
        ClockObjectPropertiesOwnerTest::failAfterCommitted = ClockObjectPropertiesOwnerTest::failPrefix = false;
        ClockObjectPropertiesOwnerTest::injected = false;
        ClockObjectPropertiesOwnerTest::restoredReads = 0;
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
        Check(began && state.bound && runtime.frameHooked.load(), "scalar properties use the actual existing post-application Runtime");
        const auto generation = state.generation;
        const auto pulse = [&](const auto& action, bool active = true)
        {
            unsigned called = 0;
            callback.action = [&](NativeGameTimingAccess& owner)
            {
                ++called;
                Check(GetCurrentThreadId() == engine.thread.load() && GetCurrentThreadId() != runtime.producerThread,
                      "scalar property work executes on the actual distinct admitted engine callback");
                action(owner);
            };
            engine.Pulse(service, generation);
            callback.action = [](NativeGameTimingAccess&) {};
            Check(called == 1 && (!active || !runtime.stopping.load()), "one requested property action consumes one current callback");
        };
        float output = 123;
        Check(runtime.access.ReadObjectGroupProperty(0, Field::Mass, output) != ClockResult::Ok && output == 123 && !runtime.objectGroupProperties.Pending(),
              "producer refusal preserves scalar output and cannot capture a default");
        pulse(
            [&](NativeGameTimingAccess& owner)
            {
                Check(owner.ReadObjectGroupProperty(0, Field::Mass, output) == ClockResult::Ok && output == 1 && !runtime.objectGroupProperties.Pending(),
                      "real native memory reads an explicitly initialized builtin property");
                output = 123;
                Check(owner.ReadObjectGroupProperty(5, Field::Mass, output) != ClockResult::Ok && output == 123,
                      "storage for builtin five cannot become a guessed total capacity or sixth row");
                bool flag = false;
                Check(owner.SetObjectGroupProperty(0, Field::Mass, 1.00001f, flag) == ClockResult::Ok && !flag && !runtime.objectGroupProperties.Pending(),
                      "actual MTA epsilon equality skips first-change capture and field effects");
            });
        if (retained || unknown)
        {
            bool modified = false;
            ClockObjectPropertiesOwnerTest::committedMass = Bits(99.0f);
            ClockObjectPropertiesOwnerTest::failAfterCommitted = unknown;
            pulse(
                [&](NativeGameTimingAccess& owner)
                {
                    const auto result = owner.SetObjectGroupProperty(0, Field::Mass, 99.0f, modified);
                    Check((unknown ? result != ClockResult::Ok : result == ClockResult::Ok) && modified && raw(0) == Bits(99.0f) && tailUnchanged() &&
                              runtime.objectGroupProperties.Pending() && runtime.objectGroupProperties.Uncertain() == unknown &&
                              ClockObjectPropertiesOwnerTest::injected == unknown,
                          "real32-bit CAS captures the wrapper flag/original and retains any post-write world uncertainty");
                    if (retained)
                    {
                        Check(owner.RestoreObjectGroupProperties(0, modified) == ClockResult::Ok && !modified && raw(0) == Bits(1.0f) &&
                                  runtime.objectGroupProperties.Pending(),
                              "single restore clears this wrapper flag while the independent first-default registry remains retained");
                    }
                },
                !unknown);
            Check(!service.End(generation), "actual End refuses retained defaults even after a physically clean single restore");
            engine.Pulse(service, generation, true);
            NativeClockService  another;
            ClockServiceStateV1 denied;
            ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
            const bool restarted = another.BeginGameTiming(request, denied, nullptr, nullptr);
            ClockNumericAssociationIssuedTest::admittedImage = 0;
            Check(
                !service.End(generation) && !restarted && runtime.objectGroupProperties.Pending() && tailUnchanged() && raw(0) == Bits(unknown ? 99.0f : 1.0f),
                "stopped-frame cleanup/new Begin cannot erase the registry or automatically replay/restore fields");
            engine.Stop();
            std::puts(unknown ? "Production builtin object properties unknown completed" : "Production builtin object properties retained completed");
            return;
        }
        pulse(
            [&](NativeGameTimingAccess& owner)
            {
                {
                    Properties a(0, owner);
                    Check(a.GetGroup() == 0 && a.GetMass() == 1 && a.GetTurnMass() == 2 && a.GetAirResistance() == 3 && a.GetElasticity() == 4 &&
                              a.GetBuoyancy() == 5,
                          "all five actual SDK getters dispatch through this same current native owner");
                    a.SetMass(10);
                    a.SetTurnMass(20);
                    a.SetAirResistance(30);
                    a.SetElasticity(40);
                    a.SetBuoyancy(50);
                    Properties b(0, owner);
                    a.RestoreDefault();
                    Check(a.GetMass() == 1 && a.GetTurnMass() == 2 && a.GetAirResistance() == 3 && a.GetElasticity() == 4 && a.GetBuoyancy() == 5 &&
                              runtime.objectGroupProperties.Pending(),
                          "first SDK restore restores all admitted first originals but deliberately retains the shared registry");
                    put(1, 55);
                    b.RestoreDefault();
                    Check(raw(1) == Bits(2.0f),
                          "a second wrapper's independent modified flag still forces its own restore after the first wrapper became clean");
                    a.SetAirResistance(30);
                    b.RestoreDefault();
                    Check(raw(2) == Bits(30.0f), "a clean second wrapper does not inherit the first wrapper's later dirty flag");
                    Properties c(0, owner);
                    c.RestoreDefault();
                    Check(raw(2) == Bits(3.0f), "new wrapper initializes its flag from retained-map membership and restores the original first value");
                    a.RestoreDefault();
                    Properties clean(1, owner);
                    clean.SetMass(1.00001f);
                    bool hasOriginal = true;
                    Check(owner.HasOriginalObjectGroupProperties(1, hasOriginal) == ClockResult::Ok && !hasOriginal,
                          "same-value scalar SDK edit never captures a new group's default");
                }
                Check(owner.RestoreAllObjectGroupProperties() == ClockResult::Ok && !runtime.objectGroupProperties.Pending() && tailUnchanged(),
                      "explicit private admitted-field cleanup drains originals without pretending to be CGame's unowned cached-wrapper bulk hook");
                bool modified = false;
                Check(owner.SetObjectGroupProperty(0, Field::Mass, 10, modified) == ClockResult::Ok &&
                          owner.SetObjectGroupProperty(0, Field::TurnMass, 20, modified) == ClockResult::Ok,
                      "a fresh first-original cycle prepares a genuine multi-field restore prefix");
                ClockObjectPropertiesOwnerTest::failPrefix = true;
                ClockObjectPropertiesOwnerTest::injected = false;
                ClockObjectPropertiesOwnerTest::restoredReads = 0;
                Check(owner.RestoreObjectGroupProperties(0, modified) != ClockResult::Ok && modified && ClockObjectPropertiesOwnerTest::injected &&
                          !runtime.objectGroupProperties.Uncertain() && runtime.objectGroupProperties.Pending() && raw(0) == Bits(1.0f) &&
                          raw(1) == Bits(20.0f),
                      "a pre-entry refusal after the first actual field acknowledgement retains the prefix and this wrapper's dirty flag");
                ClockObjectPropertiesOwnerTest::failPrefix = false;
                put(0, 77);
                Check(owner.RestoreObjectGroupProperties(0, modified) == ClockResult::Ok && !modified && raw(0) == Bits(77.0f) && raw(1) == Bits(2.0f),
                      "explicit suffix retry clears the wrapper flag without replaying its acknowledged first field");
                Check(owner.RestoreAllObjectGroupProperties() == ClockResult::Ok && !runtime.objectGroupProperties.Pending() && tailUnchanged(),
                      "a separate explicit private cleanup finishes the retained registry and never copies the unqualified FX/tail bytes");
            });
        float       foreign = 123;
        ClockResult foreignResult = ClockResult::Ok;
        std::thread outsider([&] { foreignResult = runtime.access.ReadObjectGroupProperty(0, Field::Mass, foreign); });
        outsider.join();
        Check(foreignResult != ClockResult::Ok && foreign == 123 && !runtime.objectGroupProperties.Pending(),
              "foreign threads cannot borrow scalar native admission");
        if (!service.End(generation))
            engine.Pulse(service, generation, true);
        Check(service.End(generation) && runtime.objectGroupProperties.CanDiscard(), "explicit private cleanup permits actual owner retirement");
        NativeClockService  next;
        ClockServiceStateV1 nextState;
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool nextBegan = next.BeginGameTiming(request, nextState, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(nextBegan && nextState.generation != generation, "the cleaned registry permits a distinct next native-owner generation");
        engine.Pulse(next, nextState.generation);
        if (!next.End(nextState.generation))
            engine.Pulse(next, nextState.generation, true);
        Check(next.End(nextState.generation), "the next no-effect native owner retires normally");
        engine.Stop();
        ClockReaderTestHost::image = previousImage;
        ClockObjectPropertiesOwnerTest::row = ClockObjectPropertiesOwnerTest::worldMode = 0;
        writes.Restore();
        std::puts("Production builtin object properties healthy completed");
    }
}
