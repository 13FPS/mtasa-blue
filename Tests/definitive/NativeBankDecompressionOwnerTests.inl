/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Whole original bank decompression under actual post-frame ownership
 ****************************************************************************/
namespace BankDecompressionOwnerFixture
{
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    namespace Calls = NativeAnimationHierarchyUncompressCalls;
    namespace Conversion = NativeAnimationHierarchyDecompressProfile;
    namespace Heap = NativeModelReferenceTransactionProfile;
    namespace Old = AnimationHierarchyUncompressOwnerFixture;
    unsigned scenario = 0;

    struct AllocationHost
    {
        struct Allocation
        {
            std::uintptr_t base = 0;
            bool           input = false, live = false;
        };
        struct Event
        {
            bool           free = false;
            std::uintptr_t receiver = 0, address = 0;
            std::size_t    bytes = 0;
            std::uint32_t  alignment = 0;
        };
        std::array<Allocation, 32> allocations{};
        std::array<Event, 32>      events{};
        unsigned                   used = 0, eventCount = 0, mallocs = 0, frees = 0;
        std::uintptr_t             heap = 0;
        bool                       valid = true, nullAggregate = false;
        std::uintptr_t             Own(std::size_t bytes, bool input)
        {
            Check(used < allocations.size(), "bounded test allocation registry");
            const auto base = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(base != 0, "fixture owns its encoded/output backing allocation");
            allocations[used++] = {base, input, true};
            return base;
        }
        bool Live(std::uintptr_t base) const
        {
            for (const auto& allocation : allocations)
                if (allocation.base == base && allocation.live)
                    return true;
            return false;
        }
        bool Release(std::uintptr_t base)
        {
            for (auto& allocation : allocations)
                if (allocation.base == base && allocation.live)
                {
                    if (!VirtualFree(reinterpret_cast<void*>(base), 0, MEM_RELEASE))
                        return false;
                    allocation.live = false;
                    return true;
                }
            return false;
        }
        ~AllocationHost()
        {
            // Only the synthetic host's still-live allocations are disposed.
            // Engine frees already consumed encoded bases; debt is not repaired.
            for (auto& allocation : allocations)
                if (allocation.live)
                    Release(allocation.base);
        }
    };
    AllocationHost*                        active = nullptr;
    std::uintptr_t                         image = 0, hierarchy = 0;
    NativeAnimationHierarchyReferenceToken token;
    unsigned                               calls = 0, executed = 0, mode = 0;
    bool                                   valid = true, nestedBusy = false, gateBusy = false, retained = false;

    std::uintptr_t Allocate(std::uintptr_t heap, std::size_t bytes, std::uint32_t alignment)
    {
        auto& host = *active;
        ++host.mallocs;
        host.valid = host.valid && heap == host.heap && alignment == 0 && bytes && host.eventCount < host.events.size();
        const auto base = host.nullAggregate && host.mallocs == 1 ? 0 : host.Own(bytes, false);
        if (host.eventCount < host.events.size())
            host.events[host.eventCount++] = {false, heap, base, bytes, alignment};
        return base;
    }
    void Free(std::uintptr_t heap, std::uintptr_t base)
    {
        auto& host = *active;
        ++host.frees;
        host.valid = host.valid && heap == host.heap && host.eventCount < host.events.size();
        if (host.eventCount < host.events.size())
            host.events[host.eventCount++] = {true, heap, base, 0, 0};
        host.valid = host.Release(base) && host.valid;
    }

    ClockResult Invoke(Calls::Function function, std::uintptr_t requested, std::uint32_t& exception) noexcept
    {
        ++calls;
        valid = valid && reinterpret_cast<std::uintptr_t>(function) == image + NativeAnimationAssociationProgressProfile::HierarchyTimeRva &&
                requested == hierarchy && CheckApplicationTimingOwner() == ClockResult::Ok;
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
        // This sole leaf substitution happens after production preflight. All
        // FF0D10/FEC7D0/FED6E0 and optional postprocessors remain exact originals.
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
        const auto rows = Bytes::Load<std::uintptr_t>(requested + 8);
        if (mode == 4)
        {
            const auto output = Bytes::Load<std::uintptr_t>(rows + 0x48);
            Bytes::Store(output, Bytes::Load<std::uint32_t>(output) ^ 1u);
        }
        if (mode == 6)
            Bytes::Store(rows + 0x48, std::uintptr_t{0});
        return valid ? ClockResult::Ok : ClockResult::BindingChanged;
    }

