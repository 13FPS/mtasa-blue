/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual manager-to-bank Uncompress entry, effects and retained debt
 ****************************************************************************/
#include <initializer_list>
namespace ClockHierarchyUncompressOwnerTest
{
    namespace Calls = NativeAnimationHierarchyUncompressCalls;
    namespace Profile = NativeAnimationAssociationProgressProfile;
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    std::uintptr_t                         image = 0, hierarchy = 0;
    unsigned                               calls = 0, executed = 0, mode = 0;
    bool                                   targetsValid = true, nestedBusy = false, gateBusy = false, lifetimeBusy = false;
    NativeAnimationHierarchyReferenceToken token;
    ClockResult                            Invoke(Calls::Function function, std::uintptr_t requested, std::uint32_t& exception) noexcept
    {
        ++calls;
        targetsValid = targetsValid && reinterpret_cast<std::uintptr_t>(function) == image + Profile::HierarchyTimeRva && requested == hierarchy;
        nestedBusy = runtime.access.UncompressAnimationHierarchy(token) == ClockResult::Busy;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        lifetimeBusy = runtime.hierarchyUncompress.IsRetained() && NativeLifetimeRetained();
        if (!targetsValid)
            return ClockResult::BindingChanged;
        if (mode == 1)
        {
            // Entry was attempted but its acknowledgement is unavailable.
            // No synthetic payload supplies success for this uncertainty case.
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        // Execute the whole original FF0D10 body in the owned MEM_IMAGE.
        // Its genuine compressed-time branch needs no substituted native leaf.
        function(requested);
        ++executed;
        if (mode == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (mode == 3)
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 5)
        {
            const auto node = Bytes::Load<std::uintptr_t>(requested + 0x20);
            Bytes::Store(node + 8, std::uintptr_t{0});
        }
        if (mode == 4)
        {
            const auto sequences = Bytes::Load<std::uintptr_t>(requested + 8);
            const auto frames = Bytes::Load<std::uintptr_t>(sequences + 0x48);
            Bytes::Store(frames + 8, static_cast<std::uint16_t>(Bytes::Load<std::uint16_t>(frames + 8) ^ 1));
        }
        return ClockResult::Ok;
    }
}

namespace ClockHierarchyCacheRemoveOwnerTest
{
    namespace Calls = NativeAnimationHierarchyCacheRemoveCalls;
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    std::uintptr_t                         image = 0, hierarchy = 0;
    NativeAnimationHierarchyReferenceToken token;
    unsigned                               calls = 0, executed = 0, mode = 0;
    bool                                   targetsValid = true, nestedBusy = false, gateBusy = false, lifetimeBusy = false;
    ClockResult                            Write(const Calls::Store* stores, std::size_t count, std::uint32_t& exception) noexcept
    {
        ++calls;
        targetsValid = targetsValid && count == 8 && stores[6].address == hierarchy + 0x20 && !stores[6].value && !stores[7].value;
        nestedBusy = runtime.access.RemovePrivateAnimationHierarchyFromUncompressedCache(token) == ClockResult::Busy;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        lifetimeBusy = runtime.hierarchyCacheRemove.IsRetained() && NativeLifetimeRetained();
        if (!targetsValid)
            return ClockResult::BindingChanged;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        // Execute the actual shipping store writer; a partial prefix below
        // models lost acknowledgement without publishing a successful command.
        const auto result = mta::game_de::AnimationHierarchyCacheRemoveDetail::WriteStores(stores, mode == 4 ? 3 : count, exception);
        if (result != ClockResult::Ok)
            return result;
        ++executed;
        if (mode == 2 || mode == 4)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        if (mode == 3)
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 5)
            Bytes::Store(stores[0].address, std::uintptr_t{0});
        return ClockResult::Ok;
    }
}

