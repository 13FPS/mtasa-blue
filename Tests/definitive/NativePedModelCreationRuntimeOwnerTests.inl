/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real runtime retention must not prevent unrelated clock restoration
 ****************************************************************************/
namespace PedMetadataLifecycleFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace P = NativePedModelMetadataProfile;
    using Writes = WorldGroundOwnerFixture::Writes;
    constexpr std::uint32_t Model = 17;

    template <typename T>
    void Put(unsigned char* bytes, std::size_t offset, const T& value)
    {
        std::memcpy(bytes + offset, &value, sizeof(value));
    }
    struct DefaultData
    {
        unsigned char* bytes = nullptr;
        DefaultData()
        {
            bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x3000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(bytes != nullptr, "runtime lifecycle owns its collision snapshots without adopting a game allocation");
        }
        ~DefaultData()
        {
            if (bytes)
                VirtualFree(bytes, 0, MEM_RELEASE);
        }
        void Seed(ClockImageFixture& fixture, Writes& writes)
        {
            std::memset(bytes, 0xcd, 0x3000);
            auto* spheres = bytes + 0x1000;
            auto* segments = bytes + 0x2000;
            for (const auto& pin : NativePedPoolProfile::Code)
                Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : NativeModelBankProfile::Code)
                Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : P::Code)
                Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            constexpr std::array<std::uintptr_t, 4> offsets{0, 0x20, 0x40, 0x78};
            constexpr std::array<std::uintptr_t, 4> targets{0x11f5540, 0x11f5530, 0x120c530, 0x11f5520};
            for (std::size_t index = 0; index != offsets.size(); ++index)
                writes.Global(P::VtableRva + offsets[index], fixture.image + targets[index], PAGE_READONLY);
            const std::array<unsigned char, 3> type{0xb0, 0x07, 0xc3};
            Writes::Apply(&writes, targets[1], type.data(), type.size(), PAGE_EXECUTE_READ);
            const std::array<unsigned char, 16> bounds{0x31, 0x32, 0x33, 0x34, 0x41, 0x42, 0x43, 0x44, 0x51, 0x52, 0x53, 0x54, 0x61, 0x62, 0x63, 0x64};
            const std::array<unsigned char, 4>  maxY{0x71, 0x72, 0x73, 0x74};
            Writes::Apply(&writes, P::BoundsLiteralRva, bounds.data(), bounds.size(), PAGE_READONLY);
            Writes::Apply(&writes, P::MaxYLiteralRva, maxY.data(), maxY.size(), PAGE_READONLY);
            std::array<unsigned char, 0x38> outer;
            outer.fill(0x9b);
            std::memcpy(outer.data(), bounds.data(), bounds.size());
            std::memcpy(outer.data() + 0x10, maxY.data(), maxY.size());
            Put(outer.data(), 0x14, std::uint32_t{0x3f733333});
            Put(outer.data(), 0x18, std::uint64_t{0});
            Put(outer.data(), 0x20, std::uint32_t{0});
            Put(outer.data(), 0x24, std::uint32_t{0x3f800000});
            outer[0x28] = 0;
            Put(outer.data(), 0x30, reinterpret_cast<std::uintptr_t>(bytes));
            Writes::Apply(&writes, P::DefaultCollisionRva, outer.data(), outer.size(), PAGE_READWRITE);
            bytes[6] = 0;
            Put(bytes, 8, reinterpret_cast<std::uintptr_t>(spheres));
            Put(bytes, 0x18, reinterpret_cast<std::uintptr_t>(segments));
            constexpr std::array<std::uint32_t, 3> z{0xbe4ccccc, 0x3e4cccd0, 0x3f19999a};
            for (std::size_t index = 0; index != z.size(); ++index)
            {
                auto* row = spheres + index * 0x14;
                Put(row, 0, std::uint64_t{0});
                Put(row, 8, z[index]);
                Put(row, 0xc, std::uint32_t{0x3eb33333});
                Put(row, 0x10, static_cast<std::uint16_t>(0x3e + index * 0x100));
                row[0x12] = 0xff;
            }
            for (std::size_t index = 0; index != 2; ++index)
            {
                auto* row = segments + index * 0x20;
                Put(row, 0, std::uint64_t{0});
                Put(row, 8, std::uint32_t{0});
                Put(row, 0x10, std::uint64_t{0});
                Put(row, 0x18, index ? std::uint32_t{0x3f800000} : std::uint32_t{0xbf800000});
            }
            writes.Global(NativeModelBankProfile::BankRva + Model * 8, std::uintptr_t{0});
        }
    };
    void LoseGcAfterMalloc(Host::State& host) noexcept
    {
        static_cast<Writes*>(host.context)->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
    }
    void Run(ClockImageFixture& fixture, unsigned scenario)
    {
        Check(scenario < 4, "one bounded lifecycle scope selects only its four isolated scenarios");
        Writes     writes{fixture, {}};
        const auto admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph                     graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &Writes::Apply, &writes);
        NativePedClothesFixture::Install(fixture.image, &Writes::Apply, &writes);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &Writes::Apply, &writes);
        Host::State host;
        host.Reset(fixture.image, &Writes::Apply, &writes);
        Host::active = &host;
        host.context = &writes;
        graph.Reset();
        DefaultData defaultData;
        defaultData.Seed(fixture, writes);
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        ClockResult                       created = ClockResult::NotInitialized;
        unsigned                          calls = 0;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            ++calls;
            if (scenario != 1)
            {
                Check(runtime.clock.TrySet(18, 20) == ClockResult::Ok && access.SetTimeScale(2.0f) == ClockResult::Ok,
                      "actual callback mutates independently owned clock and timing before metadata disposition");
            }
            created = access.CreateAndPublishPedModelMetadata(Model);
        };
        BeginOwned(service, fixture, true, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        if (scenario == 2)
            host.mallocResult = ClockResult::OriginalException;
        if (scenario == 3)
            host.afterMalloc = &LoseGcAfterMalloc;
        fixture.invokeTimer();
        callback.action = [&](NativeGameTimingAccess&) { ++calls; };
        Check(host.mallocCalls == 1 && runtime.pedModelCreationRetained.load() && runtime.pedModelCreationGeneration.load() == generation &&
                  runtime.pedModelCreations.size() == 1 && !runtime.pedModelCreationBusy.load(),
              "actual runtime reserves and retains one fresh native creation record before any later teardown decision");
        if (scenario < 2)
        {
            Check(created == ClockResult::Ok && runtime.pedModelCreations.front()->GetStage() == NativePedModelCreation::Stage::Published &&
                      fixture.Get<std::uintptr_t>(NativeModelBankProfile::BankRva + Model * 8) == host.base,
                  "actual runtime dispatch completes manual metadata construction and owned-bank publication before its caller returns");
        }
        else
            Check(created != ClockResult::Ok && runtime.stopping.load(), "entered native allocation refusal stops the runtime without erasing its journal");
        if (scenario == 3)
        {
            Check(!service.End() && service.IsRetained() && !host.freeCalls,
                  "producer End cannot free a proved-unpublished allocation while its native GC owner is closed");
            writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            host.afterMalloc = nullptr;
            fixture.invokeTimer();
            Check(host.freeCalls == 1 && runtime.pedModelCreations.empty() && !runtime.pedModelCreationRetained.load() &&
                      !runtime.pedModelCreationGeneration.load() && fixture.Get<std::uint8_t>(hoursRva) == 12 && fixture.Get<std::uint8_t>(minutesRva) == 34 &&
                      fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f,
                  "actual recovery callback retires acknowledged unpublished storage and restores unrelated clock/timing once GC ownership returns");
            Check(service.End() && !service.IsRetained() && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "known unpublished cleanup allows the actual End and admission-release path to finish");
            Host::active = nullptr;
            ClockReaderTestHost::image = previousImage;
            writes.Restore();
            std::puts("Production ped metadata runtime unpublished cleanup completed");
            return;
        }
        Check(!service.End() && service.IsRetained(), "retained metadata keeps real runtime hooks/module admission across producer End");
        const auto writesBeforeRestore = ClockTestFaults::clockDataWrites.load();
        fixture.invokeTimer();
        Check(!runtime.restored.load() && !runtime.nativeTouched.load() && !runtime.dataRecovery.load() && !runtime.timingDirty.load() &&
                  !runtime.timingRecovery.load() && !runtime.timingCaptured.load() && !runtime.timingGeneration.load() &&
                  runtime.pedModelCreationRetained.load() && fixture.Get<std::uint8_t>(hoursRva) == 12 && fixture.Get<std::uint8_t>(minutesRva) == 34 &&
                  fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f,
              "real restoration acknowledges clock/timing separately while overall metadata restoration stays incomplete");
        if (scenario == 0)
            Check(ClockTestFaults::clockDataWrites.load() > writesBeforeRestore,
                  "write-plus-published case executed the actual clock/timing restoration success branch");
        if (scenario == 1)
            Check(ClockTestFaults::clockDataWrites.load() == writesBeforeRestore,
                  "read-only-plus-published case executed the actual no-clock-write restoration success branch");
        fixture.Put(hoursRva, std::uint8_t{7});
        fixture.Put(minutesRva, std::uint8_t{8});
        fixture.Put(secondsRva, std::uint16_t{9});
        fixture.Put(lastChangeRva, std::uint32_t{19900});
        fixture.Put(nativeTimerRva, std::uint32_t{20000});
        const auto writesAfterRestore = ClockTestFaults::clockDataWrites.load();
        const auto allocations = host.mallocCalls, frees = host.freeCalls;
        fixture.invokeTimer();
        Check(fixture.Get<std::uint8_t>(hoursRva) == 7 && fixture.Get<std::uint8_t>(minutesRva) == 8 && fixture.Get<std::uint16_t>(secondsRva) == 9 &&
                  fixture.Get<std::uint32_t>(lastChangeRva) == 19900 && ClockTestFaults::clockDataWrites.load() == writesAfterRestore &&
                  !runtime.nativeTouched.load(),
              "later natural callbacks observe game clock progress without replaying acknowledged restoration or rewinding time");
        Check(!service.End() && service.IsRetained() && runtime.pedModelCreationRetained.load() && host.mallocCalls == allocations && host.freeCalls == frees &&
                  !frees,
              "published or unknown metadata neither replays allocation nor frees while End remains retained");
        NativeClockService  replacement;
        ClockServiceStateV1 refused;
        Check(!replacement.BeginGameTiming({}, refused) && runtime.generation.load() == generation &&
                  NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "retained metadata blocks replacement Begin without rotating the generation or releasing admission");
        const auto state = State(service);
        Check(!state.restored && state.hooked && state.dataRecoveryPending && state.stopping,
              "actual published service state keeps metadata lifetime visible after unrelated restoration succeeded");
        std::puts(scenario == 0   ? "Production ped metadata runtime write restoration retained without rewind"
                  : scenario == 1 ? "Production ped metadata runtime read-only restoration retained without rewind"
                                  : "Production ped metadata runtime unknown allocation retained without replay");
        std::printf("Passed %u production clock service checks\n", checks.load());
        std::fflush(stdout);
        // These scenarios intentionally end the ordinary owned fixture process
        // with the real retained journal intact. Never reset production state
        // or destroy synthetic allocations to manufacture a successful End.
        std::_Exit(0);
    }
}