    struct Inputs
    {
        AllocationHost&                           host;
        std::uintptr_t                            rows, h;
        bool                                      shared, postprocess;
        std::array<std::uintptr_t, 2>             pointers{};
        std::array<std::vector<unsigned char>, 2> expected;
        std::array<unsigned char, 160>            beforeRows{};
        std::array<unsigned char, 40>             beforeH{};
        Inputs(AllocationHost& host, std::uintptr_t rows, std::uintptr_t h, bool shared, std::uint32_t time)
            : host(host), rows(rows), h(h), shared(shared), postprocess((time & 0x7fffffff) == 0)
        {
            pointers[0] = host.Own(shared ? 94 : 64, true);
            pointers[1] = shared ? pointers[0] + 64 : host.Own(30, true);
            const std::int16_t times[2][4] = {{30, 90, 150, 240}, {15, 120, 300, 0}};
            const std::int16_t quaternions[4][4] = {{4096, 0, 0, 0}, {-4096, 0, 0, 0}, {2048, 2048, 0, 0}, {-2048, -2048, 0, 0}};
            for (unsigned row = 0; row != 2; ++row)
            {
                const unsigned count = row ? 3 : 4, inputStride = row ? 10 : 16, outputStride = inputStride * 2;
                expected[row].resize(count * outputStride);
                std::memset(reinterpret_cast<void*>(rows + row * 80), 0x3c, 80);
                Bytes::Store(rows + row * 80 + 0x42, static_cast<std::uint16_t>((row ? 5 : 7) | (shared ? 8 : 0)));
                Bytes::Store(rows + row * 80 + 0x44, static_cast<std::uint16_t>(count));
                Bytes::Store(rows + row * 80 + 0x48, pointers[row]);
                for (unsigned key = 0; key != count; ++key)
                {
                    for (unsigned word = 0; word != inputStride / 2; ++word)
                    {
                        const auto value = static_cast<std::int16_t>(word < 4    ? (row ? (word == 3 ? (key ? -4096 : 4096) : 0) : quaternions[key][word])
                                                                     : word == 4 ? times[row][key]
                                                                     : word == 5 ? -32768
                                                                     : word == 6 ? 32767
                                                                                 : 1024);
                        Bytes::Store(pointers[row] + key * inputStride + word * 2, value);
                        // Fixture values have exact binary quaternion/position
                        // scales; time uses the original scalar SSE multiply.
                        const float   scale = word < 4 ? 1.0f / 4096 : word == 4 ? 1.0f / 60 : 1.0f / 1024;
                        float         converted = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(static_cast<float>(value)), _mm_set_ss(scale)));
                        std::uint32_t bits = 0;
                        std::memcpy(&bits, &converted, 4);
                        // These known alternating hemispheres must flip every
                        // quaternion lane, including the sign of zero lanes.
                        if (postprocess && word < 4 && (row ? key != 0 : key == 1 || key == 3))
                            bits ^= 0x80000000u;
                        std::memcpy(expected[row].data() + key * outputStride + word * 4, &bits, 4);
                    }
                }
                if (postprocess)
                    for (unsigned key = count - 1; key; --key)
                    {
                        float current = 0, previous = 0;
                        std::memcpy(&current, expected[row].data() + key * outputStride + 16, 4);
                        std::memcpy(&previous, expected[row].data() + (key - 1) * outputStride + 16, 4);
                        const auto difference = _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(current), _mm_set_ss(previous)));
                        std::memcpy(expected[row].data() + key * outputStride + 16, &difference, 4);
                    }
            }
            Bytes::Store(h + 8, rows);
            Bytes::Store(h + 0x10, std::uint16_t{2});
            Bytes::Store(h + 0x12, static_cast<unsigned char>(1));
            Bytes::Store(h + 0x13, static_cast<unsigned char>(0));
            Bytes::Store(h + 0x18, time);
            Bytes::Store(h + 0x20, std::uintptr_t{0});
            std::memcpy(beforeRows.data(), reinterpret_cast<const void*>(rows), beforeRows.size());
            std::memcpy(beforeH.data(), reinterpret_cast<const void*>(h), beforeH.size());
        }
        void CheckEffects(const Old::CacheFixture& cache, const Old::FreeCacheFixture& free) const
        {
            const auto base = Bytes::Load<std::uintptr_t>(rows + 0x48);
            Check(base && host.Live(base) && base != pointers[0] && base != pointers[1], "source-published output is the live aggregate allocation");
            std::array<unsigned char, 160> expectedRows = beforeRows, observedRows{};
            for (unsigned row = 0; row != 2; ++row)
            {
                const auto pointer = base + (row ? expected[0].size() : 0);
                const auto flags = static_cast<std::uint16_t>((row ? 5 : 7) | 8) & ~std::uint16_t{4};
                std::memcpy(expectedRows.data() + row * 80 + 0x42, &flags, 2);
                std::memcpy(expectedRows.data() + row * 80 + 0x48, &pointer, sizeof(pointer));
                Check(!std::memcmp(reinterpret_cast<const void*>(pointer), expected[row].data(), expected[row].size()),
                      "whole conversion/postprocess produces every expected float bit including quaternion sign-zero and time deltas");
            }
            std::memcpy(observedRows.data(), reinterpret_cast<const void*>(rows), observedRows.size());
            auto expectedH = beforeH;
            expectedH[0x12] = 0;
            const auto node = free.Node(0);
            std::memcpy(expectedH.data() + 0x20, &node, sizeof(node));
            if (postprocess)
            {
                // 300 * the pinned binary32 1/60 rounds one ULP above5.
                // A decimal5 expectation would conceal native SSE ordering.
                const std::uint32_t duration = 0x40a00001;
                std::memcpy(expectedH.data() + 0x18, &duration, 4);
            }
            std::array<unsigned char, 40> observedH{};
            std::memcpy(observedH.data(), reinterpret_cast<const void*>(h), observedH.size());
            Check(observedRows == expectedRows && observedH == expectedH,
                  "all row metadata/H bytes match exact source flags, pointers, total time and unchanged opaque fields");
            Check(Bytes::Load<std::uintptr_t>(node) == h && Bytes::Load<std::uintptr_t>(cache.Head() + 16) == node &&
                      Bytes::Load<std::uintptr_t>(node + 8) == cache.Head() && Bytes::Load<std::uintptr_t>(node + 16) == cache.Node(0) &&
                      Bytes::Load<std::uintptr_t>(cache.Node(0) + 8) == node && Bytes::Load<std::uintptr_t>(cache.Node(0) + 16) == cache.Tail() &&
                      Bytes::Load<std::uintptr_t>(cache.Tail() + 8) == cache.Node(0) && Bytes::Load<std::uintptr_t>(free.Head() + 16) == free.Node(1) &&
                      Bytes::Load<std::uintptr_t>(free.Node(1) + 8) == free.Head() && Bytes::Load<std::uintptr_t>(free.Node(1) + 16) == free.Tail() &&
                      Bytes::Load<std::uintptr_t>(free.Tail() + 8) == free.Node(1),
                  "source eight cache stores unlink the selected free node and insert it before the prior active order");
            Check(host.valid && host.mallocs == 1 && host.frees == (shared ? 1u : 2u) && host.events[0].bytes == 188 && !host.events[0].free &&
                      host.events[1].free && host.events[1].address == pointers[0] &&
                      (shared || (host.events[2].free && host.events[2].address == pointers[1])) && !host.Live(pointers[0]) &&
                      (shared || !host.Live(pointers[1])),
                  "actual engine malloc precedes the exact individual/shared input free bases without private-storage adoption");
        }
    };
    struct CacheProof
    {
        std::array<std::uintptr_t, 7>                addresses;
        std::array<std::array<unsigned char, 24>, 7> expected{};
        CacheProof(const Old::CacheFixture& cache, const Old::FreeCacheFixture& free, std::uintptr_t h)
            : addresses{cache.Head(), cache.Tail(), free.Head(), free.Tail(), cache.Node(0), free.Node(0), free.Node(1)}
        {
            for (unsigned index = 0; index != addresses.size(); ++index)
                std::memcpy(expected[index].data(), reinterpret_cast<const void*>(addresses[index]), 24);
            const auto put = [&](unsigned row, unsigned offset, std::uintptr_t value) { std::memcpy(expected[row].data() + offset, &value, sizeof(value)); };
            put(0, 16, free.Node(0));
            put(2, 16, free.Node(1));
            put(4, 8, free.Node(0));
            put(5, 0, h);
            put(5, 8, cache.Head());
            put(5, 16, cache.Node(0));
            put(6, 8, free.Head());
        }
        void CheckExact() const
        {
            for (unsigned index = 0; index != addresses.size(); ++index)
                Check(!std::memcmp(expected[index].data(), reinterpret_cast<const void*>(addresses[index]), 24),
                      "all active/free anchors and nodes match source effects while every opaque and unmodified word remains exact");
        }
    };
}