namespace AnimationHierarchyUncompressOwnerFixture
{
    namespace Hook = ClockHierarchyUncompressOwnerTest;
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Profile = NativeAnimationAssociationProgressProfile;
    unsigned scenario = 0;
    struct CacheFixture
    {
        std::uintptr_t                image, hierarchy, nodes = 0;
        unsigned                      count, selected;
        std::array<unsigned char, 40> originalHierarchy{};
        std::array<unsigned char, 48> originalSentinels{};
        std::uintptr_t                Head() const { return image + 0x53c08a0; }
        std::uintptr_t                Tail() const { return image + 0x53c08b8; }
        std::uintptr_t                Node(unsigned index) const { return nodes + index * 24; }
        CacheFixture(WorldGroundOwnerFixture::Writes& saved, std::uintptr_t image, std::uintptr_t hierarchy, unsigned count, unsigned selected)
            : image(image), hierarchy(hierarchy), count(count), selected(selected)
        {
            std::memcpy(originalHierarchy.data(), reinterpret_cast<const void*>(hierarchy), originalHierarchy.size());
            std::memcpy(originalSentinels.data(), reinterpret_cast<const void*>(Head()), originalSentinels.size());
            nodes = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, count * 24, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(nodes != 0, "cache fixture owns a bounded row allocation");
            std::array<unsigned char, 48> sentinels{};
            WorldGroundOwnerFixture::Writes::Apply(&saved, 0x53c08a0, sentinels.data(), sentinels.size(), PAGE_READWRITE);
            Bytes::Store(Head(), std::uintptr_t{0x11112222});
            Bytes::Store(Head() + 8, std::uintptr_t{0x33334444});
            Bytes::Store(Head() + 16, Node(0));
            Bytes::Store(Tail(), std::uintptr_t{0x55556666});
            Bytes::Store(Tail() + 8, Node(count - 1));
            Bytes::Store(Tail() + 16, std::uintptr_t{0x77778888});
            for (unsigned index = 0; index != count; ++index)
            {
                Bytes::Store(Node(index), index == selected ? hierarchy : std::uintptr_t{0xabc000} + index);
                Bytes::Store(Node(index) + 8, index ? Node(index - 1) : Head());
                Bytes::Store(Node(index) + 16, index + 1 == count ? Tail() : Node(index + 1));
            }
            Bytes::Store(hierarchy + 0x12, static_cast<unsigned char>(0));
            Bytes::Store(hierarchy + 0x13, static_cast<unsigned char>(0));
            Bytes::Store(hierarchy + 0x20, Node(selected));
        }
        void CheckMoved() const
        {
            auto       previous = Head();
            auto       current = Bytes::Load<std::uintptr_t>(Head() + 16);
            const auto visit = [&](unsigned index, std::uintptr_t& prior, std::uintptr_t& cursor)
            {
                Check(cursor == Node(index) && Bytes::Load<std::uintptr_t>(cursor + 8) == prior &&
                          Bytes::Load<std::uintptr_t>(cursor) == (index == selected ? hierarchy : std::uintptr_t{0xabc000} + index),
                      "original cache LRU preserves node data and moves the selected row before the remaining source order");
                prior = cursor;
                cursor = Bytes::Load<std::uintptr_t>(cursor + 16);
            };
            visit(selected, previous, current);
            for (unsigned index = 0; index != count; ++index)
                if (index != selected)
                    visit(index, previous, current);
            Check(current == Tail() && Bytes::Load<std::uintptr_t>(Tail() + 8) == previous && Bytes::Load<std::uintptr_t>(Head()) == 0x11112222 &&
                      Bytes::Load<std::uintptr_t>(Head() + 8) == 0x33334444 && Bytes::Load<std::uintptr_t>(Tail()) == 0x55556666 &&
                      Bytes::Load<std::uintptr_t>(Tail() + 16) == 0x77778888,
                  "cache terminal links agree and unrelated sentinel fields remain opaque and unchanged");
        }
        ~CacheFixture()
        {
            // Dispose only this child's synthetic image/data. This is never
            // a native recovery acknowledgement or a cache-node release API.
            std::memcpy(reinterpret_cast<void*>(hierarchy), originalHierarchy.data(), originalHierarchy.size());
            std::memcpy(reinterpret_cast<void*>(Head()), originalSentinels.data(), originalSentinels.size());
            if (nodes)
                VirtualFree(reinterpret_cast<void*>(nodes), 0, MEM_RELEASE);
        }
    };
    struct FreeCacheFixture
    {
        std::uintptr_t                image, nodes = 0;
        unsigned                      count;
        std::array<unsigned char, 48> original{};
        std::uintptr_t                Head() const { return image + 0x53c08d0; }
        std::uintptr_t                Tail() const { return image + 0x53c08e8; }
        std::uintptr_t                Node(unsigned index) const { return nodes + index * 24; }
        FreeCacheFixture(WorldGroundOwnerFixture::Writes& saved, std::uintptr_t image, unsigned count) : image(image), count(count)
        {
            std::memcpy(original.data(), reinterpret_cast<const void*>(Head()), original.size());
            if (count)
            {
                nodes = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, count * 24, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                Check(nodes != 0, "free cache fixture owns its synthetic allocation");
            }
            std::array<unsigned char, 48> sentinels{};
            WorldGroundOwnerFixture::Writes::Apply(&saved, 0x53c08d0, sentinels.data(), sentinels.size(), PAGE_READWRITE);
            Bytes::Store(Head(), std::uintptr_t{0xa1});
            Bytes::Store(Head() + 8, std::uintptr_t{0xb2});
            Bytes::Store(Head() + 16, count ? Node(0) : Tail());
            Bytes::Store(Tail(), std::uintptr_t{0xc3});
            Bytes::Store(Tail() + 8, count ? Node(count - 1) : Head());
            Bytes::Store(Tail() + 16, std::uintptr_t{0xd4});
            for (unsigned index = 0; index != count; ++index)
            {
                Bytes::Store(Node(index), std::uintptr_t{0});
                Bytes::Store(Node(index) + 8, index ? Node(index - 1) : Head());
                Bytes::Store(Node(index) + 16, index + 1 == count ? Tail() : Node(index + 1));
            }
        }
        void CheckRemoved(const CacheFixture& active) const
        {
            auto previous = active.Head();
            auto current = Bytes::Load<std::uintptr_t>(previous + 16);
            for (unsigned index = 0; index != active.count; ++index)
                if (index != active.selected)
                {
                    Check(current == active.Node(index) && Bytes::Load<std::uintptr_t>(current + 8) == previous &&
                              Bytes::Load<std::uintptr_t>(current) == std::uintptr_t{0xabc000} + index,
                          "cache removal preserves remaining active order, backlinks and opaque row data");
                    previous = current;
                    current = Bytes::Load<std::uintptr_t>(current + 16);
                }
            Check(current == active.Tail() && Bytes::Load<std::uintptr_t>(active.Tail() + 8) == previous,
                  "active terminal links agree after first/middle/last/only removal");
            previous = active.Node(active.selected);
            Check(Bytes::Load<std::uintptr_t>(Head() + 16) == previous && Bytes::Load<std::uintptr_t>(previous + 8) == Head() &&
                      !Bytes::Load<std::uintptr_t>(previous) && !Bytes::Load<std::uintptr_t>(active.hierarchy + 0x20),
                  "selected row becomes free head with cleared data and H cache link");
            current = Bytes::Load<std::uintptr_t>(previous + 16);
            for (unsigned index = 0; index != count; ++index)
            {
                Check(current == Node(index) && Bytes::Load<std::uintptr_t>(current + 8) == previous && !Bytes::Load<std::uintptr_t>(current),
                      "existing free rows preserve order, backlinks and data");
                previous = current;
                current = Bytes::Load<std::uintptr_t>(current + 16);
            }
            Check(current == Tail() && Bytes::Load<std::uintptr_t>(Tail() + 8) == previous && Bytes::Load<std::uintptr_t>(Head()) == 0xa1 &&
                      Bytes::Load<std::uintptr_t>(Head() + 8) == 0xb2 && Bytes::Load<std::uintptr_t>(Tail()) == 0xc3 &&
                      Bytes::Load<std::uintptr_t>(Tail() + 16) == 0xd4 && Bytes::Load<std::uintptr_t>(active.Head()) == 0x11112222 &&
                      Bytes::Load<std::uintptr_t>(active.Tail() + 16) == 0x77778888,
                  "both lists preserve unrelated sentinel fields");
        }
        ~FreeCacheFixture()
        {
            // This is disposal of owned synthetic fixture data, after debt
            // assertions. It never acknowledges native recovery or replay.
            std::memcpy(reinterpret_cast<void*>(Head()), original.data(), original.size());
            if (nodes)
                VirtualFree(reinterpret_cast<void*>(nodes), 0, MEM_RELEASE);
        }
    };
    struct FrameStorage
    {
        std::array<std::uintptr_t, 2>                pointers{};
        std::array<std::array<unsigned char, 64>, 2> expected{};
        FrameStorage()
        {
            for (auto& pointer : pointers)
            {
                pointer = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                Check(pointer != 0, "compressed frame fixture owns two disjoint bounded private payloads");
            }
        }
        void Seed(std::uintptr_t sequences, unsigned row, std::initializer_list<std::uint16_t> times, bool root)
        {
            const auto stride = root ? 16u : 10u;
            auto&      contents = expected[row];
            contents.fill(0xa5);
            unsigned      index = 0;
            std::uint16_t previous = 0;
            for (const auto value : times)
            {
                std::memcpy(contents.data() + index * stride + 8, &value, sizeof(value));
                ++index;
            }
            std::memcpy(reinterpret_cast<void*>(pointers[row]), contents.data(), contents.size());
            index = 0;
            for (const auto value : times)
            {
                const auto delta = static_cast<std::uint16_t>(value - previous);
                // The native backward walk leaves frame0 absolute. Preserve
                // every other payload byte so an accidental wide write fails.
                if (index)
                    std::memcpy(contents.data() + index * stride + 8, &delta, sizeof(delta));
                previous = value;
                ++index;
            }
            const auto address = sequences + row * 80;
            std::memset(reinterpret_cast<void*>(address), 0x3c, 80);
            Bytes::Store(address + 0x42, static_cast<std::uint16_t>(root ? 0x07 : 0x05));
            Bytes::Store(address + 0x44, static_cast<std::uint16_t>(times.size()));
            Bytes::Store(address + 0x48, pointers[row]);
        }
        bool Matches() const noexcept
        {
            return !std::memcmp(reinterpret_cast<const void*>(pointers[0]), expected[0].data(), expected[0].size()) &&
                   !std::memcmp(reinterpret_cast<const void*>(pointers[1]), expected[1].data(), expected[1].size());
        }
        ~FrameStorage()
        {
            // Fixture-owned backing is disposed only after all debt facts have
            // been asserted; this is never an acknowledgement of native work.
            for (const auto pointer : pointers)
                if (pointer)
                    VirtualFree(reinterpret_cast<void*>(pointer), 0, MEM_RELEASE);
        }
    };
    void RunPrepared(ClockImageFixture& fixture)
    {
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
        FrameStorage frames;
        const auto   seed = [&](unsigned which)
        {
            if (!which)
            {
                frames.Seed(source.sequences, 0, {30, 90, 15, 300}, true);
                frames.Seed(source.sequences, 1, {5, 32760, 32767}, false);
            }
            else if (which == 1)
            {
                frames.Seed(source.sequences, 0, {65530, 3, 4}, true);
                frames.Seed(source.sequences, 1, {65535}, false);
            }
            else
            {
                frames.Seed(source.sequences, 0, {65535}, true);
                frames.Seed(source.sequences, 1, {300, 65534}, false);
            }
            saved.Global(source.hierarchy - fixture.image + 8, source.sequences);
            saved.Global(source.hierarchy - fixture.image + 0x10, std::uint16_t{2});
            saved.Global(source.hierarchy - fixture.image + 0x12, static_cast<unsigned char>(0x80));
            saved.Global(source.hierarchy - fixture.image + 0x13, static_cast<unsigned char>(1));
            saved.Global(source.hierarchy - fixture.image + 0x18, std::uint32_t{0});
            saved.Global(source.hierarchy - fixture.image + 0x20, std::uintptr_t{0});
        };
        seed(0);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Hook::image = fixture.image;
        Hook::hierarchy = source.hierarchy;
        Hook::calls = Hook::executed = Hook::mode = 0;
        Hook::targetsValid = true;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        ClockServiceStateV1 rejected, admitted;
        Check(!service.BeginGameTiming({}, rejected, &WorldGroundOwnerFixture::Callback::Invoke, &callback) &&
                  rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound && !rejected.hooked &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "shipping public Begin refuses the fixture executable before any explicit owned-image supplier");
        fixture.Seed();
        // A rejected epoch remains closed. Only a fresh actual public Begin
        // may admit this explicitly owned image and install its real hook.
        ClockNumericAssociationIssuedTest::admittedImage = fixture.image;
        const bool began = service.BeginGameTiming({}, admitted, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        ClockNumericAssociationIssuedTest::admittedImage = 0;
        Check(began && admitted.generation > rejected.generation && admitted.bound && admitted.modulePinned &&
                  NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "actual fresh public Begin owns the native Uncompress fixture image, hook and callback epoch");
        const auto generation = runtime.generation.load();
        const auto action = [&](const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&) { body(); };
            Check(service.Renew(generation), "renew the actual Uncompress callback owner");
            StaticAssociationInitializationOwnerFixture::ProducerPulse pulse(service, generation);
            Check(pulse.renewals.load() && !pulse.unexpectedFailures.load(), "Uncompress has a real independent producer before original entry");
            fixture.invokeTimer();
            pulse.Stop();
            Check(!pulse.unexpectedFailures.load() && runtime.producerThread == GetCurrentThreadId(),
                  "Uncompress preserves the public producer and engine callback handoff");
            callback.action = [](NativeGameTimingAccess&) {};
        };
        AnimationManagerUncompressReceiver   manager(runtime.access, runtime.access, runtime.access, generation);
        CAnimManager&                        sdk = manager;
        std::unique_ptr<CAnimBlendHierarchy> hierarchy;
        action(
            [&]
            {
                Check(runtime.access.ResolveBankAnimationHierarchyAt(generation, 0, Hook::token) == ClockResult::Ok && Hook::token.locator == source.hierarchy,
                      "actual runtime resolves the source bank hierarchy through its present block and range");
                hierarchy = sdk.GetAnimation(0);
                Check(hierarchy && reinterpret_cast<std::uintptr_t>(hierarchy->GetInterface()) == source.hierarchy,
                      "actual manager creates the real hierarchy facade from the current bank, without a successful provider fixture");
            });
        const auto refuses = [&](const auto& operation, const char* label)
        {
            bool rejectedOperation = false;
            try
            {
                operation();
            }
            catch (const std::exception&)
            {
                rejectedOperation = true;
            }
            Check(rejectedOperation, label);
        };
        const auto finish = [&]
        {
            bool done = service.End();
            for (unsigned attempt = 0; attempt != 3 && !done; ++attempt)
            {
                fixture.invokeTimer();
                done = service.End();
            }
            return done;
        };
        if (scenario == 0)
        {
            action(
                [&]
                {
                    auto custom = manager.CAnimManagerDE::GetCustomAnimBlendHierarchy();
                    Check(custom != nullptr, "actual MTA custom hierarchy factory owns a genuine private record");
                    custom->Initialize();
                    const auto                    tag = reinterpret_cast<std::uintptr_t>(custom->GetInterface());
                    std::array<unsigned char, 40> before{}, after{};
                    std::memcpy(before.data(), reinterpret_cast<const void*>(tag), before.size());
                    const auto calls = Hook::calls;
                    sdk.UncompressAnimation(custom.get());
                    std::memcpy(after.data(), reinterpret_cast<const void*>(tag), after.size());
                    Check(before == after && Hook::calls == calls && runtime.hierarchyUncompressReceipt.noOp &&
                              runtime.hierarchyUncompressReceipt.effectsVerified && !runtime.hierarchyUncompressReceipt.entered &&
                              !runtime.hierarchyUncompressReceipt.debt && runtime.hierarchyUncompressReceipt.hierarchy == tag &&
                              !runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load(),
                          "real private flags12/13 and cache0 admit the actual MTA no-effect call without bank adoption or native mutation");
                    custom->SetRunningCompressed(true);
                    refuses([&] { sdk.UncompressAnimation(custom.get()); }, "private compressed payload phase is still an explicit refusal");
                    custom->SetRunningCompressed(false);
                    Bytes::Store(tag + 0x13, static_cast<unsigned char>(1));
                    refuses([&] { sdk.UncompressAnimation(custom.get()); }, "KeepCompressed byte13 is observed separately from the SDK byte12 getter");
                    Bytes::Store(tag + 0x13, static_cast<unsigned char>(0));
                    const auto lastByte = Bytes::Load<unsigned char>(fixture.image + Profile::HierarchyTimeRva + Profile::HierarchyTimeBytes.size() - 1);
                    saved.Global(Profile::HierarchyTimeRva + Profile::HierarchyTimeBytes.size() - 1, static_cast<unsigned char>(lastByte ^ 1),
                                 PAGE_EXECUTE_READ);
                    refuses([&] { sdk.UncompressAnimation(custom.get()); }, "private no-effect still requires the exact original source entry");
                    saved.Global(Profile::HierarchyTimeRva + Profile::HierarchyTimeBytes.size() - 1, lastByte, PAGE_EXECUTE_READ);
                    sdk.UncompressAnimation(custom.get());
                    Check(Hook::calls == calls && runtime.hierarchyUncompressReceipt.noOp && runtime.hierarchyUncompressReceipt.effectsVerified,
                          "refused private source phases leave no entered command to replay and the later valid no-op remains available");
                    manager.CAnimManagerDE::DeleteCustomAnimHierarchyInterface(custom->GetInterface());
                    custom.reset();
                    Check(!runtime.privateHierarchies.IsRetained() && !runtime.hierarchyUncompress.IsRetained(),
                          "actual owner retires the private record after CPU-only no-effect calls without leaking bank or conversion debt");
                });
            std::puts("Production private hierarchy Uncompress source no-effect fixture completed");
            for (const auto& spec : std::array<std::array<unsigned, 2>, 4>{{{3, 1}, {3, 2}, {3, 0}, {1, 0}}})
            {
                seed(0);
                CacheFixture                  cache(saved, fixture.image, source.hierarchy, spec[0], spec[1]);
                std::array<unsigned char, 40> before{}, after{};
                std::memcpy(before.data(), reinterpret_cast<const void*>(source.hierarchy), before.size());
                const auto calls = Hook::calls;
                action([&] { sdk.UncompressAnimation(hierarchy.get()); });
                cache.CheckMoved();
                std::memcpy(after.data(), reinterpret_cast<const void*>(source.hierarchy), after.size());
                Check(before == after && Hook::calls == calls + 1 && Hook::targetsValid && Hook::nestedBusy && Hook::gateBusy && Hook::lifetimeBusy &&
                          runtime.hierarchyUncompressReceipt.cachePath && runtime.hierarchyUncompressReceipt.cacheNodes == spec[0] &&
                          runtime.hierarchyUncompressReceipt.modifiedCacheWords == (spec[1] ? 6u : 0u) &&
                          runtime.hierarchyUncompressReceipt.noOp == (spec[1] == 0) && runtime.hierarchyUncompressReceipt.effectsVerified &&
                          !runtime.hierarchyUncompress.IsRetained(),
                      "whole native entry moves existing middle/last cache rows and handles already-first/single aliases without touching H or frames");
            }
            std::puts("Production hierarchy Uncompress existing cache LRU fixture completed");
            float scale = 0;
            std::memcpy(&scale, Profile::TimeScaleBytes.data(), sizeof(scale));
            for (unsigned which = 0; which != 3; ++which)
            {
                seed(which);
                const auto    before = Hook::calls;
                const auto    expectedTime = (which == 0 ? 32767.0f : which == 1 ? 4.0f : 0.0f) * scale;
                std::uint32_t expectedTimeBits = 0;
                std::memcpy(&expectedTimeBits, &expectedTime, sizeof(expectedTimeBits));
                std::array<unsigned char, 160> originalRows{};
                std::memcpy(originalRows.data(), reinterpret_cast<const void*>(source.sequences), originalRows.size());
                action([&] { sdk.UncompressAnimation(hierarchy.get()); });
                const auto& receipt = runtime.hierarchyUncompressReceipt;
                Check(Hook::calls == before + 1 && Hook::executed == Hook::calls && Hook::targetsValid && Hook::nestedBusy && Hook::gateBusy &&
                          Hook::lifetimeBusy && receipt.entered && receipt.returned && receipt.effectsVerified && !receipt.debt &&
                          receipt.result == ClockResult::Ok && receipt.hierarchy == source.hierarchy && receipt.generation == generation &&
                          receipt.entryRva == Profile::HierarchyTimeRva && !receipt.beforeTimeBits && receipt.afterTimeBits == expectedTimeBits &&
                          receipt.sequences == 2 &&
                          receipt.preparedFrames == (which == 0   ? 7u
                                                     : which == 1 ? 4u
                                                                  : 3u) &&
                          receipt.modifiedFrames == (which == 0   ? 5u
                                                     : which == 1 ? 2u
                                                                  : 1u),
                      "actual SDK reaches the same native bank component and one whole original call under its busy gate and retained lifetime");
                Check(frames.Matches() && Bytes::Load<std::uint32_t>(source.hierarchy + 0x18) == expectedTimeBits &&
                          !std::memcmp(originalRows.data(), reinterpret_cast<const void*>(source.sequences), originalRows.size()) &&
                          Bytes::Load<unsigned char>(source.hierarchy + 0x12) == 0x80 && Bytes::Load<unsigned char>(source.hierarchy + 0x13) == 1 &&
                          !runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load(),
                      "whole native compressed conversion preserves flags and row metadata, writes exact modulo WORD deltas and signed-last maximum time");
                if (!which)
                {
                    action([&] { sdk.UncompressAnimation(hierarchy.get()); });
                    Check(Hook::calls == before + 2 && runtime.hierarchyUncompressReceipt.noOp && runtime.hierarchyUncompressReceipt.effectsVerified &&
                              frames.Matches(),
                          "the genuine warm nonzero-time native path performs no second timestamp subtraction");
                }
            }
            seed(0);
            saved.Global(source.hierarchy - fixture.image + 8, std::uintptr_t{1});
            saved.Global(source.hierarchy - fixture.image + 0x10, std::uint16_t{0});
            saved.Global(source.hierarchy - fixture.image + 0x18, std::uint32_t{0x80000000});
            const auto before = Hook::calls;
            action([&] { sdk.UncompressAnimation(hierarchy.get()); });
            Check(Hook::calls == before + 1 && runtime.hierarchyUncompressReceipt.effectsVerified && !runtime.hierarchyUncompressReceipt.sequences &&
                      !runtime.hierarchyUncompressReceipt.preparedFrames && !Bytes::Load<std::uint32_t>(source.hierarchy + 0x18),
                  "native count0 converts negative zero without borrowing its deliberately invalid child pointer");
            saved.Global(source.hierarchy - fixture.image + 0x10, std::uint16_t{0x8000});
            saved.Global(source.hierarchy - fixture.image + 0x18, std::uint32_t{0x80000000});
            action([&] { sdk.UncompressAnimation(hierarchy.get()); });
            Check(Hook::calls == before + 2 && runtime.hierarchyUncompressReceipt.effectsVerified && !Bytes::Load<std::uint32_t>(source.hierarchy + 0x18),
                  "the native signed WORD hierarchy count skips the same child walk for a high-bit count");
            hierarchy.reset();
            Check(finish() && !service.IsRetained() && !host.mallocCalls && !host.freeCalls,
                  "verified bank conversion closes the actual owner without inventing a private allocation or free receipt");
            std::puts("Production hierarchy Uncompress actual SDK original compressed conversion fixture completed");
        }
        else if (scenario == 1)
        {
            refuses([&] { sdk.UncompressAnimation(hierarchy.get()); }, "retained manager and H facade cannot mutate between callbacks");
            action(
                [&]
                {
                    refuses([&] { sdk.UncompressAnimation(nullptr); }, "null hierarchy cannot become native Uncompress authority");
                    animation_hierarchy_fixture::AnimationHierarchyReceiver wrong(runtime.access, {generation - 1, source.hierarchy});
                    refuses([&] { sdk.UncompressAnimation(&wrong); }, "actual manager refuses a wrong-generation hierarchy");
                    NativeAnimationHierarchyReferenceToken missing{};
                    Check(runtime.access.UncompressAnimationHierarchy(missing) != ClockResult::Ok, "missing token cannot enter original conversion");
                    saved.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                    Check(runtime.access.UncompressAnimationHierarchy(Hook::token) != ClockResult::Ok, "borrowed GC marker blocks Uncompress before entry");
                    saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
                    saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() ^ 0x80000000u);
                    Check(runtime.access.UncompressAnimationHierarchy(Hook::token) != ClockResult::Ok, "native GT identity blocks Uncompress before entry");
                    saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
                });
            const auto rejectCurrent = [&](const auto& change, const char* label)
            {
                seed(0);
                change();
                action([&] { refuses([&] { sdk.UncompressAnimation(hierarchy.get()); }, label); });
                Check(!Hook::calls && !runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load(),
                      "pre-entry source refusal creates no native debt or stop condition");
            };
            rejectCurrent([&] { saved.Global(source.hierarchy - fixture.image + 0x13, static_cast<unsigned char>(0)); },
                          "compressed cache miss still requires allocation and decompression authority");
            rejectCurrent([&] { saved.Global(source.hierarchy - fixture.image + 0x10, std::uint16_t{3}); },
                          "actual hierarchy count cannot exceed its current row cookie");
            rejectCurrent([&]
                          { saved.Global(NativeAnimationBlockProfile::BankRva + NativeAnimationBlockProfile::LoadedOffset, static_cast<unsigned char>(0)); },
                          "unloaded bank supplies no compressed payload authority");
            saved.Global(NativeAnimationBlockProfile::BankRva + NativeAnimationBlockProfile::LoadedOffset, static_cast<unsigned char>(1));
            rejectCurrent([&] { Bytes::Store(source.sequences + 0x42, std::uint16_t{3}); },
                          "uncompressed frame representation cannot enter compressed conversion");
            rejectCurrent([&] { Bytes::Store(source.sequences + 0x44, std::uint16_t{0}); }, "native last-frame access refuses an empty sequence row");
            rejectCurrent([&] { Bytes::Store(source.sequences + 0x48, std::uintptr_t{1}); }, "unreadable odd frame pointer supplies no payload provenance");
            rejectCurrent([&] { Bytes::Store(source.sequences + 80 + 0x48, frames.pointers[0]); }, "two rows cannot share timestamp write authority");
            rejectCurrent([&] { saved.Global(0x4bc80ac, std::uint32_t{0x3f800000}, PAGE_READONLY); },
                          "changed native time scale refuses before original entry");
            WorldGroundOwnerFixture::Writes::Apply(&saved, 0x4bc80ac, Profile::TimeScaleBytes.data(), Profile::TimeScaleBytes.size(), PAGE_READONLY);
            rejectCurrent(
                [&]
                {
                    saved.Global(Profile::HierarchyTimeRva + Profile::HierarchyTimeBytes.size() - 1,
                                 static_cast<unsigned char>(Profile::HierarchyTimeBytes.back() ^ 1), PAGE_EXECUTE_READ);
                },
                "changed last byte of the whole saved native entry refuses before execution");
            WorldGroundOwnerFixture::Writes::Apply(&saved, Profile::HierarchyTimeRva, Profile::HierarchyTimeBytes.data(), Profile::HierarchyTimeBytes.size(),
                                                   PAGE_EXECUTE_READ);
            for (unsigned malformed = 0; malformed != 3; ++malformed)
            {
                seed(0);
                CacheFixture cache(saved, fixture.image, source.hierarchy, 3, 1);
                if (malformed == 0)
                    Bytes::Store(cache.Node(1) + 8, cache.Head());
                else if (malformed == 1)
                    Bytes::Store(cache.Node(2) + 16, cache.Node(0));
                else
                    Bytes::Store(cache.Node(1), source.hierarchy + 40);
                action(
                    [&]
                    { refuses([&] { sdk.UncompressAnimation(hierarchy.get()); }, "malformed cache backlink/cycle/H identity refuses before native entry"); });
                Check(!Hook::calls && !runtime.hierarchyUncompress.IsRetained() && !runtime.stopping.load(),
                      "cache shape refusal creates no entered journal or speculative linked-list repair");
            }
            seed(0);
            hierarchy.reset();
            Check(finish() && !service.IsRetained() && !Hook::calls, "all typed pre-entry refusals leave the actual module owner releasable");
            std::puts("Production hierarchy Uncompress actual owner coupled refusal fixture completed");
        }
        else if (scenario >= 9)
        {
            namespace RemoveHook = ClockHierarchyCacheRemoveOwnerTest;
            RemoveHook::image = fixture.image;
            RemoveHook::hierarchy = source.hierarchy;
            RemoveHook::token = Hook::token;
            RemoveHook::calls = RemoveHook::executed = RemoveHook::mode = 0;
            RemoveHook::targetsValid = true;
            if (scenario == 9)
            {
                action([&] { sdk.RemoveFromUncompressedCache(hierarchy.get()); });
                Check(!RemoveHook::calls && runtime.hierarchyCacheRemoveReceipt.noOp && runtime.hierarchyCacheRemoveReceipt.effectsVerified,
                      "actual bank cache0 source operation acknowledges no effect without touching either list");
                for (const auto& spec : {std::array<unsigned, 3>{3, 1, 2}, {3, 2, 0}, {3, 0, 2}, {1, 0, 0}})
                {
                    seed(0);
                    CacheFixture                  cache(saved, fixture.image, source.hierarchy, spec[0], spec[1]);
                    FreeCacheFixture              free(saved, fixture.image, spec[2]);
                    std::array<unsigned char, 40> expected{}, observed{};
                    std::memcpy(expected.data(), reinterpret_cast<const void*>(source.hierarchy), expected.size());
                    std::memset(expected.data() + 0x20, 0, sizeof(std::uintptr_t));
                    const auto before = RemoveHook::calls;
                    action([&] { sdk.RemoveFromUncompressedCache(hierarchy.get()); });
                    free.CheckRemoved(cache);
                    std::memcpy(observed.data(), reinterpret_cast<const void*>(source.hierarchy), observed.size());
                    const auto& receipt = runtime.hierarchyCacheRemoveReceipt;
                    Check(observed == expected && RemoveHook::calls == before + 1 && RemoveHook::targetsValid && RemoveHook::nestedBusy &&
                              RemoveHook::gateBusy && RemoveHook::lifetimeBusy && receipt.entered && receipt.returned && receipt.effectsVerified &&
                              !receipt.noOp && !receipt.debt && receipt.cacheNodes == spec[0] + spec[2] && !runtime.hierarchyCacheRemove.IsRetained(),
                          "actual MTA virtual cache removal executes the shipping eight-store command under the same callback/gate/lifetime");
                }
                for (unsigned malformed = 0; malformed != 3; ++malformed)
                {
                    seed(0);
                    CacheFixture     cache(saved, fixture.image, source.hierarchy, 3, 1);
                    FreeCacheFixture free(saved, fixture.image, 2);
                    if (malformed == 0)
                        Bytes::Store(cache.Node(1) + 8, cache.Head());
                    else if (malformed == 1)
                        Bytes::Store(free.Node(1) + 16, free.Node(0));
                    else
                        Bytes::Store(free.Head() + 16, cache.Node(0));
                    const auto before = RemoveHook::calls;
                    action(
                        [&]
                        { refuses([&] { sdk.RemoveFromUncompressedCache(hierarchy.get()); }, "bad active/free backlinks, cycles and shared nodes refuse"); });
                    Check(RemoveHook::calls == before && !runtime.hierarchyCacheRemove.IsRetained() && !runtime.stopping.load(),
                          "malformed cache shape refuses before any store or entered debt");
                }
                seed(0);
                hierarchy.reset();
                Check(finish() && !service.IsRetained() && !host.mallocCalls && !host.freeCalls,
                      "verified cache removal retires the actual owner without allocating or freeing frame storage");
                std::puts("Production hierarchy cache removal actual MTA owner fixture completed");
            }
            else
            {
                seed(0);
                CacheFixture     cache(saved, fixture.image, source.hierarchy, 3, 1);
                FreeCacheFixture free(saved, fixture.image, 2);
                RemoveHook::mode = scenario == 10 ? 1 : scenario == 11 ? 2 : scenario == 12 ? 5 : scenario == 13 ? 4 : 3;
                action([&] { refuses([&] { sdk.RemoveFromUncompressedCache(hierarchy.get()); }, "unknown cache stores cannot publish SDK success"); });
                Check(runtime.hierarchyCacheRemoveReceipt.entered && !runtime.hierarchyCacheRemoveReceipt.effectsVerified &&
                          runtime.hierarchyCacheRemoveReceipt.debt && runtime.hierarchyCacheRemove.RecoveryPending() && NativeLifetimeRetained() &&
                          RemoveHook::calls == 1,
                      "cache command retains the prepared whole-graph/store journal after unknown, partial, mismatched or late-owner effects");
                if (scenario == 11)
                    free.CheckRemoved(cache);
                if (RemoveHook::mode == 3)
                    Bytes::Store(fixture.image + NativeBorrowedGcProfile::markerRva, LONG{0});
                std::array<unsigned char, 72> active{}, afterActive{};
                std::array<unsigned char, 48> freeNodes{}, afterFreeNodes{}, activeEnds{}, afterActiveEnds{}, freeEnds{}, afterFreeEnds{};
                std::array<unsigned char, 40> h{}, afterH{};
                const auto                    snapshot = [&](auto& a, auto& f, auto& ae, auto& fe, auto& header)
                {
                    std::memcpy(a.data(), reinterpret_cast<const void*>(cache.nodes), a.size());
                    std::memcpy(f.data(), reinterpret_cast<const void*>(free.nodes), f.size());
                    std::memcpy(ae.data(), reinterpret_cast<const void*>(cache.Head()), ae.size());
                    std::memcpy(fe.data(), reinterpret_cast<const void*>(free.Head()), fe.size());
                    std::memcpy(header.data(), reinterpret_cast<const void*>(source.hierarchy), header.size());
                };
                snapshot(active, freeNodes, activeEnds, freeEnds, h);
                hierarchy.reset();
                Check(!finish() && service.IsRetained() && runtime.hierarchyCacheRemove.RecoveryPending() && RemoveHook::calls == 1 &&
                          RemoveHook::executed == (scenario == 10 ? 0u : 1u),
                      "End retains unknown cache effects and its module without retrying any store");
                snapshot(afterActive, afterFreeNodes, afterActiveEnds, afterFreeEnds, afterH);
                Check(active == afterActive && freeNodes == afterFreeNodes && activeEnds == afterActiveEnds && freeEnds == afterFreeEnds && h == afterH,
                      "shutdown performs no guessed linked-list repair or rollback");
                std::puts(scenario == 10   ? "Production hierarchy cache removal before acknowledgement debt fixture completed"
                          : scenario == 11 ? "Production hierarchy cache removal after acknowledgement debt fixture completed"
                          : scenario == 12 ? "Production hierarchy cache removal mismatch debt fixture completed"
                          : scenario == 13 ? "Production hierarchy cache removal partial debt fixture completed"
                                           : "Production hierarchy cache removal owner loss debt fixture completed");
            }
            RemoveHook::image = RemoveHook::hierarchy = 0;
        }
        else if (scenario >= 6)
        {
            seed(0);
            CacheFixture cache(saved, fixture.image, source.hierarchy, 3, 1);
            Hook::mode = scenario == 6 ? 1 : scenario == 7 ? 2 : 5;
            action([&]
                   { refuses([&] { sdk.UncompressAnimation(hierarchy.get()); }, "cache entered uncertainty cannot acknowledge a successful manager call"); });
            const auto receipt = runtime.hierarchyUncompressReceipt;
            Check(receipt.cachePath && receipt.cacheNodes == 3 && receipt.modifiedCacheWords == 6 && receipt.entered && !receipt.effectsVerified &&
                      receipt.debt && runtime.hierarchyUncompress.RecoveryPending() && NativeLifetimeRetained() && Hook::calls == 1,
                  "actual cache operation retains its prepared link journal after missing or mismatched acknowledgement");
            if (scenario == 7)
                cache.CheckMoved();
            std::array<unsigned char, 72> nodeBytes{}, afterNodes{};
            std::array<unsigned char, 48> sentinelBytes{}, afterSentinels{};
            std::memcpy(nodeBytes.data(), reinterpret_cast<const void*>(cache.nodes), nodeBytes.size());
            std::memcpy(sentinelBytes.data(), reinterpret_cast<const void*>(cache.Head()), sentinelBytes.size());
            hierarchy.reset();
            Check(!finish() && service.IsRetained() && runtime.hierarchyUncompress.RecoveryPending() && Hook::calls == 1 &&
                      Hook::executed == (scenario == 6 ? 0u : 1u),
                  "End retains entered cache debt without replaying the original or guessing node ownership");
            std::memcpy(afterNodes.data(), reinterpret_cast<const void*>(cache.nodes), afterNodes.size());
            std::memcpy(afterSentinels.data(), reinterpret_cast<const void*>(cache.Head()), afterSentinels.size());
            Check(afterNodes == nodeBytes && afterSentinels == sentinelBytes, "unknown cache links are neither rolled back nor repaired by callback shutdown");
            std::puts(scenario == 6   ? "Production hierarchy Uncompress cache entered-before debt fixture completed"
                      : scenario == 7 ? "Production hierarchy Uncompress cache entered-after debt fixture completed"
                                      : "Production hierarchy Uncompress cache mismatch debt fixture completed");
        }
        else
        {
            Hook::mode = scenario - 1;
            action(
                [&]
                {
                    refuses([&] { sdk.UncompressAnimation(hierarchy.get()); }, "entered uncertainty cannot publish a successful SDK return");
                    const auto& receipt = runtime.hierarchyUncompressReceipt;
                    Check(Hook::calls == 1 && receipt.entered && !receipt.effectsVerified && receipt.debt && runtime.hierarchyUncompress.RecoveryPending() &&
                              runtime.hierarchyUncompress.IsRetained() && NativeLifetimeRetained(),
                          "actual entered call retains its native bank, sequence and frame journal before callback exit");
                });
            if (Hook::mode == 3)
                Bytes::Store(fixture.image + NativeBorrowedGcProfile::markerRva, LONG{0});
            std::array<unsigned char, 64> after{};
            std::memcpy(after.data(), reinterpret_cast<const void*>(frames.pointers[0]), after.size());
            const auto afterTime = Bytes::Load<std::uint32_t>(source.hierarchy + 0x18);
            refuses([&] { sdk.UncompressAnimation(hierarchy.get()); }, "an entered ambiguous call cannot replay through its old retained facade");
            hierarchy.reset();
            Check(runtime.stopping.load() && !finish() && service.IsRetained() && runtime.hierarchyUncompress.RecoveryPending() && Hook::calls == 1 &&
                      Hook::executed == (Hook::mode == 1 ? 0u : 1u) && Bytes::Load<std::uint32_t>(source.hierarchy + 0x18) == afterTime &&
                      !std::memcmp(after.data(), reinterpret_cast<const void*>(frames.pointers[0]), after.size()) && !host.mallocCalls && !host.freeCalls,
                  "End and natural callbacks retain unknown effects without replay, rollback, guessed cleanup or allocation receipts");
            std::puts(Hook::mode == 1   ? "Production hierarchy Uncompress entered before acknowledgement debt fixture completed"
                      : Hook::mode == 2 ? "Production hierarchy Uncompress entered after original debt fixture completed"
                      : Hook::mode == 3 ? "Production hierarchy Uncompress post original owner loss fixture completed"
                                        : "Production hierarchy Uncompress post original effect mismatch fixture completed");
        }
        ClockReaderTestHost::image = previousImage;
        Hook::image = Hook::hierarchy = 0;
        Host::active = nullptr;
        saved.Restore();
    }
}
