/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual callback-owned numeric garage publication and persistent-byte Stop
 ****************************************************************************/
namespace StartupNumericGaragesRuntimeFixture
{
    namespace G = NativeNamedGarageProfile;
    namespace N = NativeNumericGarageProfile;
    using Writes = WorldGroundOwnerFixture::Writes;

    std::uint32_t PhysicalIndex(std::size_t descriptor) noexcept
    {
        // Deliberately disagree with SDK/captured native indices. Selection
        // must use descriptor identity, including the two sprLAe geometries.
        return 49u - static_cast<std::uint32_t>(descriptor) * 2u;
    }
    std::uintptr_t RowRva(std::size_t descriptor) noexcept
    {
        return G::BankRva + static_cast<std::uintptr_t>(PhysicalIndex(descriptor)) * G::RowBytes;
    }
    void AdmitCountPage(ClockImageFixture& fixture, Writes& writes)
    {
        // Preserve the real writable-image section requirement. The generic
        // sparse clock PE contains this exact count page in an immutable gap.
        // Split only that known page, following the existing named fixture.
        const auto                           oldNt = fixture.Get<IMAGE_NT_HEADERS64>(0x100);
        const auto                           at = std::uintptr_t{0x100} + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + oldNt.FileHeader.SizeOfOptionalHeader;
        std::array<IMAGE_SECTION_HEADER, 96> old{};
        SIZE_T                               copied = 0;
        Check(oldNt.FileHeader.NumberOfSections && oldNt.FileHeader.NumberOfSections < old.size() &&
                  ::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + at), old.data(),
                                      oldNt.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER), &copied) &&
                  copied == oldNt.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER),
              "numeric runtime fixture reads only its bounded owned PE section table");
        const auto                        page = static_cast<DWORD>(G::CountRva & ~std::uintptr_t{0xfff});
        std::vector<IMAGE_SECTION_HEADER> sections;
        for (unsigned index = 0; index != oldNt.FileHeader.NumberOfSections; ++index)
        {
            const auto& section = old[index];
            if (G::CountRva < section.VirtualAddress || G::CountRva - section.VirtualAddress >= section.Misc.VirtualSize ||
                (section.Characteristics & IMAGE_SCN_MEM_WRITE))
            {
                sections.push_back(section);
                continue;
            }
            Check(!section.SizeOfRawData && page >= section.VirtualAddress && page + 0x1000 <= section.VirtualAddress + section.Misc.VirtualSize,
                  "numeric runtime only splits the original zero-filled count-page section");
            if (page != section.VirtualAddress)
            {
                auto before = section;
                before.Misc.VirtualSize = page - section.VirtualAddress;
                sections.push_back(before);
            }
            auto current = section;
            std::memcpy(current.Name, ".ngrc\0\0\0", 8);
            current.VirtualAddress = page;
            current.Misc.VirtualSize = 0x1000;
            current.Characteristics |= IMAGE_SCN_MEM_WRITE;
            sections.push_back(current);
            if (page + 0x1000 != section.VirtualAddress + section.Misc.VirtualSize)
            {
                auto after = section;
                after.VirtualAddress = page + 0x1000;
                after.Misc.VirtualSize = section.VirtualAddress + section.Misc.VirtualSize - after.VirtualAddress;
                sections.push_back(after);
            }
        }
        Check(sections.size() <= old.size(), "numeric count-page section split stays bounded");
        auto nt = oldNt;
        nt.FileHeader.NumberOfSections = static_cast<WORD>(sections.size());
        writes.Global(0x100, nt, PAGE_READONLY);
        Writes::Apply(&writes, at, sections.data(), sections.size() * sizeof(IMAGE_SECTION_HEADER), PAGE_READONLY);
    }
    void Run(ClockImageFixture& fixture)
    {
        Writes writes{fixture, {}};
        AdmitCountPage(fixture, writes);
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
        for (const auto& pin : NativePedPoolProfile::Code)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : G::Code)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeNamedGarageGeometryProfile::Code)
            Writes::Apply(&writes, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                Writes::Apply(&writes, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        Writes::Apply(&writes, G::FoldRva, G::Fold, sizeof(G::Fold), PAGE_READONLY);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph                     graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &Writes::Apply, &writes);
        graph.Reset();
        writes.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        writes.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        writes.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        writes.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        writes.Global(G::CountRva, N::CertifiedPublishedCount);
        for (std::uint32_t index = 0; index != N::CertifiedPublishedCount; ++index)
        {
            std::array<unsigned char, G::RowBytes> empty{};
            Writes::Apply(&writes, G::BankRva + static_cast<std::uintptr_t>(index) * G::RowBytes, empty.data(), empty.size(), PAGE_READWRITE);
        }
        for (std::size_t slot = 0; slot != N::Descriptors.size(); ++slot)
        {
            const auto& descriptor = N::Descriptors[slot];
            Writes::Apply(&writes, RowRva(slot), descriptor.geometry.words.data(), sizeof(descriptor.geometry.words), PAGE_READWRITE);
            Writes::Apply(&writes, RowRva(slot) + G::NameOffset, descriptor.name.data(), descriptor.name.size(), PAGE_READWRITE);
            writes.Global(RowRva(slot) + G::TypeOffset, static_cast<std::uint8_t>(0x80u + slot));
            writes.Global(RowRva(slot) + G::DoorStateOffset, std::uint8_t{0});
            writes.Global(RowRva(slot) + G::DoorStateOffset + 1, std::uint8_t{0xa5});
        }
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        ClockNamedGarageOwnerTest::stores = ClockNamedGarageOwnerTest::action = 0;
        ClockNamedGarageOwnerTest::geometryReadAction = 0;
        ClockNamedGarageOwnerTest::nameAddress = fixture.image + RowRva(1) + G::NameOffset;
        ClockNamedGarageOwnerTest::countAddress = fixture.image + G::CountRva;
        ClockNamedGarageOwnerTest::geometryAddress = fixture.image + RowRva(1);
        const auto sentinel = reinterpret_cast<CGarages*>(std::uintptr_t{0x1234});
        CGarages*  outside = sentinel;
        const auto closed = CheckTimingOwner();
        Check(closed != ClockResult::Ok && runtime.access.RequireStartupNumericGaragesReady() == closed &&
                  runtime.access.GetStartupNumericGarages(outside) == closed && outside == sentinel && !ClockNamedGarageOwnerTest::stores,
              "numeric startup publication cannot escape the actual admitted callback or change a refused output");

        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        CGarages*                         published = nullptr;
        CGarage*                          garage8 = nullptr;
        CGarage*                          garage11 = nullptr;
        unsigned                          calls = 0, phase = 0, storesBeforeFailure = 0;
        auto                              beforeRestore = ClockTestFaults::clockDataWrites.load();
        ClockResult                       failedWrite = ClockResult::Ok;
        // This fixture serializes many independent client operations and
        // unbuffered assertions into one real timer callback. Use the actual
        // public producer heartbeat between phases, as the host normally does;
        // never edit the lease, maximum duration, clock source or owner state.
        // Initialize itself stays one uninterrupted sixteen-intent MTA call.
        const auto heartbeat = [&](const char* boundary)
        {
            const auto now = GetTickCount64();
            const auto refreshed = runtime.refreshed.load();
            const auto age = now >= refreshed ? now - refreshed : 0;
            const auto elapsed = now >= runtime.started ? now - runtime.started : 0;
            const bool renewed = service.Renew(runtime.generation.load());
            std::printf("Numeric runtime phase %s: prior lease age=%llu ms, elapsed=%llu ms, maximum=%u ms, renewed=%u\n", boundary,
                        static_cast<unsigned long long>(age), static_cast<unsigned long long>(elapsed), runtime.maximumDuration,
                        static_cast<unsigned>(renewed));
            Check(renewed, "the real public producer heartbeat renews only a still-fresh current generation");
            Check(CheckTimingOwner() == ClockResult::Ok, "phase heartbeat preserves every real callback and binding check");
        };
        callback.action = [&](NativeGameTimingAccess& access)
        {
            ++calls;
            Check(CheckTimingOwner() == ClockResult::Ok, "numeric fixture entered the real retained timing callback");
            heartbeat("callback entry");
            if (!phase)
            {
                Check(access.RequireStartupNumericGaragesReady() == ClockResult::Ok && !ClockNamedGarageOwnerTest::stores && !runtime.stopping.load(),
                      "all16 current numeric descriptors qualify without a garage write or Stop");
                Check(access.GetStartupNumericGarages(published) == ClockResult::Ok && published == &runtime.startupGarages,
                      "actual NativeAccess publishes the real fixed runtime-owned numeric SDK service");
                CGarages* again = nullptr;
                Check(access.GetStartupNumericGarages(again) == ClockResult::Ok && again == published && !ClockNamedGarageOwnerTest::stores,
                      "repeated current publication borrows one stable service without reallocating wrappers");
                const auto originalAdmission = runtime.admissionGeneration.load();
                runtime.admissionGeneration.store(originalAdmission + 1);
                CGarages* refused = sentinel;
                Check(access.GetStartupNumericGarages(refused) == ClockResult::OwnerConflict && refused == sentinel && !ClockNamedGarageOwnerTest::stores,
                      "a stale generation refuses receiver publication before outputs or effects");
                runtime.admissionGeneration.store(originalAdmission);
                Check(NativeStreamingCompletionContext::TryEnter(), "fixture holds a competing existing completion scope");
                Check(access.RequireStartupNumericGaragesReady() == ClockResult::Busy && !ClockNamedGarageOwnerTest::stores && !runtime.stopping.load(),
                      "readiness contention is an ordinary refusal and never stops the owner");
                NativeStreamingCompletionContext::Leave();
                fixture.Put(G::CountRva, std::uint32_t{49});
                refused = sentinel;
                Check(access.GetStartupNumericGarages(refused) == ClockResult::BindingChanged && refused == sentinel && !ClockNamedGarageOwnerTest::stores,
                      "a changed published set refuses without native-row aliasing or receiver publication");
                fixture.Put(G::CountRva, N::CertifiedPublishedCount);
                heartbeat("before uninterrupted Initialize");
                published->Initialize();
                heartbeat("after uninterrupted Initialize");
                Check(ClockNamedGarageOwnerTest::stores == 16, "the actual MTA Initialize schedule performs exactly16 selected byte intents");
                for (std::size_t slot = 0; slot != N::Descriptors.size(); ++slot)
                    Check(fixture.Get<std::uint8_t>(RowRva(slot) + G::TypeOffset) == 1, "each initialized SDK ID reaches its permuted physical descriptor row");
                garage8 = published->GetGarage(8);
                garage11 = published->GetGarage(11);
                Check(garage8 && garage11 && garage8 != garage11 && published->GetGarage(8) == garage8,
                      "the two same-name sprLAe descriptors own distinct stable borrowed SDK wrappers");
                garage8->SetType(0x71);
                garage11->SetType(0x72);
                Check(fixture.Get<std::uint8_t>(RowRva(1) + G::TypeOffset) == 0x71 && fixture.Get<std::uint8_t>(RowRva(3) + G::TypeOffset) == 0x72 &&
                          garage8->GetType() == 0x71,
                      "numeric SetType and GetType preserve both same-name native identities instead of selecting the first");
                garage8->SetOpen(true);
                Check(garage8->IsOpen() && fixture.Get<std::uint8_t>(RowRva(1) + G::DoorStateOffset) == 3 &&
                          fixture.Get<std::uint8_t>(RowRva(1) + G::DoorStateOffset + 1) == 0xa5,
                      "actual numeric SetOpen and IsOpen use classic state3 and leave the adjacent flag byte alone");
                heartbeat("before independent geometry getters");
                const auto            stores = ClockNamedGarageOwnerTest::stores;
                CVector               position;
                std::array<float, 14> geometry{};
                garage8->GetPosition(position);
                geometry[0] = position.fX;
                geometry[1] = position.fY;
                geometry[2] = position.fZ;
                garage8->GetDirection(geometry[3], geometry[4], geometry[5], geometry[6]);
                garage8->GetSize(geometry[7], geometry[8], geometry[9]);
                garage8->GetBoundingBox(geometry[10], geometry[11], geometry[12], geometry[13]);
                NativeGarageGeometry observed;
                std::memcpy(observed.words.data(), geometry.data(), sizeof(observed.words));
                Check(observed.words == N::Descriptors[1].geometry.words && ClockNamedGarageOwnerTest::stores == stores,
                      "all4 actual numeric SDK geometry getters preserve the same fourteen raw storage words without a write");
                heartbeat("before ordinary geometry refusal");
                NativeGarageGeometry unchanged;
                unchanged.words.fill(0xdeadbeef);
                // The numeric selector uses the shared ImageAccess reader,
                // rather than the named getter's direct RPM seam. Revoke an
                // owned descriptor before this call; native primitive tests
                // separately cover reads changing between their snapshots.
                fixture.Put(RowRva(1) + 7 * sizeof(std::uint32_t), std::uint32_t{0x12345678});
                Check(access.ReadNumericGarageGeometry(8, unchanged) != ClockResult::Ok &&
                          std::all_of(unchanged.words.begin(), unchanged.words.end(), [](std::uint32_t word) { return word == 0xdeadbeef; }) &&
                          ClockNamedGarageOwnerTest::stores == stores && !runtime.stopping.load(),
                      "geometry revocation before publication leaves outputs intact and creates no Stop or write");
                fixture.Put(RowRva(1) + 7 * sizeof(std::uint32_t), N::Descriptors[1].geometry.words[7]);
                bool unmapped = false;
                try
                {
                    published->GetGarage(0);
                }
                catch (const std::runtime_error&)
                {
                    unmapped = true;
                }
                Check(unmapped && !published->GetGarage(MAX_GARAGES) && ClockNamedGarageOwnerTest::stores == stores,
                      "unmapped valid IDs refuse explicitly while out-of-range SDK IDs remain absent");
                Check(!runtime.stopping.load() && runtime.active.load(), "ordinary numeric observations and refusals keep this actual owner active");
                std::puts("Production startup numeric garages actual owner and SDK fixture completed");
            }
            else
            {
                Check(runtime.clock.TrySet(18, 20) == ClockResult::Ok && access.SetTimeScale(2.0f) == ClockResult::Ok,
                      "same callback owns independent displayed clock and timing effects before garage failure");
                heartbeat("before entered garage failure");
                beforeRestore = ClockTestFaults::clockDataWrites.load();
                storesBeforeFailure = ClockNamedGarageOwnerTest::stores;
                ClockNamedGarageOwnerTest::action = 3;
                // MTA stores closed as state2. An entered uncertain write
                // must retain that SDK byte through independent cleanup.
                failedWrite = access.SetNumericGarageOpen(8, false);
                Check(failedWrite == ClockResult::WriteFailure && runtime.stopping.load() && ClockNamedGarageOwnerTest::stores == storesBeforeFailure + 1 &&
                          fixture.Get<std::uint8_t>(RowRva(1) + G::DoorStateOffset) == 2,
                      "the actual numeric NativeAccess stops after one entered uncertain byte effect without replay");
            }
        };
        BeginOwned(service, fixture, true, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        fixture.invokeTimer();
        Check(calls == 1 && !runtime.stopping.load(), "one new focused callback exercised numeric publication and all8 SDK properties");
        outside = sentinel;
        Check(runtime.access.GetStartupNumericGarages(outside) != ClockResult::Ok && outside == sentinel,
              "the published SDK pointer never grants receiver admission outside a natural callback");
        phase = 1;
        Check(service.Renew(runtime.generation.load()), "renew the existing callback before the one faulted numeric byte attempt");
        fixture.invokeTimer();
        callback.action = [](NativeGameTimingAccess&) {};
        ClockNamedGarageOwnerTest::action = 0;
        Check(calls == 2 && failedWrite == ClockResult::WriteFailure, "the isolated runtime scope executes exactly one entered failure case");
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(ClockNamedGarageOwnerTest::stores == storesBeforeFailure + 1 && fixture.Get<std::uint8_t>(RowRva(1) + G::DoorStateOffset) == 2 &&
                  fixture.Get<std::uint8_t>(RowRva(1) + G::TypeOffset) == 0x71 && fixture.Get<std::uint8_t>(RowRva(3) + G::TypeOffset) == 0x72 &&
                  fixture.Get<std::uint8_t>(hoursRva) == 12 && fixture.Get<std::uint8_t>(minutesRva) == 34 &&
                  fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f && ClockTestFaults::clockDataWrites.load() > beforeRestore,
              "normal End restores independent clock/timing but neither replays nor rewinds persistent numeric garage bytes");
        Check(!NativeLifetimeRetained() && !runtime.pedReferences.IsRetained() && runtime.pedModelCreations.empty() && runtime.startupCollisionCopies.empty(),
              "a garage-byte receipt creates no allocation journal, resource lease or new module-retention obligation");
        bool expiredBorrow = false;
        try
        {
            garage8->GetType();
        }
        catch (const std::runtime_error&)
        {
            expiredBorrow = true;
        }
        Check(expiredBorrow && ClockNamedGarageOwnerTest::stores == storesBeforeFailure + 1,
              "the borrowed concrete wrapper refuses after owner retirement without a cached field or implicit write");
        ClockReaderTestHost::image = previousImage;
        ClockNamedGarageOwnerTest::nameAddress = ClockNamedGarageOwnerTest::countAddress = ClockNamedGarageOwnerTest::geometryAddress = 0;
        ClockNamedGarageOwnerTest::geometryReadAction = 0;
        writes.Restore();
        std::puts("Production startup numeric garages entered failure stopped without replay and independent timing restored");
        std::puts("Production startup numeric garages runtime fixture completed");
    }
}
