/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Whole original eviction with two actual current-bank lifetimes
 ****************************************************************************/
#include "../../Client/game_de/NativeAnimationHierarchyEvictionProfile.h"
namespace BankEvictionOwnerFixture
{
    namespace Bank = BankDecompressionOwnerFixture;
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    namespace Calls = NativeAnimationHierarchyUncompressCalls;
    namespace Conversion = NativeAnimationHierarchyDecompressProfile;
    namespace Eviction = NativeAnimationHierarchyEvictionProfile;
    namespace Heap = NativeModelReferenceTransactionProfile;
    namespace Old = AnimationHierarchyUncompressOwnerFixture;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Profile = NativeAnimationAssociationProgressProfile;
    unsigned scenario = 0;
    struct AllocationHost : Bank::AllocationHost
    {
        unsigned       nullCall = 0, reuseCall = 0;
        std::uintptr_t reuse = 0;
        std::uintptr_t AllocateAt(std::size_t bytes, std::uintptr_t address)
        {
            if (!address)
                return Own(bytes, false);
            Check(!Live(address) && used < allocations.size(), "reuse request names only the fixture's genuinely freed victim allocation");
            const auto base = reinterpret_cast<std::uintptr_t>(VirtualAlloc(reinterpret_cast<void*>(address), bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(base == address, "native target output deliberately reuses the freed victim input address");
            allocations[used++] = {base, false, true};
            return base;
        }
    };
    AllocationHost*                        active = nullptr;
    std::uintptr_t                         image = 0, hierarchy = 0, victim = 0;
    NativeAnimationHierarchyReferenceToken token;
    unsigned                               calls = 0, executed = 0, mode = 0;
    bool                                   valid = true, nestedBusy = false, gateBusy = false, retained = false;
    std::uintptr_t                         Allocate(std::uintptr_t receiver, std::size_t bytes, std::uint32_t alignment)
    {
        auto& host = *active;
        ++host.mallocs;
        host.valid = host.valid && receiver == host.heap && alignment == 0 && bytes && host.eventCount < host.events.size();
        const auto base = host.mallocs == host.nullCall ? 0 : host.AllocateAt(bytes, host.mallocs == host.reuseCall ? host.reuse : 0);
        if (host.eventCount < host.events.size())
            host.events[host.eventCount++] = {false, receiver, base, bytes, alignment};
        return base;
    }
    void Free(std::uintptr_t receiver, std::uintptr_t base)
    {
        auto& host = *active;
        ++host.frees;
        host.valid = host.valid && receiver == host.heap && host.eventCount < host.events.size();
        if (host.eventCount < host.events.size())
            host.events[host.eventCount++] = {true, receiver, base, 0, 0};
        host.valid = host.Release(base) && host.valid;
    }
    ClockResult Invoke(Calls::Function function, std::uintptr_t requested, std::uint32_t& exception) noexcept
    {
        ++calls;
        valid = valid && reinterpret_cast<std::uintptr_t>(function) == image + Profile::HierarchyTimeRva && requested == hierarchy &&
                CheckApplicationTimingOwner() == ClockResult::Ok;
        nestedBusy = runtime.access.UncompressAnimationHierarchy(token) == ClockResult::Busy;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        retained = runtime.hierarchyUncompress.IsRetained() && NativeLifetimeRetained();
        if (!valid)
            return ClockResult::BindingChanged;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        const auto table = Bytes::Load<std::uintptr_t>(active->heap);
        const auto allocate = Bytes::Load<std::uintptr_t>(table + 0x10), release = Bytes::Load<std::uintptr_t>(table + 0x30);
        DWORD      protection = 0, ignored = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(table), 0x40, PAGE_READWRITE, &protection))
            return ClockResult::WriteFailure;
        // Qualification already succeeded. Only allocator leaves are replaced
        // inside entry; every cache/pack/unpack/postprocess body stays original.
        Bytes::Store(table + 0x10, reinterpret_cast<std::uintptr_t>(&Allocate));
        Bytes::Store(table + 0x30, reinterpret_cast<std::uintptr_t>(&Free));
        const auto result = mta::game_de::AnimationHierarchyUncompressDetail::InvokeOriginal(function, requested, exception);
        Bytes::Store(table + 0x10, allocate);
        Bytes::Store(table + 0x30, release);
        valid = VirtualProtect(reinterpret_cast<void*>(table), 0x40, protection, &ignored) && protection == PAGE_READONLY && valid;
        if (result != ClockResult::Ok)
            return result;
        ++executed;
        if (mode == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 3)
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        const auto targetRows = Bytes::Load<std::uintptr_t>(hierarchy + 8), victimRows = Bytes::Load<std::uintptr_t>(victim + 8);
        if (mode == 4 || mode == 5)
        {
            const auto output = Bytes::Load<std::uintptr_t>((mode == 4 ? victimRows : targetRows) + 0x48);
            Bytes::Store(output, Bytes::Load<std::uint16_t>(output) ^ std::uint16_t{1});
        }
        if (mode == 6 || mode == 7)
            Bytes::Store((mode == 6 ? victimRows : targetRows) + 0x48, std::uintptr_t{0});
        if (mode == 8)
            Bytes::Store(Bytes::Load<std::uintptr_t>(hierarchy + 0x20) + 8, std::uintptr_t{0});
        return valid ? ClockResult::Ok : ClockResult::BindingChanged;
    }
    struct VictimInputs
    {
        AllocationHost&                           host;
        std::uintptr_t                            h, rows;
        bool                                      shared;
        std::array<std::uintptr_t, 2>             pointers{};
        std::array<std::vector<unsigned char>, 2> packed;
        std::array<unsigned char, 160>            beforeRows{};
        std::array<unsigned char, 40>             beforeH{};
        VictimInputs(AllocationHost& host, std::uintptr_t h, bool shared) : host(host), h(h), shared(shared)
        {
            rows = host.Own(168, true) + 8;
            Bytes::Store(rows - 8, std::uint64_t{2});
            pointers[0] = host.Own(shared ? 188 : 128, true);
            pointers[1] = shared ? pointers[0] + 128 : host.Own(60, true);
            const std::int16_t times[2][4] = {{30, 90, 150, 240}, {15, 120, 300, 0}};
            const float        floatTimes[2][4] = {{0.5f, 1.5f, 2.5f, 4.0f}, {0.25f, 2.0f, 5.0f, 0.0f}};
            const std::int16_t quaternions[4][4] = {{4096, 0, 0, 0}, {-4096, 0, 0, 0}, {2048, 2048, 0, 0}, {-2048, -2048, 0, 0}};
            for (unsigned row = 0; row != 2; ++row)
            {
                const unsigned count = row ? 3 : 4, stride = row ? 10 : 16;
                packed[row].resize(count * stride);
                std::memset(reinterpret_cast<void*>(rows + row * 80), 0x6d, 80);
                Bytes::Store(rows + row * 80 + 0x42, static_cast<std::uint16_t>((row ? 1 : 3) | (shared ? 8 : 0)));
                Bytes::Store(rows + row * 80 + 0x44, static_cast<std::uint16_t>(count));
                Bytes::Store(rows + row * 80 + 0x48, pointers[row]);
                for (unsigned key = 0; key != count; ++key)
                    for (unsigned word = 0; word != stride / 2; ++word)
                    {
                        // Expectations are explicit signed WORD values. All
                        // fixture floats are exact binary fractions, so this
                        // oracle does not duplicate the native SSE predictor.
                        const auto value = static_cast<std::int16_t>(word < 4    ? (row ? (word == 3 ? (key ? -4096 : 4096) : 0) : quaternions[key][word])
                                                                     : word == 4 ? times[row][key]
                                                                     : word == 5 ? -32768
                                                                     : word == 6 ? 32767
                                                                                 : 1024);
                        std::memcpy(packed[row].data() + key * stride + word * 2, &value, 2);
                        const float decoded = word < 4 ? static_cast<float>(value) / 4096 : word == 4 ? floatTimes[row][key] : static_cast<float>(value) / 1024;
                        Bytes::Store(pointers[row] + key * stride * 2 + word * 4, decoded);
                    }
            }
            Bytes::Store(h + 8, rows);
            Bytes::Store(h + 0x10, std::uint16_t{2});
            Bytes::Store(h + 0x12, static_cast<unsigned char>(0));
            Bytes::Store(h + 0x13, static_cast<unsigned char>(0));
            Bytes::Store(h + 0x18, std::uint32_t{0x3fa00000});
            std::memcpy(beforeRows.data(), reinterpret_cast<const void*>(rows), beforeRows.size());
            std::memcpy(beforeH.data(), reinterpret_cast<const void*>(h), beforeH.size());
        }
        void CheckAfter() const
        {
            const auto base = Bytes::Load<std::uintptr_t>(rows + 0x48);
            Check(base && host.Live(base), "victim compressed output remains a live native allocation");
            auto expectedRows = beforeRows;
            for (unsigned row = 0; row != 2; ++row)
            {
                const auto pointer = base + (row ? packed[0].size() : 0);
                const auto flags = static_cast<std::uint16_t>((row ? 1 : 3) | 12);
                std::memcpy(expectedRows.data() + row * 80 + 0x42, &flags, 2);
                std::memcpy(expectedRows.data() + row * 80 + 0x48, &pointer, sizeof(pointer));
                Check(!std::memcmp(reinterpret_cast<const void*>(pointer), packed[row].data(), packed[row].size()),
                      "whole original quaternion/time/position pack produces exact victim WORD bytes");
            }
            Check(!std::memcmp(expectedRows.data(), reinterpret_cast<const void*>(rows), expectedRows.size()),
                  "victim complete rows preserve opaque metadata and publish contiguous compressed ownership");
            auto expectedH = beforeH;
            expectedH[0x12] = 1;
            const std::uintptr_t zero = 0;
            std::memcpy(expectedH.data() + 0x20, &zero, sizeof(zero));
            Check(!std::memcmp(expectedH.data(), reinterpret_cast<const void*>(h), expectedH.size()),
                  "victim complete H preserves time/custom fields while compressed and cache-retired");
        }
    };
    struct CacheProof
    {
        std::vector<std::uintptr_t>                addresses;
        std::vector<std::array<unsigned char, 24>> expected;
        CacheProof(const Old::CacheFixture& cache, const Old::FreeCacheFixture& free, std::uintptr_t target)
        {
            addresses = {cache.Head(), cache.Tail(), free.Head(), free.Tail()};
            for (unsigned index = 0; index != cache.count; ++index)
                addresses.push_back(cache.Node(index));
            expected.resize(addresses.size());
            for (unsigned index = 0; index != addresses.size(); ++index)
                std::memcpy(expected[index].data(), reinterpret_cast<const void*>(addresses[index]), 24);
            const auto put = [&](unsigned index, unsigned offset, std::uintptr_t value)
            { std::memcpy(expected[index].data() + offset, &value, sizeof(value)); };
            const auto selected = cache.Node(cache.count - 1);
            // Expected final list order is [old tail, every other old node].
            // This is an independent final graph oracle, not a replay of the
            // original's intermediate active/free store instructions.
            put(0, 16, selected);
            put(1, 8, cache.count == 1 ? selected : cache.Node(cache.count - 2));
            put(3 + cache.count, 0, target);
            put(3 + cache.count, 8, cache.Head());
            put(3 + cache.count, 16, cache.count == 1 ? cache.Tail() : cache.Node(0));
            if (cache.count != 1)
            {
                put(4, 8, selected);
                put(2 + cache.count, 16, cache.Tail());
            }
        }
        void CheckExact() const
        {
            for (unsigned index = 0; index != addresses.size(); ++index)
                Check(!std::memcmp(expected[index].data(), reinterpret_cast<const void*>(addresses[index]), 24),
                      "eviction final active/free graph and all opaque anchor/node words match exactly");
        }
    };
    void CheckTarget(const Bank::Inputs& input, std::uintptr_t selected)
    {
        const auto base = Bytes::Load<std::uintptr_t>(input.rows + 0x48);
        Check(base && input.host.Live(base), "target source-published expanded output remains live");
        auto rows = input.beforeRows;
        for (unsigned row = 0; row != 2; ++row)
        {
            const auto pointer = base + (row ? input.expected[0].size() : 0);
            const auto flags = static_cast<std::uint16_t>((row ? 1 : 3) | 8);
            std::memcpy(rows.data() + row * 80 + 0x42, &flags, 2);
            std::memcpy(rows.data() + row * 80 + 0x48, &pointer, sizeof(pointer));
            Check(!std::memcmp(reinterpret_cast<const void*>(pointer), input.expected[row].data(), input.expected[row].size()),
                  "target whole original decode/postprocessing matches all expected float bits");
        }
        Check(!std::memcmp(rows.data(), reinterpret_cast<const void*>(input.rows), rows.size()),
              "target full rows publish aggregate flags/pointers and preserve every opaque byte");
        auto header = input.beforeH;
        header[0x12] = 0;
        std::memcpy(header.data() + 0x20, &selected, sizeof(selected));
        if (input.postprocess)
        {
            const std::uint32_t duration = 0x40a00001;
            std::memcpy(header.data() + 0x18, &duration, 4);
        }
        Check(!std::memcmp(header.data(), reinterpret_cast<const void*>(input.h), header.size()),
              "target complete H verifies exact time/flags/rebound cache node");
    }
    ApplicationFrameFunction originalFrame = nullptr;
    unsigned                 originals = 0;
    bool                     originalReturned = false;
    void                     Original(void* receiver, float delta)
    {
        ++originals;
        originalReturned = false;
        Check(!timingCallbackGeneration && !timingTimerScope && !frameCallbackGeneration && !frameCallbackReceiver && !frameCallbackSerial &&
                  runtime.access.UncompressAnimationHierarchy(token) != ClockResult::Ok,
              "whole original application call has no borrowed eviction scope");
        originalFrame(receiver, delta);
        originalReturned = true;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        // The real frame root is shared with the typed startup/world graph.
        // Admit frame code first, then preserve that graph's actual receiver.
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        };
        admit(NativeStreamingLoadAllProfile::Code);
        for (const auto& pin : NativeStartupOwnerProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeStartupWorldProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeUObjectMembershipProfile::pins);
        for (const auto& pin : NativeGameStateProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        admit(NativeBorrowedGcProfile::pins);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &saved);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::active = &host;
        admit(NativePedPoolProfile::Code);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        admit(NativeAnimationBlockProfile::Code);
        saved.Global(NativeAnimationBlockProfile::RemapRva + NativeAnimationBlockProfile::LoadedOpcodeIndex,
                     static_cast<unsigned char>(NativeAnimationBlockProfile::LoadedSelector), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::DispatchRva + NativeAnimationBlockProfile::LoadedSelector * 4,
                     static_cast<std::uint32_t>(NativeAnimationBlockProfile::LoadedBranchRva), PAGE_EXECUTE_READ);
        saved.Global(NativeAnimationBlockProfile::GroupSlotRva, fixture.image + NativeAnimationBlockProfile::GroupRva, PAGE_READONLY);
        admit(NativeAnimationBlockRefProfile::Code);
        admit(NativeAnimationGroupArrayProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::Code);
        admit(NativeAnimationStaticAssociationProfile::ReadCode);
        admit(NativeAnimationAssociationProfile::Code);
        admit(NativeAnimationHierarchyReadProfile::Code);
        // Real private allocation/init/read/retirement shares the existing
        // manager and cache0 producer proof; this is not a fake H facade.
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        admit(NativeAnimationSequenceRowProfile::Code);
        admit(NativeAnimationSequenceFrameBindProfile::Code);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Profile::HierarchyTimeRva, Profile::HierarchyTimeBytes.data(), Profile::HierarchyTimeBytes.size(),
                                               PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0x4bc80ac, Profile::TimeScaleBytes.data(), Profile::TimeScaleBytes.size(), PAGE_READONLY);
        saved.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        PrivateAnimationAssociationOwnerFixture::SourceBank source(fixture, saved);
        WorldGroundOwnerFixture::Writes::Apply(&saved, NativeAnimationHierarchyCacheRemoveProfile::SourceRva,
                                               NativeAnimationHierarchyCacheRemoveProfile::SourceBytes.data(),
                                               NativeAnimationHierarchyCacheRemoveProfile::SourceBytes.size(), PAGE_EXECUTE_READ);

        admit(Conversion::Code);
        admit(Eviction::Code);
        const auto literal = [&](std::uintptr_t rva, const auto& bytes)
        { WorldGroundOwnerFixture::Writes::Apply(&saved, rva, bytes.data(), bytes.size(), PAGE_READONLY); };
        literal(Eviction::QuaternionScaleRva, Eviction::QuaternionScaleBytes);
        literal(Eviction::PositionScaleRva, Eviction::PositionScaleBytes);
        literal(Eviction::TimeScaleRva, Eviction::TimeScaleBytes);
        literal(Eviction::HalfRva, Eviction::HalfBytes);
        admit(NativeModelBankProfile::Code);
        admit(NativeModelLoadedProfile::Code);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Conversion::QuaternionScaleRva, Conversion::QuaternionScaleBytes.data(), 4, PAGE_READONLY);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Conversion::PositionScaleRva, Conversion::PositionScaleBytes.data(), 4, PAGE_READONLY);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Conversion::TimeScaleRva, Conversion::TimeScaleBytes.data(), 4, PAGE_READONLY);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Conversion::SignMask.rva, Conversion::SignMask.bytes, Conversion::SignMask.size, PAGE_READONLY);
        const auto streamRva = NativeModelLoadedProfile::ArrayRva + NativeStreamingResourceProfile::IfpFirst * NativeModelLoadedProfile::RowStride;
        std::array<unsigned char, 16> stream{};
        stream[NativeModelLoadedProfile::FlagsOffset] = 0x96;
        stream[NativeModelLoadedProfile::StateOffset] = 1;
        WorldGroundOwnerFixture::Writes::Apply(&saved, streamRva, stream.data(), stream.size(), PAGE_READWRITE);
        graph.Reset();
        fixture.Seed();
        fixture.SetFrameReceiver(root.data());
        saved.Global(NativeApplicationFrameProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto priorImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        image = fixture.image;
        hierarchy = source.hierarchy;
        calls = executed = mode = originals = 0;
        valid = true;
        ClockHierarchyUncompressOwnerTest::bankInvoke = &Invoke;
        WorldGroundOwnerFixture::Callback callback;
        callback.action = [](NativeGameTimingAccess&) {};
        NativeClockService    service;
        ClockServiceRequestV1 request;
        if (scenario != 2)
            request.reserved[0] = NativeClockService::GameTimingAfterApplicationFrameV1;
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming(request, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound && !rejected.hooked && !rejected.modulePinned,
              "destructive bank owner starts with the genuine shipping wrong-executable refusal");
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming(request, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.generation > rejected.generation && admitted.bound && admitted.modulePinned &&
                  (scenario == 2 ? runtime.timerHooked.load() : runtime.frameHooked.load() && !runtime.timerHooked.load() && !runtime.timerReady.load()),
              "fresh actual public Begin admits the exact native owner mode without synthetic runtime admission");
        const auto generation = admitted.generation;
        if (scenario != 2)
        {
            originalFrame = runtime.originalFrame;
            runtime.originalFrame = &Original;
        }
        unsigned   handled = 0;
        const auto action = [&](const auto& operation)
        {
            const auto before = handled;
            callback.action = [&](NativeGameTimingAccess&)
            {
                ++handled;
                Check(scenario == 2 ||
                          (originalReturned && originals >= handled && CheckApplicationTimingOwner() == ClockResult::Ok && !runtime.timerReady.load()),
                      "same existing timing access is lent only after the actual normal original frame returns");
                operation();
            };
            Check(service.Renew(generation), "renew the actual public bank conversion owner");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            if (scenario == 2)
                fixture.invokeTimer();
            else
                fixture.invokeFrame(root.data(), 0.25f);
            pulse.Stop();
            Check(handled == before + 1 && !pulse.unexpectedFailures.load(), "actual qualified callback executes exactly one requested bank action");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        AnimationManagerUncompressReceiver   manager(runtime.access, runtime.access, runtime.access, generation);
        CAnimManager&                        sdk = manager;
        std::unique_ptr<CAnimBlendHierarchy> facade;
        action(
            [&]
            {
                Check(runtime.access.ResolveBankAnimationHierarchyAt(generation, 0, token) == ClockResult::Ok && token.locator == hierarchy,
                      "actual manager bank origin resolves this current H without private frame-storage adoption");
                facade = sdk.GetAnimation(0);
                Check(facade && reinterpret_cast<std::uintptr_t>(facade->GetInterface()) == hierarchy,
                      "actual SDK factory publishes the current native-bank facade");
            });
        const auto refuses = [&](const auto& operation, const char* why)
        {
            bool refused = false;
            try
            {
                operation();
            }
            catch (const std::exception&)
            {
                refused = true;
            }
            Check(refused, why);
        };
        const auto finish = [&]
        {
            if (scenario != 2)
                runtime.originalFrame = originalFrame;
            bool done = service.End();
            for (unsigned attempt = 0; !done && attempt != 3; ++attempt)
            {
                if (scenario == 2)
                    fixture.invokeTimer();
                else
                    fixture.invokeFrame(root.data(), 0.25f);
                done = service.End();
            }
            return done;
        };

        // Both locators must belong to the same completed native bank.
        // The victim has its own hierarchy row and separate sequence cookie.
        const auto victimH = source.hierarchy + NativeAnimationBlockHierarchyProfile::RowBytes;
        NativeAnimationBlockFactoryFixture::SeedHierarchy(image, &WorldGroundOwnerFixture::Writes::Apply, &saved, 1, 0);
        saved.Global(NativeAnimationBlockProfile::BankRva + NativeAnimationBlockProfile::AnimationCountOffset, std::int32_t{2});
        saved.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{2});
        victim = victimH;
        const auto exercise = [&](unsigned variant, const auto& operation)
        {
            Old::CacheFixture     cache(saved, image, hierarchy, variant & 1 ? 1 : 3, variant & 1 ? 0 : 2);
            Old::FreeCacheFixture free(saved, image, 0);
            AllocationHost        allocations;
            allocations.heap = reinterpret_cast<std::uintptr_t>(names.heap.data());
            active = &allocations;
            const std::uint32_t time = variant % 3 == 0 ? 0x3fa00000u : variant % 3 == 1 ? 0u : 0x80000000u;
            Bank::Inputs        target(allocations, source.sequences, hierarchy, (variant / 3) & 1, time);
            VictimInputs        old(allocations, victimH, (variant / 6) & 1);
            const auto          selected = cache.Node(cache.count - 1);
            Bytes::Store(selected, victimH);
            Bytes::Store(victimH + 0x20, selected);
            std::memcpy(old.beforeH.data(), reinterpret_cast<const void*>(victimH), old.beforeH.size());
            CacheProof graphProof(cache, free, hierarchy);
            operation(cache, free, allocations, target, old, graphProof);
            active = nullptr;
        };
        if (scenario == 0)
        {
            for (unsigned variant = 0; variant != 12; ++variant)
                exercise(variant,
                         [&](const auto& cache, const auto&, auto& allocations, const auto& target, const auto& old, const auto& proof)
                         {
                             allocations.reuse = old.pointers[0];
                             allocations.reuseCall = 2;
                             const auto before = calls, beforeExecuted = executed;
                             const auto sse = _mm_getcsr() & 0xffc0u;
                             action([&] { sdk.UncompressAnimation(facade.get()); });
                             old.CheckAfter();
                             CheckTarget(target, cache.Node(cache.count - 1));
                             proof.CheckExact();
                             const auto& receipt = runtime.hierarchyUncompressReceipt;
                             Check(calls == before + 1 && executed == beforeExecuted + 1 && valid && nestedBusy && gateBusy && retained &&
                                       receipt.decompressionPath && receipt.entered && receipt.returned && receipt.effectsVerified && !receipt.debt &&
                                       receipt.result == ClockResult::Ok && receipt.convertedFrames == 7 && receipt.outputBytes == 188 &&
                                       receipt.encodedBytes == 94 && receipt.cacheStores == 16 && receipt.cacheNodes == cache.count && receipt.evictionPath &&
                                       receipt.victimHierarchy == victimH && receipt.victimSharedInput == old.shared && receipt.victimSequences == 2 &&
                                       receipt.victimPreparedFrames == 7 && receipt.victimCompressedFrames == 7 && receipt.victimInputBytes == 188 &&
                                       receipt.victimOutputBytes == 94 && receipt.victimOutputBase == Bytes::Load<std::uintptr_t>(old.rows + 0x48) &&
                                       receipt.victimOutputBase != receipt.outputBase && receipt.outputBase == old.pointers[0] &&
                                       !runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load() && (_mm_getcsr() & 0xffc0u) == sse,
                                   "actual SDK eviction verifies two native bank lifetimes and genuine victim-buffer address reuse");
                             const unsigned oldFrees = old.shared ? 1 : 2, targetFrees = target.shared ? 1 : 2;
                             Check(allocations.valid && allocations.mallocs == 2 && allocations.frees == oldFrees + targetFrees &&
                                       allocations.eventCount == 2 + oldFrees + targetFrees,
                                   "exact allocator event count covers both original native conversions");
                             Check(!allocations.events[0].free && allocations.events[0].bytes == 94 && allocations.events[1].free &&
                                       allocations.events[1].address == old.pointers[0] && !allocations.events[oldFrees + 1].free &&
                                       allocations.events[oldFrees + 1].bytes == 188 && allocations.events[oldFrees + 1].address == old.pointers[0] &&
                                       allocations.events[oldFrees + 2].free && allocations.events[oldFrees + 2].address == target.pointers[0],
                                   "victim packs and frees before target allocation reuses that freed address, then target encoded input is freed");
                             if (!old.shared)
                                 Check(allocations.events[2].free && allocations.events[2].address == old.pointers[1],
                                       "individual victim child frees in source order");
                             if (!target.shared)
                                 Check(allocations.events[oldFrees + 3].free && allocations.events[oldFrees + 3].address == target.pointers[1],
                                       "individual target child frees in source order");
                         });
            facade.reset();
            Check(finish() && !service.IsRetained(), "both verified current-bank conversions retire without any adapter native free");
            std::puts("Production bank eviction actual SDK whole original fixture completed");
            std::puts("Production bank eviction both lifetime conversion and freed address reuse fixture completed");
        }
        else if (scenario <= 2)
        {
            exercise(0,
                     [&](const auto& cache, const auto& free, auto& allocations, const auto& target, const auto& old, const auto&)
                     {
                         const auto check = [&](const auto& operation)
                         {
                             const auto                     before = calls;
                             std::array<unsigned char, 160> victimRows{}, targetRows{};
                             std::array<unsigned char, 40>  victimHeader{}, targetHeader{};
                             std::memcpy(victimRows.data(), reinterpret_cast<const void*>(old.rows), victimRows.size());
                             std::memcpy(targetRows.data(), reinterpret_cast<const void*>(target.rows), targetRows.size());
                             std::memcpy(victimHeader.data(), reinterpret_cast<const void*>(victimH), victimHeader.size());
                             std::memcpy(targetHeader.data(), reinterpret_cast<const void*>(hierarchy), targetHeader.size());
                             action([&] { refuses(operation, "unsupported joint eviction state refuses the actual SDK call before original entry"); });
                             Check(calls == before && !allocations.mallocs && !allocations.frees && !runtime.hierarchyUncompress.IsRetained() &&
                                       !runtime.stopping.load() &&
                                       !std::memcmp(victimRows.data(), reinterpret_cast<const void*>(old.rows), victimRows.size()) &&
                                       !std::memcmp(targetRows.data(), reinterpret_cast<const void*>(target.rows), targetRows.size()) &&
                                       !std::memcmp(victimHeader.data(), reinterpret_cast<const void*>(victimH), victimHeader.size()) &&
                                       !std::memcmp(targetHeader.data(), reinterpret_cast<const void*>(hierarchy), targetHeader.size()),
                                   "refused joint command changes neither native hierarchy nor allocations");
                         };
                         const auto command = [&] { sdk.UncompressAnimation(facade.get()); };
                         if (scenario == 2)
                             check(command);
                         else
                         {
                             const auto selected = cache.Node(cache.count - 1);
                             Bytes::Store(selected, hierarchy);
                             check(command);
                             Bytes::Store(selected, victimH);
                             Bytes::Store(selected, std::uintptr_t{0xabc001});
                             check(command);
                             Bytes::Store(selected, victimH);
                             Bytes::Store(victimH + 0x20, cache.Node(0));
                             check(command);
                             Bytes::Store(victimH + 0x20, selected);
                             Bytes::Store(victimH + 0x12, static_cast<unsigned char>(1));
                             check(command);
                             Bytes::Store(victimH + 0x12, static_cast<unsigned char>(0));
                             Bytes::Store(victimH + 0x13, static_cast<unsigned char>(1));
                             check(command);
                             Bytes::Store(victimH + 0x13, static_cast<unsigned char>(0));
                             saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + NativeAnimationBlockHierarchyProfile::RowBytes +
                                              NativeAnimationBlockHierarchyProfile::OwningBlockOffset,
                                          std::uint32_t{1});
                             check(command);
                             saved.Global(NativeAnimationBlockHierarchyProfile::BankRva + NativeAnimationBlockHierarchyProfile::RowBytes +
                                              NativeAnimationBlockHierarchyProfile::OwningBlockOffset,
                                          std::uint32_t{0});
                             Bytes::Store(old.rows + 0x42, std::uint16_t{7});
                             check(command);
                             Bytes::Store(old.rows + 0x42, std::uint16_t{3});
                             Bytes::Store(old.rows + 80 + 0x42, std::uint16_t{9});
                             check(command);
                             Bytes::Store(old.rows + 80 + 0x42, std::uint16_t{1});
                             Bytes::Store(old.rows - 8, std::uint64_t{1});
                             check(command);
                             Bytes::Store(old.rows - 8, std::uint64_t{2});
                             const auto pointer = Bytes::Load<std::uintptr_t>(old.rows + 0x48);
                             Bytes::Store(old.rows + 0x48, target.pointers[0]);
                             check(command);
                             Bytes::Store(old.rows + 0x48, pointer);
                             saved.Global(streamRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(0));
                             check(command);
                             saved.Global(streamRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(1));
                             const auto pin = Eviction::Code[0];
                             saved.Global(pin.rva + pin.size - 1, static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1), PAGE_EXECUTE_READ);
                             check(command);
                             saved.Global(pin.rva + pin.size - 1, pin.bytes[pin.size - 1], PAGE_EXECUTE_READ);
                             Bytes::Store(cache.Head() + 16, cache.Tail());
                             Bytes::Store(cache.Tail() + 8, cache.Head());
                             check(command);
                             Bytes::Store(cache.Head() + 16, cache.Node(0));
                             Bytes::Store(cache.Tail() + 8, selected);
                             Bytes::Store(free.Tail() + 8, selected);
                             check(command);
                             Bytes::Store(free.Tail() + 8, free.Head());
                             refuses(command, "a producer outside the actual frame callback cannot borrow either bank lifetime");
                         }
                     });
            facade.reset();
            Check(finish() && !service.IsRetained() && !calls, "joint pre-entry refusals cleanly retire without heap entry");
            std::puts(scenario == 2 ? "Production bank eviction default timer refusal fixture completed"
                                    : "Production bank eviction coupled current bank refusal fixture completed");
        }
        else
        {
            exercise(
                0,
                [&](const auto& cache, const auto&, auto& allocations, const auto& target, const auto& old, const auto&)
                {
                    mode = scenario == 3    ? 1
                           : scenario == 4  ? 2
                           : scenario == 5  ? 3
                           : scenario == 6  ? 4
                           : scenario == 7  ? 5
                           : scenario == 10 ? 6
                           : scenario == 11 ? 7
                           : scenario == 12 ? 8
                                            : 0;
                    if (scenario == 8)
                    {
                        allocations.nullCall = 1;
                        Bytes::Store(victimH + 0x10, std::uint16_t{1});
                    }
                    if (scenario == 9)
                    {
                        allocations.nullCall = 2;
                        Bytes::Store(hierarchy + 0x10, std::uint16_t{1});
                    }
                    action([&]
                           { refuses([&] { sdk.UncompressAnimation(facade.get()); }, "entered joint uncertainty never becomes successful SDK completion"); });
                    const auto receipt = runtime.hierarchyUncompressReceipt;
                    Check(calls == 1 && executed == (scenario == 3 ? 0u : 1u) && receipt.entered && receipt.decompressionPath && !receipt.effectsVerified &&
                              receipt.debt && runtime.hierarchyUncompress.RecoveryPending() && NativeLifetimeRetained() && runtime.stopping.load() &&
                              allocations.valid,
                          "entered joint uncertainty retains both hierarchy plans, cache effects and module fence");
                    if (scenario == 5)
                        Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{0});
                    if (scenario == 8 || scenario == 9)
                        Check(allocations.mallocs == 3,
                              "real positive single-row fallback executes through the whole original but is not accepted as aggregate success");
                    std::array<unsigned char, 160> victimRows{}, targetRows{};
                    std::array<unsigned char, 40>  victimHeader{}, targetHeader{};
                    std::array<unsigned char, 24>  selected{};
                    std::memcpy(victimRows.data(), reinterpret_cast<const void*>(old.rows), victimRows.size());
                    std::memcpy(targetRows.data(), reinterpret_cast<const void*>(target.rows), targetRows.size());
                    std::memcpy(victimHeader.data(), reinterpret_cast<const void*>(victimH), victimHeader.size());
                    std::memcpy(targetHeader.data(), reinterpret_cast<const void*>(hierarchy), targetHeader.size());
                    std::memcpy(selected.data(), reinterpret_cast<const void*>(cache.Node(cache.count - 1)), selected.size());
                    const auto mallocs = allocations.mallocs, frees = allocations.frees;
                    refuses([&] { sdk.UncompressAnimation(facade.get()); }, "retained facade cannot replay a partially acknowledged joint eviction");
                    facade.reset();
                    Check(!finish() && service.IsRetained() && calls == 1 && allocations.mallocs == mallocs && allocations.frees == frees,
                          "End/natural frames retain uncertain native effects without replay/repair/allocation/free");
                    Check(!std::memcmp(victimRows.data(), reinterpret_cast<const void*>(old.rows), victimRows.size()) &&
                              !std::memcmp(targetRows.data(), reinterpret_cast<const void*>(target.rows), targetRows.size()) &&
                              !std::memcmp(victimHeader.data(), reinterpret_cast<const void*>(victimH), victimHeader.size()) &&
                              !std::memcmp(targetHeader.data(), reinterpret_cast<const void*>(hierarchy), targetHeader.size()) &&
                              !std::memcmp(selected.data(), reinterpret_cast<const void*>(cache.Node(cache.count - 1)), selected.size()),
                          "retained joint journal preserves observable effects without rereading either freed input");
                    const char* names[] = {"before",           "after",           "owner",           "victim-mismatch",
                                           "target-mismatch",  "victim-fallback", "target-fallback", "victim-no-output",
                                           "target-no-output", "cache-mismatch"};
                    std::printf("Production bank eviction %s retained debt fixture completed\n", names[scenario - 3]);
                });
        }
        active = nullptr;
        ClockHierarchyUncompressOwnerTest::bankInvoke = nullptr;
        image = hierarchy = victim = 0;
        originalFrame = nullptr;
        ClockReaderTestHost::image = priorImage;
        Host::active = nullptr;
        saved.Restore();
    }
}