namespace BankDecompressionOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Profile = NativeAnimationAssociationProgressProfile;
    ApplicationFrameFunction originalFrame = nullptr;
    unsigned                 originals = 0;
    bool                     originalReturned = false;
    void                     Original(void* receiver, float delta)
    {
        ++originals;
        originalReturned = false;
        Check(!timingCallbackGeneration && !timingTimerScope && !frameCallbackGeneration && !frameCallbackReceiver && !frameCallbackSerial &&
                  runtime.access.UncompressAnimationHierarchy(token) != ClockResult::Ok,
              "original application work has no borrowed timing/frame access to the destructive bank operation");
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
        if (scenario == 0)
        {
            for (unsigned which = 0; which != 6; ++which)
            {
                Old::CacheFixture     cache(saved, image, hierarchy, 1, 0);
                Old::FreeCacheFixture free(saved, image, 2);
                AllocationHost        allocations;
                allocations.heap = reinterpret_cast<std::uintptr_t>(names.heap.data());
                active = &allocations;
                const std::uint32_t time = which % 3 == 0 ? 0x3fa00000u : which % 3 == 1 ? 0u : 0x80000000u;
                Inputs              inputs(allocations, source.sequences, hierarchy, which >= 3, time);
                CacheProof          cacheProof(cache, free, hierarchy);
                const auto          control = _mm_getcsr() & 0xffc0u;
                const auto          before = calls;
                action([&] { sdk.UncompressAnimation(facade.get()); });
                inputs.CheckEffects(cache, free);
                cacheProof.CheckExact();
                const auto& receipt = runtime.hierarchyUncompressReceipt;
                Check(calls == before + 1 && executed == calls && valid && nestedBusy && gateBusy && retained && receipt.decompressionPath &&
                          receipt.sharedInput == inputs.shared && receipt.entered && receipt.returned && receipt.effectsVerified && !receipt.debt &&
                          receipt.result == ClockResult::Ok && receipt.hierarchy == hierarchy && receipt.generation == generation && receipt.sequences == 2 &&
                          receipt.preparedFrames == 7 && receipt.convertedFrames == 7 && receipt.encodedBytes == 94 && receipt.outputBytes == 188 &&
                          receipt.cacheNodes == 3 && receipt.cacheStores == 8 && receipt.outputBase == Bytes::Load<std::uintptr_t>(source.sequences + 0x48) &&
                          !runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load() && (_mm_getcsr() & 0xffc0u) == control,
                      "actual SDK whole original succeeds once with exact conversion, cache, current heap, busy gate and retained-lifetime evidence");
            }
            facade.reset();
            Check(finish() && !service.IsRetained() && !runtime.timerReady.load(),
                  "verified borrowed-bank operation retires without adopting or freeing engine output");
            std::puts("Production bank decompression actual SDK whole original fixture completed");
            std::puts("Production bank decompression exact individual shared zero signed-zero conversion fixture completed");
        }
        else if (scenario <= 2)
        {
            Old::CacheFixture     cache(saved, image, hierarchy, 1, 0);
            Old::FreeCacheFixture free(saved, image, 2);
            AllocationHost        allocations;
            allocations.heap = reinterpret_cast<std::uintptr_t>(names.heap.data());
            active = &allocations;
            Inputs inputs(allocations, source.sequences, hierarchy, false, 0);
            // Refusals compare only still-live encoded storage. After actual
            // conversion all checks use CPU snapshots or published outputs.
            const auto checkRefusal = [&](const auto& operation)
            {
                const auto                     before = calls;
                std::array<unsigned char, 160> rows{}, afterRows{};
                std::array<unsigned char, 40>  header{}, afterHeader{};
                std::array<unsigned char, 64>  payload{}, afterPayload{};
                std::memcpy(rows.data(), reinterpret_cast<const void*>(source.sequences), rows.size());
                std::memcpy(header.data(), reinterpret_cast<const void*>(hierarchy), header.size());
                std::memcpy(payload.data(), reinterpret_cast<const void*>(inputs.pointers[0]), payload.size());
                action([&] { refuses(operation, "unsupported bank conversion state refuses the actual SDK call before native entry"); });
                std::memcpy(afterRows.data(), reinterpret_cast<const void*>(source.sequences), afterRows.size());
                std::memcpy(afterHeader.data(), reinterpret_cast<const void*>(hierarchy), afterHeader.size());
                std::memcpy(afterPayload.data(), reinterpret_cast<const void*>(inputs.pointers[0]), afterPayload.size());
                Check(calls == before && !allocations.mallocs && !allocations.frees && rows == afterRows && header == afterHeader && payload == afterPayload &&
                          !runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load(),
                      "pre-entry refusal keeps every source byte/live input and owner usable");
            };
            if (scenario == 2)
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
            else
            {
                checkRefusal([&] { sdk.UncompressAnimation(nullptr); });
                saved.Global(streamRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(0));
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
                saved.Global(streamRva + NativeModelLoadedProfile::StateOffset, static_cast<unsigned char>(1));
                saved.Global(NativeAnimationBlockProfile::BankRva + NativeAnimationBlockProfile::LoadedOffset, static_cast<unsigned char>(0));
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
                saved.Global(NativeAnimationBlockProfile::BankRva + NativeAnimationBlockProfile::LoadedOffset, static_cast<unsigned char>(1));
                const auto original = Bytes::Load<std::uintptr_t>(image + Heap::HeapVtableRva + 0x10);
                saved.Global(Heap::HeapVtableRva + 0x10, original + 1, PAGE_READONLY);
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
                saved.Global(Heap::HeapVtableRva + 0x10, original, PAGE_READONLY);
                const auto pin = Conversion::Code[0];
                saved.Global(pin.rva + pin.size - 1, static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1), PAGE_EXECUTE_READ);
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
                saved.Global(pin.rva + pin.size - 1, pin.bytes[pin.size - 1], PAGE_EXECUTE_READ);
                Bytes::Store(free.Node(1) + 8, free.Head());
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
                Bytes::Store(free.Node(1) + 8, free.Node(0));
                Bytes::Store(cache.Node(0) + 16, free.Node(0));
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
                Bytes::Store(cache.Node(0) + 16, cache.Tail());
                Bytes::Store(source.sequences + 80 + 0x42, std::uint16_t{13});
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
                Bytes::Store(source.sequences + 80 + 0x42, std::uint16_t{5});
                Bytes::Store(free.Head() + 16, free.Tail());
                Bytes::Store(free.Tail() + 8, free.Head());
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
                Bytes::Store(free.Head() + 16, free.Node(0));
                Bytes::Store(free.Tail() + 8, free.Node(1));
                Bytes::Store(source.sequences - 8, std::uint64_t{1});
                checkRefusal([&] { sdk.UncompressAnimation(facade.get()); });
                Bytes::Store(source.sequences - 8, std::uint64_t{2});
                checkRefusal(
                    [&]
                    {
                        const auto serial = frameCallbackSerial;
                        --frameCallbackSerial;
                        try
                        {
                            sdk.UncompressAnimation(facade.get());
                        }
                        catch (...)
                        {
                            frameCallbackSerial = serial;
                            throw;
                        }
                        frameCallbackSerial = serial;
                    });
                refuses([&] { sdk.UncompressAnimation(facade.get()); }, "producer thread outside its actual callback has no conversion owner");
            }
            facade.reset();
            Check(finish() && !service.IsRetained() && !calls && !allocations.mallocs && !allocations.frees,
                  "all pre-entry bank refusals retire cleanly without heap entry");
            std::puts(scenario == 2 ? "Production bank decompression default timer phase refusal fixture completed"
                                    : "Production bank decompression actual owner coupled refusals fixture completed");
        }
        else
        {
            Old::CacheFixture     cache(saved, image, hierarchy, 1, 0);
            Old::FreeCacheFixture free(saved, image, 2);
            AllocationHost        allocations;
            allocations.heap = reinterpret_cast<std::uintptr_t>(names.heap.data());
            allocations.nullAggregate = scenario == 7;
            active = &allocations;
            Inputs inputs(allocations, source.sequences, hierarchy, false, 0);
            if (scenario == 7)
            {
                // With aggregate NULL the original advances its cursor from
                // zero; a second row would receive address128. One positive
                // row safely executes the genuine E94810 fallback and lets
                // production refuse its incompatible publication afterward.
                Bytes::Store(hierarchy + 0x10, std::uint16_t{1});
            }
            mode = scenario == 3 ? 1 : scenario == 4 ? 2 : scenario == 5 ? 3 : scenario == 6 ? 4 : scenario == 8 ? 6 : 0;
            action([&] { refuses([&] { sdk.UncompressAnimation(facade.get()); }, "entered uncertain conversion never publishes a successful SDK result"); });
            const auto receipt = runtime.hierarchyUncompressReceipt;
            Check(calls == 1 && executed == (scenario == 3 ? 0u : 1u) && receipt.decompressionPath && receipt.entered && !receipt.effectsVerified &&
                      receipt.debt && runtime.hierarchyUncompress.RecoveryPending() && NativeLifetimeRetained() && runtime.stopping.load() && allocations.valid,
                  "entered failure/owner/effect/fallback uncertainty keeps the actual prepared bank/cache/heap journal and module fence");
            if (scenario == 5)
                Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{0});
            if (scenario == 7)
                Check(allocations.mallocs == 2 && allocations.frees == 1,
                      "actual null aggregate path executes the source row fallback allocation/input free but remains unverified debt");
            std::array<unsigned char, 160> rows{}, afterRows{};
            std::array<unsigned char, 40>  header{}, afterHeader{};
            std::array<unsigned char, 24>  node{}, afterNode{};
            std::memcpy(rows.data(), reinterpret_cast<const void*>(source.sequences), rows.size());
            std::memcpy(header.data(), reinterpret_cast<const void*>(hierarchy), header.size());
            std::memcpy(node.data(), reinterpret_cast<const void*>(free.Node(0)), node.size());
            const auto mallocs = allocations.mallocs, frees = allocations.frees;
            refuses([&] { sdk.UncompressAnimation(facade.get()); }, "retained stale facade cannot replay the entered original");
            facade.reset();
            Check(!finish() && service.IsRetained() && runtime.hierarchyUncompress.RecoveryPending() && calls == 1 && allocations.mallocs == mallocs &&
                      allocations.frees == frees,
                  "End and natural frames retain uncertain engine ownership without replay, guessed free, repair or rollback");
            std::memcpy(afterRows.data(), reinterpret_cast<const void*>(source.sequences), afterRows.size());
            std::memcpy(afterHeader.data(), reinterpret_cast<const void*>(hierarchy), afterHeader.size());
            std::memcpy(afterNode.data(), reinterpret_cast<const void*>(free.Node(0)), afterNode.size());
            Check(rows == afterRows && header == afterHeader && node == afterNode,
                  "retained shutdown preserves observed effects and never rereads freed input storage");
            const char* markers[] = {"before", "after", "owner", "mismatch", "fallback", "no-output"};
            std::printf("Production bank decompression %s retained debt fixture completed\n", markers[scenario - 3]);
        }
        active = nullptr;
        ClockHierarchyUncompressOwnerTest::bankInvoke = nullptr;
        image = hierarchy = 0;
        originalFrame = nullptr;
        ClockReaderTestHost::image = priorImage;
        Host::active = nullptr;
        saved.Restore();
    }
}
