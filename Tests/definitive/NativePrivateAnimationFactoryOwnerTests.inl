/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Four real partial manager factories stay on the callback owner
 ****************************************************************************/
namespace PrivateAnimationFactoryOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace ReadHook = ClockPrivateFactoryOwnerReadFixture;
    std::uint64_t* callerGeneration = nullptr;
    std::uint32_t* callerIndex = nullptr;
    void           ChangeCaller() noexcept
    {
        if (callerGeneration)
            *callerGeneration = 0;
        if (callerIndex)
            *callerIndex = 0xffffffffu;
        callerGeneration = nullptr;
        callerIndex = nullptr;
    }
    void LoseAdmission() noexcept
    {
        runtime.admissionGeneration.fetch_add(1);
    }
    bool Same(const NativeAnimationSequenceReferenceToken& a, const NativeAnimationSequenceReferenceToken& b)
    {
        return a.ownerGeneration == b.ownerGeneration && a.firstTag == b.firstTag && a.index == b.index;
    }
    template <class Manager>
    void Run(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
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
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        const auto generation = runtime.generation.load();
        Manager    manager(runtime.access, runtime.access, runtime.access, runtime.access, generation);
        auto&      memory = static_cast<NativeAnimationSequenceStorageAccess&>(runtime.access);
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew a natural callback for each private factory/resolver operation group");
            fixture.invokeTimer();
        };
        std::unique_ptr<CAnimBlendHierarchy> hierarchy, clone;
        CAnimBlendHierarchySAInterface*      hierarchyTag = nullptr;
        CAnimBlendSequenceSAInterface*       array = nullptr;
        std::unique_ptr<CAnimBlendSequence>  last;
        callback.action = [&](NativeGameTimingAccess& access)
        {
            Check(dynamic_cast<NativeAnimationHierarchyAccess*>(&access) == static_cast<NativeAnimationHierarchyAccess*>(&runtime.access),
                  "private factories borrow the same actual hierarchy, sequence, storage and manager callback object");
            hierarchy = manager.GetCustomAnimBlendHierarchy();
            Check(hierarchy != nullptr, "real manager factory issues a private CPU hierarchy facade");
            hierarchyTag = hierarchy->GetInterface();
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            clone = manager.GetCustomAnimBlendHierarchy(hierarchyTag);
            hierarchy->SetName("Private-Factory");
            Check(clone && clone->GetInterface() == hierarchyTag && clone->GetNameHashKey() == hierarchy->GetNameHashKey(),
                  "real borrowed clone shares an issued private record and current owner-backed state");
        };
        invoke();
        hierarchy.reset();
        Check(runtime.privateHierarchies.IsRetained() && !host.freeCalls,
              "CPU facade destruction cannot delete its borrowed private record or native children");
        callback.action = [&](NativeGameTimingAccess&)
        {
            std::uintptr_t issued = 0;
            Check(memory.AllocateAnimationSequences(generation, 3, issued) == ClockResult::Ok,
                  "the actual storage service issues an original array for the four-method factory fixture");
            array = reinterpret_cast<CAnimBlendSequenceSAInterface*>(issued);
            last = manager.GetCustomAnimBlendSequenceAt(array, 2);
            Check(last != nullptr && reinterpret_cast<std::uintptr_t>(last->GetInterface()) == reinterpret_cast<std::uintptr_t>(array) + 160,
                  "actual At factory selects a current row by original allocation capacity");
            last->SetBoneTag(17);
            Check(last->GetBoneTag() == 17, "factory product dispatches the actual qualified sequence implementation");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(!manager.GetCustomAnimBlendSequenceAt(array, 3) && !manager.GetCustomAnimBlendSequenceAt(array, 0x10001u),
                  "qualified full DWORD out-of-capacity indexes are legitimate null products without WORD wrapping");
            bool                                  present = true;
            NativeAnimationSequenceReferenceToken untouched{0x55, 0x66, 0x77}, output = untouched;
            Check(
                runtime.access.ResolvePrivateAnimationSequenceAt(generation, reinterpret_cast<std::uintptr_t>(array), 3, present, output) == ClockResult::Ok &&
                    !present && Same(output, untouched),
                "actual runtime publishes absent only for a qualified original array and preserves the absent token output");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            bool refused = false;
            try
            {
                (void)manager.GetCustomAnimBlendSequenceAt(reinterpret_cast<CAnimBlendSequenceSAInterface*>(reinterpret_cast<std::uintptr_t>(array) + 80), 0);
            }
            catch (const std::runtime_error&)
            {
                refused = true;
            }
            Check(refused, "an interior row tag cannot become an original-array factory authority");
            bool                                  present = true;
            NativeAnimationSequenceReferenceToken untouched{0x55, 0x66, 0x77}, output = untouched;
            Check(runtime.access.ResolvePrivateAnimationSequenceAt(generation + 1, reinterpret_cast<std::uintptr_t>(array), 0, present, output) !=
                          ClockResult::Ok &&
                      present && Same(output, untouched),
                  "foreign generation is refusal rather than missing-row output");
            NativeAnimationHierarchyReferenceToken hOutput{0x55, 0x66};
            Check(runtime.access.ResolvePrivateAnimationHierarchyReference(generation, fixture.image + 0x100, hOutput) != ClockResult::Ok &&
                      hOutput.ownerGeneration == 0x55 && hOutput.locator == 0x66,
                  "actual private resolver cannot adopt a readable foreign image locator");
        };
        invoke();
        std::uint64_t mutableGeneration = generation;
        std::uint32_t mutableIndex = 1;
        ReadHook::address = reinterpret_cast<std::uintptr_t>(array) - 8;
        callback.action = [&](NativeGameTimingAccess&)
        {
            callerGeneration = &mutableGeneration;
            callerIndex = &mutableIndex;
            ReadHook::afterCopy = &ChangeCaller;
            bool                                  present = false;
            NativeAnimationSequenceReferenceToken output;
            Check(runtime.access.ResolvePrivateAnimationSequenceAt(mutableGeneration, reinterpret_cast<std::uintptr_t>(array), mutableIndex, present, output) ==
                          ClockResult::Ok &&
                      present && output.ownerGeneration == generation && output.index == 1 && !mutableGeneration && mutableIndex == 0xffffffffu,
                  "runtime keeps copied full generation/index intent through a real synchronous cookie-copy callback");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            const auto admission = runtime.admissionGeneration.load();
            ReadHook::afterCopy = &LoseAdmission;
            bool                                  present = false;
            NativeAnimationSequenceReferenceToken untouched{0x55, 0x66, 0x77}, output = untouched;
            const auto result = runtime.access.ResolvePrivateAnimationSequenceAt(generation, reinterpret_cast<std::uintptr_t>(array), 1, present, output);
            runtime.admissionGeneration.store(admission);
            Check(result == ClockResult::OwnerConflict && !present && Same(output, untouched) && !ReadHook::afterCopy && !runtime.stopping.load(),
                  "actual output bookends suppress both present/token publication after admission loss during a real cookie read");
        };
        invoke();
        ReadHook::address = 0;
        callback.action = [&](NativeGameTimingAccess&)
        {
            manager.DeleteCustomAnimHierarchyInterface(hierarchyTag);
            bool refused = false;
            try
            {
                (void)clone->GetNumSequences();
            }
            catch (const std::runtime_error&)
            {
                refused = true;
            }
            Check(refused && !runtime.privateHierarchies.IsRetained(), "actual matching delete retires the record while stale borrowed facades refuse");
            NativeAnimationHierarchyReferenceToken output{0x55, 0x66};
            Check(runtime.access.ResolvePrivateAnimationHierarchyReference(generation, reinterpret_cast<std::uintptr_t>(hierarchyTag), output) !=
                          ClockResult::Ok &&
                      output.ownerGeneration == 0x55 && output.locator == 0x66,
                  "retired private locators do not regain factory authority through map absence");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(memory.FreeAnimationSequences(generation, reinterpret_cast<std::uintptr_t>(array)) == ClockResult::Ok,
                  "the actual storage service retires its original array record");
            bool refused = false;
            try
            {
                (void)last->GetBoneTag();
            }
            catch (const std::runtime_error&)
            {
                refused = true;
            }
            Check(refused && !runtime.customSequences.IsRetained() && host.freeCalls == 1,
                  "original array retirement happens once and its old row product has no subsequent native lifetime");
        };
        invoke();
        clone.reset();
        last.reset();
        Check(!manager.GetCustomAnimBlendHierarchy(nullptr) && !manager.GetCustomAnimBlendSequenceAt(nullptr, 0xffffffffu),
              "classic null product paths remain CPU-only even between callbacks");
        manager.DeleteCustomAnimHierarchyInterface(nullptr);
        bool                                  present = true;
        NativeAnimationSequenceReferenceToken output{0x55, 0x66, 0x77};
        Check(runtime.access.ResolvePrivateAnimationSequenceAt(generation, reinterpret_cast<std::uintptr_t>(array), 0, present, output) != ClockResult::Ok &&
                  present && output.ownerGeneration == 0x55 && output.firstTag == 0x66 && output.index == 0x77,
              "CPU service retention between callbacks cannot publish a factory result");
        WorldGroundOwnerFixture::Retire(service, fixture);
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production native private animation factory owner fixture completed");
        std::puts("Production actual private animation factory SDK owner fixture completed");
        std::puts("Production private animation factory output bookends completed");
    }
}
