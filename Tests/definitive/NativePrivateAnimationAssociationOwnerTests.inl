/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute private association bodies under their actual owner protocol
 ****************************************************************************/
namespace ClockPrivateAnimationAssociationTest
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Calls = NativeAnimationAssociationCalls;
    namespace Profile = NativeAnimationAssociationProfile;
    std::uintptr_t              image = 0, originalSource = 0, originalNodes = 0;
    std::uintptr_t              lastReceiver = 0, lastSource = 0, lastConstructor = 0, lastDestructor = 0;
    std::uint32_t               lastDeletingFlags = 0;
    unsigned                    constructorCalls = 0, destructorCalls = 0, exchangeCalls = 0, heapAllocations = 0, heapFrees = 0, mode = 0;
    bool                        targetsValid = true, snapshotsCopied = true, heapFailed = false;
    Host::Calls::MallocFunction originalAllocate = nullptr;
    Host::Calls::FreeFunction   originalRelease = nullptr;
    template <class T>
    T Load(std::uintptr_t address) noexcept
    {
        T value;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
        return value;
    }
    template <class T>
    void Store(std::uintptr_t address, const T& value) noexcept
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }
    std::uintptr_t HeapAllocate(std::uintptr_t receiver, std::size_t bytes, std::uint32_t alignment) noexcept
    {
        ++heapAllocations;
        std::uintptr_t output = 0;
        std::uint32_t  exception = 0;
        if (!Host::active || Host::active->Malloc(originalAllocate, receiver, bytes, alignment, output, exception) != ClockResult::Ok)
            heapFailed = true;
        // All selected copied-body calls use a non-null bounded allocation.
        // A host allocation failure fails this isolated process instead of
        // pretending that copied unwind metadata handles a hardware fault.
        if (!output)
            std::abort();
        return output;
    }
    void HeapRelease(std::uintptr_t receiver, std::uintptr_t originalBase) noexcept
    {
        ++heapFrees;
        std::uint32_t exception = 0;
        if (!Host::active || Host::active->Free(originalRelease, receiver, originalBase, exception) != ClockResult::Ok)
            heapFailed = true;
    }
    struct HeapRoute
    {
        std::uintptr_t table = 0, allocate = 0, release = 0;
        DWORD          protection = 0;
        bool           active = false;
        bool           Begin() noexcept
        {
            if (!Host::active || !Host::active->heap || Host::active->nullMalloc || Host::active->allocationCount >= Host::active->allocations.size())
                return false;
            table = Load<std::uintptr_t>(Host::active->heap);
            allocate = Load<std::uintptr_t>(table + 0x10);
            release = Load<std::uintptr_t>(table + 0x30);
            originalAllocate = reinterpret_cast<Host::Calls::MallocFunction>(allocate);
            originalRelease = reinterpret_cast<Host::Calls::FreeFunction>(release);
            if (!allocate || !release || !VirtualProtect(reinterpret_cast<void*>(table + 0x10), 0x28, PAGE_READWRITE, &protection))
                return false;
            Store(table + 0x10, reinterpret_cast<std::uintptr_t>(&HeapAllocate));
            Store(table + 0x30, reinterpret_cast<std::uintptr_t>(&HeapRelease));
            active = true;
            return true;
        }
        bool Restore() noexcept
        {
            if (!active)
                return true;
            Store(table + 0x10, allocate);
            Store(table + 0x30, release);
            DWORD      ignored = 0;
            const bool restored = VirtualProtect(reinterpret_cast<void*>(table + 0x10), 0x28, protection, &ignored) != FALSE;
            active = false;
            return restored;
        }
        ~HeapRoute()
        {
            if (active)
                (void)Restore();
        }
    };
    ClockResult Construct(Calls::ConstructorFunction function, std::uintptr_t receiver, std::uintptr_t source, std::uintptr_t& returned,
                          std::uint32_t& exception) noexcept
    {
        ++constructorCalls;
        lastReceiver = receiver;
        lastSource = source;
        lastConstructor = reinterpret_cast<std::uintptr_t>(function);
        targetsValid = targetsValid && lastConstructor == image + Profile::ConstructorRva && receiver && source;
        const auto count = Load<std::int16_t>(source + 8);
        snapshotsCopied = snapshotsCopied && source != originalSource && (!count || Load<std::uintptr_t>(source + 0x10) != originalNodes);
        HeapRoute route;
        if (!targetsValid || !route.Begin())
            return ClockResult::WriteFailure;
        // The exact saved constructor calls its exact saved node helper. Only
        // its owned heap virtual10/30 targets are routed to the bounded host;
        // restore them before production heap/profile postqualification.
        const auto result = AnimationAssociationStorageDetail::InvokeConstructor(function, receiver, source, returned, exception);
        if (!route.Restore() || heapFailed)
            return ClockResult::WriteFailure;
        if (result == ClockResult::Ok && mode == 1)
        {
            // Deliberate lost acknowledgement after return, not a hardware
            // exception or an unwind test inside the copied native body.
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        return result;
    }
    ClockResult Destroy(Calls::DestructorFunction function, std::uintptr_t receiver, std::uint32_t flags, std::uintptr_t& returned,
                        std::uint32_t& exception) noexcept
    {
        ++destructorCalls;
        lastReceiver = receiver;
        lastDestructor = reinterpret_cast<std::uintptr_t>(function);
        lastDeletingFlags = flags;
        targetsValid = targetsValid && lastDestructor == image + Profile::DestructorRva && flags == 1;
        HeapRoute route;
        if (!targetsValid || !route.Begin())
            return ClockResult::WriteFailure;
        const auto result = AnimationAssociationStorageDetail::InvokeDestructor(function, receiver, flags, returned, exception);
        if (!route.Restore() || heapFailed)
            return ClockResult::WriteFailure;
        if (result == ClockResult::Ok && mode == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (result == ClockResult::Ok && mode == 3)
            Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        return result;
    }
    ClockResult Exchange(std::uintptr_t address, std::size_t bytes, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, bool& accepted,
                         std::uint32_t& exception) noexcept
    {
        ++exchangeCalls;
        return AnimationAssociationStorageDetail::ExchangeScalar(address, bytes, expected, target, observed, accepted, exception);
    }
}

namespace PrivateAnimationAssociationOwnerFixture
{
    namespace Hook = ClockPrivateAnimationAssociationTest;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Calls = NativeAnimationAssociationCalls;
    using Controller = NativeAnimationAssociationController;
    using Token = NativeAnimationAssociationReferenceToken;
    using Values = NativeAnimationAssociationValues;
    unsigned    scenario = 0;
    Token*      mutableToken = nullptr;
    ClockResult Owner() noexcept
    {
        const auto result = CheckTimingOwner();
        if (mutableToken)
        {
            mutableToken->ownerGeneration = 0;
            mutableToken->serial = 0;
            mutableToken = nullptr;
        }
        return result;
    }
    bool Same(const Token& a, const Token& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.serial == b.serial;
    }
    struct SourceBank
    {
        WorldGroundOwnerFixture::Writes& saved;
        std::uintptr_t                   image = 0, groups = 0, children = 0, nodes = 0, sequences = 0, hierarchy = 0, counter = 0;
        std::array<std::uintptr_t, 4>    allocations{};
        std::array<std::uintptr_t, 5>    map{};
        std::uintptr_t                   Allocate(std::size_t bytes, unsigned index)
        {
            allocations[index] = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(allocations[index] != 0, "source bank owns a bounded private allocation with explicit cookie or map role");
            return allocations[index];
        }
        SourceBank(ClockImageFixture& fixture, WorldGroundOwnerFixture::Writes& writes) : saved(writes), image(fixture.image)
        {
            namespace B = NativeAnimationBlockProfile;
            namespace G = NativeAnimationGroupArrayProfile;
            namespace H = NativeAnimationBlockHierarchyProfile;
            groups = Allocate(8 + 4097 * 32, 0) + 8;
            children = Allocate(8 + 2 * 32, 1) + 8;
            nodes = Allocate(map.size() * sizeof(std::uintptr_t), 2);
            sequences = Allocate(8 + 2 * 80, 3) + 8;
            Hook::Store(groups - 8, std::uint64_t{4097});
            Hook::Store(children - 8, std::uint64_t{2});
            Hook::Store(sequences - 8, std::uint64_t{2});
            Hook::Store(groups + 8, children);
            // Logical Created is deliberately zero. Raw physical source
            // resolution uses the real current parent/child interval only.
            Hook::Store(groups + 0x10, std::uint32_t{0});
            map = {sequences + 80, 0, sequences + 80, sequences, 0};
            std::memcpy(reinterpret_cast<void*>(nodes), map.data(), sizeof(map));
            std::array<unsigned char, B::RowBytes> block{};
            std::memcpy(block.data(), "DYNAMIC-RAW-BANK", 16);
            const std::uint8_t  loaded = 1;
            const std::uint16_t references = 17;
            const std::int32_t  start = 0, count = 1;
            std::memcpy(block.data() + B::LoadedOffset, &loaded, 1);
            std::memcpy(block.data() + NativeAnimationBlockRefProfile::RefOffset, &references, 2);
            std::memcpy(block.data() + B::IdOffset, &start, 4);
            std::memcpy(block.data() + B::AnimationCountOffset, &count, 4);
            saved.Global(B::CountRva, std::int32_t{1});
            WorldGroundOwnerFixture::Writes::Apply(&saved, B::BankRva, block.data(), block.size(), PAGE_READWRITE);
            NativeAnimationBlockFactoryFixture::SeedHierarchy(image, &WorldGroundOwnerFixture::Writes::Apply, &saved, 0, 0);
            hierarchy = image + H::BankRva;
            saved.Global(H::BankRva + 8, sequences);
            saved.Global(H::BankRva + 0x10, std::uint16_t{1});
            saved.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{1});
            saved.Global(G::GlobalRva, groups);
            saved.Global(G::CountRva, std::int32_t{1});
            counter = image + B::BankRva + NativeAnimationBlockRefProfile::RefOffset;
            ResetRow(5);
        }
        void ResetRow(std::uint16_t count)
        {
            Hook::Store(children + 8, count);
            Hook::Store(children + 0x0a, std::int16_t{-32768});
            Hook::Store(children + 0x0c, std::int16_t{-1});
            Hook::Store(children + 0x0e, std::uint16_t{0x10});
            Hook::Store(children + 0x10, count ? nodes : std::uintptr_t{0});
            Hook::Store(children + 0x18, hierarchy);
            Hook::originalSource = children;
            Hook::originalNodes = nodes;
        }
        ~SourceBank()
        {
            // These are task-owned source fixtures, not dynamic allocation
            // receipts. Their disposal never acknowledges engine debt.
            for (const auto allocation : allocations)
                if (allocation)
                    VirtualFree(reinterpret_cast<void*>(allocation), 0, MEM_RELEASE);
        }
    };
    void Diagnose(const char* phase, ClockResult actual, const SourceBank& source, const Host::State& host, const Calls::Receipt& receipt,
                  std::uintptr_t returned)
    {
        NativeAnimationManagerMemory                   memory({source.image}, &CheckTimingOwner);
        NativeAnimationStaticAssociationReferenceToken malformed, valid;
        const auto                                     bad = memory.ResolveStaticAssociationReference(source.children + 1, malformed);
        const auto                                     good = memory.ResolveStaticAssociationReference(source.children, valid);
        NativeAnimationStaticAssociationValues         values;
        const auto read = good == ClockResult::Ok ? memory.ReadStaticAssociation(valid, values) : ClockResult::NotInitialized;
        std::fprintf(stderr,
                     "Private association diagnostic %s: actual=%u owner=%u thread=%u dynamicProfile=%u pool=%u manager=%u block=%u "
                     "rawBad=%u rawGood=%u staticRead=%u groupGlobal=%llx groups=%llx count=%d cookie=%llu child=%llx heap=%llx "
                     "entered=%u accepted=%u verified=%u output=%llx ctor=%u dtor=%u malloc=%u free=%u stopping=%u\n",
                     phase, static_cast<unsigned>(actual), static_cast<unsigned>(CheckTimingOwner()),
                     static_cast<unsigned>(NativeStartGameCalls::CheckThread(source.image)),
                     static_cast<unsigned>(mta::game_de::AnimationAssociationStorageDetail::QualifyAssociationProfile(source.image)),
                     static_cast<unsigned>(NativePedPoolReader::Revalidate({source.image})),
                     static_cast<unsigned>(NativeAnimationManagerMemory::Revalidate({source.image})),
                     static_cast<unsigned>(NativeAnimationBlockMemory::Revalidate({source.image})), static_cast<unsigned>(bad), static_cast<unsigned>(good),
                     static_cast<unsigned>(read),
                     static_cast<unsigned long long>(Hook::Load<std::uintptr_t>(source.image + NativeAnimationGroupArrayProfile::GlobalRva)),
                     static_cast<unsigned long long>(source.groups),
                     static_cast<int>(Hook::Load<std::int32_t>(source.image + NativeAnimationGroupArrayProfile::CountRva)),
                     static_cast<unsigned long long>(Hook::Load<std::uint64_t>(source.groups - 8)), static_cast<unsigned long long>(source.children),
                     static_cast<unsigned long long>(host.heap), static_cast<unsigned>(receipt.entered), static_cast<unsigned>(receipt.accepted),
                     static_cast<unsigned>(receipt.verified), static_cast<unsigned long long>(returned), Hook::constructorCalls, Hook::destructorCalls,
                     host.mallocCalls, host.freeCalls, static_cast<unsigned>(runtime.stopping.load()));
    }
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
        // The direct fixture does not run the old AnimationBlocks wrapper.
        // Its read-only bank foundation still validates these exact pool pins.
        admit(NativePedPoolProfile::Code);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        admit(NativeAnimationBlockProfile::Code);
        // These genuine dispatch cells share executable text pages. Match the
        // established bank fixture preparation without replaying its cases.
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
        saved.Global(NativeAnimationAssociationProfile::VtableRva, fixture.image + NativeAnimationAssociationProfile::DestructorRva, PAGE_READONLY);
        SourceBank source(fixture, saved);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        Hook::image = fixture.image;
        Hook::constructorCalls = Hook::destructorCalls = Hook::exchangeCalls = Hook::heapAllocations = Hook::heapFrees = 0;
        Hook::targetsValid = Hook::snapshotsCopied = true;
        Hook::heapFailed = false;
        Hook::mode = 0;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        HMODULE module = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &module) &&
                  module,
              "private association scope pins the actual callback module once for this process");
        runtime.modulePinned.store(true);
        const auto generation = runtime.generation.load();
        const auto invoke = [&]
        {
            Check(service.Renew(generation), "renew a bounded real callback before each private association action");
            fixture.invokeTimer();
        };
        auto& access = static_cast<NativeAnimationAssociationAccess&>(runtime.access);

        Check(dynamic_cast<NativeAnimationAssociationAccess*>(&runtime.access) == &access,
              "dynamic records use the same actual native callback provider as the other runtime services");
        if (scenario)
        {
            Token token;
            callback.action = [&](NativeGameTimingAccess&)
            {
                Check(access.CreatePrivateAnimationAssociation(generation, token) == ClockResult::Ok && token.ownerGeneration == generation && token.serial,
                      "failure process first issues genuine unconstructed dynamic104 storage");
            };
            invoke();
            bool refused = false;
            callback.action = [&](NativeGameTimingAccess&)
            {
                PrivateAnimationAssociationReceiver observer(access, token);
                Hook::mode = scenario == 1 ? 1u : 0u;
                try
                {
                    (void)observer.Constructor(*reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(source.children));
                }
                catch (const std::runtime_error&)
                {
                    refused = true;
                }
            };
            invoke();
            if (scenario >= 2)
            {
                Check(!refused && Hook::constructorCalls == 1 && Hook::heapAllocations == 1 && Hook::Load<std::uint16_t>(source.counter) == 18,
                      "destructor uncertainty starts only from an acknowledged genuine constructor and one actual block ref");
                callback.action = [&](NativeGameTimingAccess&)
                {
                    Hook::mode = scenario == 2 ? 2u : 3u;
                    const auto result = access.RetirePrivateAnimationAssociation(token);
                    Check(result == (scenario == 2 ? ClockResult::OriginalException : ClockResult::Busy),
                          "destructor return or original-reference postflight loss propagates its precise refusal status");
                };
                invoke();
            }
            Check((scenario == 1 ? refused && !Hook::destructorCalls && !host.freeCalls : Hook::destructorCalls == 1 && host.freeCalls == 2) &&
                      Hook::constructorCalls == 1 && Hook::targetsValid && Hook::snapshotsCopied && !Hook::heapFailed,
                  "injected failure follows the real copied body and never fabricates allocator or ownership completion");
            Check(access.HasPendingPrivateAnimationAssociations() && runtime.privateAssociations.IsRetained() &&
                      runtime.privateAssociations.RecoveryPending() && runtime.modulePinned.load() && service.IsRetained(),
                  "unknown entered constructor or destructor effects keep the actual controller and module debt");
            const auto frees = host.freeCalls, constructors = Hook::constructorCalls, destructors = Hook::destructorCalls;
            Hook::mode = 0;
            saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
            Check(!service.End(), "unknown dynamic effects prevent successful producer shutdown");
            fixture.invokeTimer();
            Check(!service.End() && host.freeCalls == frees && Hook::constructorCalls == constructors && Hook::destructorCalls == destructors &&
                      NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "actual End drainer neither replays a constructor/destructor nor guesses a cleanup acknowledgement");
            Check(Hook::Load<std::uint16_t>(source.counter) == (scenario == 1 ? 18 : 17),
                  "unknown receipt state never rewinds the genuine native reference effect");
            std::puts(scenario == 1   ? "Production private animation association constructor uncertainty retained"
                      : scenario == 2 ? "Production private animation association destructor uncertainty retained"
                                      : "Production private animation association returned destructor reference debt retained");
            return;
        }
        Controller     controller;
        Token          local;
        Calls::Receipt receipt;
        std::uintptr_t returned = 0x55667788;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(controller.Create(fixture.image, generation, &Owner, local, &receipt) == ClockResult::Ok && receipt.allocated && receipt.verified &&
                      receipt.receiver && local.ownerGeneration == generation && local.serial,
                  "actual controller reserves a serial record before the acknowledged original-heap104 allocation");
            Values unchanged;
            unchanged.interfaceAddress = 0x1122;
            Check(controller.Read(fixture.image, generation, local, &Owner, unchanged) == ClockResult::NotInitialized && unchanged.interfaceAddress == 0x1122,
                  "unconstructed storage cannot publish a fabricated ready interface");
            const auto before = Hook::constructorCalls;
            auto       foreign = local;
            ++foreign.ownerGeneration;
            Check(controller.Construct(fixture.image, generation, foreign, source.children, &Owner, returned, &receipt) == ClockResult::OwnerConflict &&
                      !receipt.entered && returned == 0x55667788 && Hook::constructorCalls == before,
                  "foreign issued identity refuses before constructor entry and preserves the returned tag");
            const auto malformedResult = controller.Construct(fixture.image, generation, local, source.children + 1, &Owner, returned, &receipt);
            if (malformedResult != ClockResult::InvalidArgument || receipt.entered || returned != 0x55667788 || Hook::constructorCalls != before)
                Diagnose("malformed-source", malformedResult, source, host, receipt, returned);
            Check(malformedResult == ClockResult::InvalidArgument && !receipt.entered && returned == 0x55667788 && Hook::constructorCalls == before,
                  "an unaligned raw source cannot acquire a physical static-row identity");
            Hook::Store(source.children + 0x0e, std::uint16_t{0x4010});
            Check(controller.Construct(fixture.image, generation, local, source.children, &Owner, returned, &receipt) == ClockResult::InvalidArgument &&
                      !receipt.entered && Hook::Load<std::uint16_t>(source.counter) == 17 && Hook::constructorCalls == before,
                  "an incoming4000 bit is not a reference hold and refuses without an extra native AddRef");
            source.ResetRow(5);
            Hook::Store(source.counter, std::uint16_t{0xffff});
            Check(controller.Construct(fixture.image, generation, local, source.children, &Owner, returned, &receipt) == ClockResult::OwnerConflict &&
                      !receipt.entered && Hook::constructorCalls == before,
                  "the private ownership subset refuses a full reference counter before native increment");
            Hook::Store(source.counter, std::uint16_t{17});
            Check(controller.RetireOne(fixture.image, generation, local, &Owner, &receipt) == ClockResult::Ok && receipt.retired && !controller.IsRetained() &&
                      !Hook::destructorCalls && host.freeCalls == 1,
                  "unconstructed dynamic storage uses only its original-heap free and never a guessed destructor");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            NativeAnimationManagerMemory                   memory({fixture.image}, &Owner);
            NativeAnimationStaticAssociationReferenceToken output{0x44, 0x55, 0x66, 0x77};
            const auto                                     untouched = output;
            saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{4097});
            Check(memory.ResolveStaticAssociationReference(source.children, output) == ClockResult::Busy &&
                      output.ownerGeneration == untouched.ownerGeneration && output.groupIndex == untouched.groupIndex && output.index == untouched.index &&
                      output.locator == untouched.locator,
                  "raw resolution applies the explicit4096 parent-work bound without changing output");
            saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{4096});
            Check(memory.ResolveStaticAssociationReference(source.children, output) == ClockResult::Ok && !output.ownerGeneration && !output.groupIndex &&
                      !output.index && output.locator == source.children,
                  "the exact4096 boundary resolves current physical rows without enum or Created filtering");
            saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{2});
            Hook::Store(source.groups + 32 + 8, source.children);
            output = untouched;
            Check(memory.ResolveStaticAssociationReference(source.children, output) == ClockResult::OwnerConflict && output.locator == untouched.locator,
                  "two current parent intervals cannot fabricate a unique raw static-row identity");
            Hook::Store(source.groups + 32 + 8, std::uintptr_t{0});
            saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{1});
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            source.ResetRow(0);
            Hook::Store(source.children + 0x10, std::uintptr_t{1});
            Check(controller.Create(fixture.image, generation, &Owner, local, &receipt) == ClockResult::Ok,
                  "zero-node constructor has its own explicit original104 allocation");
            const auto calls = Hook::constructorCalls;
            auto       caller = local;
            mutableToken = &caller;
            const auto validResult = controller.Construct(fixture.image, generation, caller, source.children, &Owner, returned, &receipt);
            if (validResult != ClockResult::Ok)
                Diagnose("valid-zero-constructor", validResult, source, host, receipt, returned);
            Check(validResult == ClockResult::Ok && receipt.accepted && receipt.constructed && receipt.refVerified && receipt.verified &&
                      receipt.nodeCount == 0 && receipt.nodeRequestBytes == 64 && receipt.nodes % 64 == 0 && receipt.nodes != receipt.nodeRaw &&
                      Hook::Load<std::uintptr_t>(receipt.nodes - 8) == receipt.nodeRaw && !caller.ownerGeneration && !caller.serial &&
                      Hook::constructorCalls == calls + 1,
                  "actual zero-count helper still allocates aligned64 raw-header storage and controller copies caller intent");
            Check(Hook::Load<std::uint16_t>(source.counter) == 18 && receipt.receiver == returned && host.Contains(receipt.receiver) &&
                      host.Contains(receipt.nodeRaw),
                  "one genuine constructor adds one reference and records both original allocator bases");
            Check(controller.RetireOne(fixture.image, generation, local, &Owner, &receipt) == ClockResult::Ok && receipt.retired && receipt.verified &&
                      Hook::lastDeletingFlags == 1 && Hook::Load<std::uint16_t>(source.counter) == 17 && !controller.IsRetained(),
                  "actual deleting destructor frees raw node header and104 outer once and balances that reference");
            source.ResetRow(5);
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(controller.Create(fixture.image, generation, &Owner, local, &receipt) == ClockResult::Ok &&
                      controller.Construct(fixture.image, generation, local, source.children, &Owner, returned, &receipt) == ClockResult::Ok &&
                      receipt.verified && receipt.refVerified && receipt.nodeCount == 5 && receipt.nodeRequestBytes == 320,
                  "actual constructor executes both native node initialization loops with a current bank hierarchy");
            for (std::size_t index = 0; index != source.map.size(); ++index)
            {
                const auto node = receipt.nodes + index * 40;
                Check(Hook::Load<std::uint32_t>(node + 8) == 0xffffffffu && !Hook::Load<std::uint32_t>(node + 0x0c) &&
                          Hook::Load<std::uintptr_t>(node + 0x10) == source.map[index] && Hook::Load<std::uintptr_t>(node + 0x18) == receipt.receiver &&
                          Hook::Load<std::uint32_t>(node + 0x20) == 0xffffffffu,
                      "real node40 fields preserve aliased/null sequence pointers and identify their actual dynamic owner");
            }
            Values values;
            Check(controller.Read(fixture.image, generation, local, &Owner, values) == ClockResult::Ok && values.interfaceAddress == returned &&
                      values.animId == -32768 && values.animGroup == -1 && static_cast<std::uint16_t>(values.flags) == 0x4010 &&
                      values.blendAmountBits == 0x3f800000 && values.speedBits == 0x3f800000,
                  "metadata publishes only actual copied IDs/flags and native raw float defaults");
            const auto exchanges = Hook::exchangeCalls;
            Check(controller.Set(fixture.image, generation, local, Calls::Scalar::Flags, 0x10, &Owner, &receipt) == ClockResult::OwnerConflict &&
                      !receipt.entered && Hook::exchangeCalls == exchanges && Hook::Load<std::uint16_t>(source.counter) == 18,
                  "private flag mutation cannot silently clear or transfer the owned4000 resource bit");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            // These independent SDK actions receive normal fresh callback
            // leases. One test sequence must not consume a production lease
            // while printing and checking earlier constructor assertions.
            Values         values;
            const auto     blendResult = controller.Set(fixture.image, generation, local, Calls::Scalar::BlendAmount, 0x7fc12345, &Owner, &receipt);
            const auto     blendReceipt = receipt;
            auto           speedResult = ClockResult::NotInitialized, readResult = ClockResult::NotInitialized;
            Calls::Receipt speedReceipt;
            bool           speedAttempted = false, readAttempted = false;
            // Preserve the original short-circuit effects exactly: a refused
            // or unverified earlier operation never enters a later setter.
            if (blendResult == ClockResult::Ok && blendReceipt.verified)
            {
                speedAttempted = true;
                speedResult = controller.Set(fixture.image, generation, local, Calls::Scalar::CurrentSpeed, 0x80000000, &Owner, &receipt);
                speedReceipt = receipt;
                if (speedResult == ClockResult::Ok)
                {
                    readAttempted = true;
                    readResult = controller.Read(fixture.image, generation, local, &Owner, values);
                }
            }
            const bool scalarPassed = blendResult == ClockResult::Ok && blendReceipt.verified && speedAttempted && speedResult == ClockResult::Ok &&
                                      readAttempted && readResult == ClockResult::Ok && values.blendAmountBits == 0x7fc12345 && values.speedBits == 0x80000000;
            if (!scalarPassed)
            {
                const auto ownerStatus = CheckTimingOwner();
                const bool rawAvailable = ownerStatus == ClockResult::Ok && host.Contains(returned) &&
                                          NativeModelReferenceMap::PrivateWritable(returned, NativeAnimationAssociationProfile::ReceiverBytes);
                const auto rawBlend = rawAvailable ? Hook::Load<std::uint32_t>(returned + 0x30) : std::uint32_t{0};
                const auto rawSpeed = rawAvailable ? Hook::Load<std::uint32_t>(returned + 0x3c) : std::uint32_t{0};
                const auto now = GetTickCount64(), refreshed = runtime.refreshed.load();
                std::fprintf(
                    stderr,
                    "Private scalar diagnostic: blend=%u entered=%u accepted=%u verified=%u speedAttempted=%u speed=%u entered=%u accepted=%u verified=%u "
                    "readAttempted=%u read=%u owner=%u fresh=%u ageMs=%llu leaseMs=%u recovery=%u blendBits=%08x speedBits=%08x rawAvailable=%u raw30=%08x "
                    "raw3c=%08x\n",
                    static_cast<unsigned>(blendResult), static_cast<unsigned>(blendReceipt.entered), static_cast<unsigned>(blendReceipt.accepted),
                    static_cast<unsigned>(blendReceipt.verified), static_cast<unsigned>(speedAttempted), static_cast<unsigned>(speedResult),
                    static_cast<unsigned>(speedReceipt.entered), static_cast<unsigned>(speedReceipt.accepted), static_cast<unsigned>(speedReceipt.verified),
                    static_cast<unsigned>(readAttempted), static_cast<unsigned>(readResult), static_cast<unsigned>(ownerStatus),
                    static_cast<unsigned>(mta::game_de::Fresh()), static_cast<unsigned long long>(now >= refreshed ? now - refreshed : 0),
                    static_cast<unsigned>(ClockServiceLeaseMsV1), static_cast<unsigned>(controller.RecoveryPending()), values.blendAmountBits, values.speedBits,
                    static_cast<unsigned>(rawAvailable), rawBlend, rawSpeed);
            }
            Check(scalarPassed, "actual guarded scalar writer preserves raw NaN and signed-zero bits without a speed or blend clamp");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(controller.RetireOne(fixture.image, generation, local, &Owner, &receipt) == ClockResult::Ok && receipt.retired && !controller.IsRetained() &&
                      Hook::Load<std::uint16_t>(source.counter) == 17,
                  "explicit controller retirement consumes only its own nodes, outer allocation and native reference");
        };
        invoke();
        Token                                                runtimeToken;
        std::unique_ptr<PrivateAnimationAssociationReceiver> observer;
        CAnimBlendAssociationSAInterface*                    sdkRow = nullptr;
        std::uint64_t                                        sdkFrees = 0;
        callback.action = [&](NativeGameTimingAccess& current)
        {
            Check(dynamic_cast<NativeAnimationAssociationAccess*>(&current) == &access &&
                      access.CreatePrivateAnimationAssociation(generation, runtimeToken) == ClockResult::Ok,
                  "actual callback exposes the typed optional owner and issues its private serial");
            sdkFrees = host.freeCalls;
            observer = std::make_unique<PrivateAnimationAssociationReceiver>(access, runtimeToken);
            sdkRow = observer->Constructor(*reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(source.children));
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(sdkRow && observer->GetInterface() == sdkRow && static_cast<int>(observer->GetAnimID()) == -32768 &&
                      static_cast<int>(observer->GetAnimGroup()) == -1 && observer->IsPartial(),
                  "eleven-method strict SDK receiver inherits the actual constructor and current metadata dispatch");
        };
        invoke();
        // The strict receiver and native serial survive callback boundaries;
        // that does not extend any individual 500ms native admission lease.
        callback.action = [&](NativeGameTimingAccess&) { observer->SetAnimID(321); };
        invoke();
        callback.action = [&](NativeGameTimingAccess&) { observer->SetAnimGroup(-12); };
        invoke();
        callback.action = [&](NativeGameTimingAccess&) { observer->SetFlags(static_cast<short>(0xc000u)); };
        invoke();
        const std::uint32_t nanBits = 0x7fc54321;
        callback.action = [&](NativeGameTimingAccess&)
        {
            float nan = 0;
            std::memcpy(&nan, &nanBits, sizeof(nan));
            observer->SetBlendAmount(nan);
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&) { observer->SetCurrentSpeed(-2.0f); };
        invoke();
        std::uint32_t amountBits = 0;
        callback.action = [&](NativeGameTimingAccess&)
        {
            const auto amount = observer->GetBlendAmount();
            std::memcpy(&amountBits, &amount, sizeof(amountBits));
        };
        invoke();
        Values sdkValues;
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(static_cast<int>(observer->GetAnimID()) == 321 && static_cast<int>(observer->GetAnimGroup()) == -12 && !observer->IsPartial() &&
                      amountBits == nanBits && access.ReadPrivateAnimationAssociation(runtimeToken, sdkValues) == ClockResult::Ok &&
                      sdkValues.speedBits == 0xc0000000u,
                  "real SDK setters and typed speed observation retain full signed and float bits");
        };
        invoke();
        observer.reset();
        Check(host.freeCalls == sdkFrees && access.HasPendingPrivateAnimationAssociations(),
              "CPU observer destruction cannot delete native children or turn a retained record into completion");
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(access.RetirePrivateAnimationAssociation(runtimeToken) == ClockResult::Ok && !access.HasPendingPrivateAnimationAssociations() &&
                      Hook::Load<std::uint16_t>(source.counter) == 17,
                  "actual Runtime retirement acknowledges the engine deleting destructor before dropping its own record");
        };
        invoke();
        callback.action = [&](NativeGameTimingAccess&)
        {
            Check(access.CreatePrivateAnimationAssociation(generation, runtimeToken) == ClockResult::Ok,
                  "End drainer receives one separately issued private dynamic record");
            PrivateAnimationAssociationReceiver observer(access, runtimeToken);
            (void)observer.Constructor(*reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(source.children));
        };
        invoke();
        Check(access.HasPendingPrivateAnimationAssociations() && Hook::Load<std::uint16_t>(source.counter) == 18,
              "dropping a ready CPU observer leaves the actual producer's native retirement obligation");
        const auto beforeDtor = Hook::destructorCalls, beforeFree = host.freeCalls;
        Check(!service.End() && Hook::destructorCalls == beforeDtor && host.freeCalls == beforeFree,
              "outside-callback End cannot manufacture a native destructor acknowledgement");
        fixture.invokeTimer();
        Check(service.End(), "the qualified native timer drains the ready dynamic record before releasing hook debt");
        Check(!service.IsRetained() && !runtime.privateAssociations.IsRetained() && runtime.modulePinned.load() &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && Hook::destructorCalls == beforeDtor + 1 &&
                  host.freeCalls == beforeFree + 2 && Hook::Load<std::uint16_t>(source.counter) == 17,
              "acknowledged retirement never frees twice and preserves the genuine process-lifetime module pin");
        Check(Hook::targetsValid && Hook::snapshotsCopied && !Hook::heapFailed && Hook::heapAllocations == Hook::constructorCalls &&
                  Hook::heapFrees == Hook::destructorCalls * 2,
              "all copied bodies used exact pinned functions, immutable CPU inputs and only bounded host heap targets");
        ClockReaderTestHost::image = previousImage;
        Hook::image = 0;
        Host::active = nullptr;
        saved.Restore();
        std::puts("Production private animation association owner fixture completed");
        std::puts("Production actual private animation association SDK fixture completed");
        std::puts("Production copied dynamic constructor helper and destructor executed in owned fixture");
    }
}
