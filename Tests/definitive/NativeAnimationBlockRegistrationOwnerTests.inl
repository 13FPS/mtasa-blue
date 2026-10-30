/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK register/find/append with retained uncertain stores
 ****************************************************************************/
namespace BlockRegistrationOwnerFixture
{
    namespace Calls = NativeAnimationBlockRegistrationCalls;
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    namespace Conversion = NativeAnimationHierarchyDecompressProfile;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Profile = NativeAnimationAssociationProgressProfile;
    namespace Block = NativeAnimationBlockProfile;
    namespace Definition = NativeAnimationAssocDefinitionProfile;
    unsigned                 scenario = 0, calls = 0, executed = 0, mode = 0;
    std::uintptr_t           image = 0, hierarchy = 0;
    bool                     valid = true, nestedBusy = false, gateBusy = false, retained = false;
    NativeAnimationBlockName Name(const char* text)
    {
        NativeAnimationBlockName value{};
        const auto               size = std::strlen(text);
        Check(size < value.size(), "fixture name fits native bounded spelling");
        std::memcpy(value.data(), text, size);
        return value;
    }
    ClockResult Write(const Calls::Store* stores, std::size_t count, std::uint32_t& exception) noexcept
    {
        ++calls;
        valid = valid && count == 7 && stores[0].address == image + Block::CountRva &&
                stores[1].address == image + Block::BankRva + static_cast<std::uintptr_t>(stores[0].value - 1) * 32 &&
                CheckApplicationTimingOwner() == ClockResult::Ok;
        std::int32_t ignored = 991;
        nestedBusy = runtime.access.RegisterAnimationBlock(runtime.generation.load(), Name("nested"), ignored) == ClockResult::Busy && ignored == 991;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        retained = runtime.blockRegistration.IsRetained() && NativeLifetimeRetained();
        if (!valid)
            return ClockResult::BindingChanged;
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        // Invoke the shipping guarded writer. Fault cases deliberately lose
        // acknowledgement after a real prefix/full source-ordered store list.
        const auto result = mta::game_de::AnimationBlockRegistrationDetail::WriteStores(stores, mode == 3 ? 3 : count, exception);
        if (result != ClockResult::Ok)
            return result;
        ++executed;
        if (mode == 2 || mode == 3)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::WriteFailure;
        }
        if (mode == 4)
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 5)
            Bytes::Store(stores[1].address, Bytes::Load<std::uint32_t>(stores[1].address) ^ 1u);
        if (mode == 6)
            Bytes::Store(image + Block::CountRva, std::int32_t{3});
        if (mode == 7)
            Bytes::Store(image + Block::BankRva, std::uint32_t{0x78787878});
        if (mode == 8)
            Bytes::Store(image + Definition::BankRva + 3 * Definition::RowBytes + Definition::BlockNameOffset, std::uint32_t{0x78787878});
        return ClockResult::Ok;
    }
    ApplicationFrameFunction originalFrame = nullptr;
    unsigned                 originals = 0;
    bool                     originalReturned = false;
    void                     Original(void* receiver, float delta)
    {
        ++originals;
        originalReturned = false;
        NativeAnimationManagerCounts counts;
        Check(!timingCallbackGeneration && !timingTimerScope && !frameCallbackGeneration && !frameCallbackReceiver && !frameCallbackSerial &&
                  runtime.access.ReadAnimationManagerCounts(runtime.generation.load(), counts) != ClockResult::Ok,
              "whole original frame has no lent manager registration authority");
        originalFrame(receiver, delta);
        originalReturned = true;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        // The real frame root is shared with the typed startup/world graph.
        // Admit frame code first, then preserve that graph's actual receiver.
        fixture.AdmitApplicationPins();
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        constexpr auto                  definitionPage = NativeAnimationAssocDefinitionProfile::BankRva & ~std::uintptr_t{0xfff};
        constexpr auto                  definitionSpan = ((NativeAnimationAssocDefinitionProfile::BankRva +
                                          NativeAnimationAssociationGroupPrefixCount * NativeAnimationAssocDefinitionProfile::RowBytes + 0xfff) &
                                         ~std::uintptr_t{0xfff}) -
                                        definitionPage;
        const auto dos = fixture.Get<IMAGE_DOS_HEADER>(0);
        const auto nt = fixture.Get<IMAGE_NT_HEADERS64>(static_cast<std::uintptr_t>(dos.e_lfanew));
        const auto sectionRva = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        std::vector<IMAGE_SECTION_HEADER> oldSections(nt.FileHeader.NumberOfSections), sections;
        SIZE_T                            geometryCopied = 0;
        Check(ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + sectionRva), oldSections.data(),
                                oldSections.size() * sizeof(IMAGE_SECTION_HEADER), &geometryCopied) &&
                  geometryCopied == oldSections.size() * sizeof(IMAGE_SECTION_HEADER),
              "read actual owned PE geometry before admitting the bounded definition data page");
        bool split = false;
        for (const auto& section : oldSections)
        {
            const auto end = static_cast<std::uintptr_t>(section.VirtualAddress) + section.Misc.VirtualSize;
            if (section.VirtualAddress < definitionPage && end > definitionPage + definitionSpan)
            {
                Check(!split && !section.SizeOfRawData && (section.Characteristics & IMAGE_SCN_MEM_READ) &&
                          !(section.Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)),
                      "the new row page splits only its existing synthetic read-only zero section");
                auto before = section, data = section, after = section;
                before.Misc.VirtualSize = static_cast<DWORD>(definitionPage - section.VirtualAddress);
                data.VirtualAddress = static_cast<DWORD>(definitionPage);
                data.Misc.VirtualSize = static_cast<DWORD>(definitionSpan);
                data.Characteristics |= IMAGE_SCN_MEM_WRITE;
                after.VirtualAddress = static_cast<DWORD>(definitionPage + definitionSpan);
                after.Misc.VirtualSize = static_cast<DWORD>(end - definitionPage - definitionSpan);
                sections.push_back(before);
                sections.push_back(data);
                sections.push_back(after);
                split = true;
            }
            else
                sections.push_back(section);
        }
        Check(
            split && sections.size() == oldSections.size() + 2 && sections.size() < 96 && sectionRva + sections.size() * sizeof(IMAGE_SECTION_HEADER) <= 0x1000,
            "bounded RO/RW/RO split retains adjacent virtual ranges inside the original owned header page");
        const auto sectionCount = static_cast<WORD>(sections.size());
        saved.Global(static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, FileHeader) + offsetof(IMAGE_FILE_HEADER, NumberOfSections),
                     sectionCount, PAGE_READONLY);
        WorldGroundOwnerFixture::Writes::Apply(&saved, sectionRva, sections.data(), sections.size() * sizeof(IMAGE_SECTION_HEADER), PAGE_READONLY);

        const auto admit = [&](const auto& pins)
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
        AnimationManagerRegistrationReceiver manager(runtime.access, runtime.access, generation);
        CAnimManager&                        sdk = manager;
        const auto                           refuses = [&](const auto& operation, const char* why)
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

        admit(NativeAnimationManagerProfile::Code);
        WorldGroundOwnerFixture::Writes::Apply(&saved, NativeAnimationManagerProfile::NameFoldRva, NativeAnimationManagerProfile::NameFold.data(),
                                               NativeAnimationManagerProfile::NameFold.size(), PAGE_READONLY);
        // Definition lookup uses its own actual script-group entry, distinct
        // from the animation-block loaded opcode's group slot.
        saved.Global(Definition::GroupSlotRva, image + Definition::GroupRva, PAGE_READONLY);
        admit(Definition::Code);
        admit(Definition::BlockNameCode);
        std::vector<unsigned char> definitions(Definition::BlockNamePrefixCount * Definition::RowBytes, 0);
        for (unsigned row = 0; row != Definition::BlockNamePrefixCount; ++row)
            std::memcpy(definitions.data() + row * Definition::RowBytes + Definition::BlockNameOffset, "unused", 7);
        std::memcpy(definitions.data() + 3 * Definition::RowBytes + Definition::BlockNameOffset, "new_a", 6);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Definition::BankRva, definitions.data(), definitions.size(), PAGE_READWRITE);
        saved.Global(Definition::CountRva, std::int32_t{0});
        // The observed bank prefix is not claimed to be native allocation
        // capacity. Zero unused rows model the admitted fresh append domain.
        std::vector<unsigned char> bank(Block::ObservationLimit * Block::RowBytes, 0);
        std::memcpy(bank.data(), reinterpret_cast<const void*>(image + Block::BankRva), Block::RowBytes);
        std::memset(bank.data(), 0, 16);
        std::memcpy(bank.data(), "stock", 6);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Block::BankRva, bank.data(), bank.size(), PAGE_READWRITE);
        saved.Global(Block::CountRva, std::int32_t{1});
        const auto snapshot = [&]
        {
            std::vector<unsigned char> bytes(bank.size() + 4);
            std::memcpy(bytes.data(), reinterpret_cast<const void*>(image + Block::BankRva), bank.size());
            std::memcpy(bytes.data() + bank.size(), reinterpret_cast<const void*>(image + Block::CountRva), 4);
            return bytes;
        };
        const auto registration = [&](const char* name, int expectedIndex, unsigned expectedGroup, bool append)
        {
            const auto before = snapshot();
            const auto beforeCalls = calls;
            int        returned = -42;
            action([&] { returned = sdk.RegisterAnimBlock(name); });
            const auto receipt = runtime.blockRegistrationReceipt;
            if (returned != expectedIndex || receipt.result != ClockResult::Ok || calls != beforeCalls + (append ? 1u : 0u) || !valid)
                std::fprintf(stderr, "registration %s: returned=%d receipt=%d result=%u stores=%u entered=%u verified=%u calls=%u valid=%u\n", name, returned,
                             receipt.index, static_cast<unsigned>(receipt.result), receipt.stores, receipt.entered ? 1u : 0u, receipt.effectsVerified ? 1u : 0u,
                             calls - beforeCalls, valid ? 1u : 0u);
            Check(returned == expectedIndex && receipt.index == expectedIndex && receipt.generation == generation && receipt.effectsVerified && !receipt.debt &&
                      receipt.result == ClockResult::Ok && receipt.noWrite == !append && receipt.entered == append && receipt.stores == (append ? 7u : 0u) &&
                      calls == beforeCalls + (append ? 1u : 0u) && valid && (!append || (nestedBusy && gateBusy && retained)),
                  "actual MTA RegisterAnimBlock returns only a verified existing or fully appended current index");
            auto expected = before;
            if (append)
            {
                const auto row = static_cast<std::size_t>(expectedIndex) * 32;
                const auto copied = Name(name);
                std::memcpy(expected.data() + row, copied.data(), 16);
                const std::uint32_t zero = 0;
                std::memcpy(expected.data() + row + 0x18, &zero, 4);
                std::memcpy(expected.data() + row + 0x1c, &expectedGroup, 4);
                const std::int32_t afterCount = expectedIndex + 1;
                std::memcpy(expected.data() + bank.size(), &afterCount, 4);
                Check(receipt.group == expectedGroup && receipt.beforeCount == expectedIndex && receipt.afterCount == afterCount,
                      "registered group and count follow the captured classic policy on qualified DE layout");
            }
            Check(snapshot() == expected, "complete bank prefix/count preserves existing and opaque loaded/ref/start bytes exactly");
            action(
                [&]
                {
                    Check(sdk.GetAnimationBlockIndex(name) == expectedIndex && sdk.GetNumAnimBlocks() == receipt.afterCount,
                          "existing MTA name/counter readers observe the verified registration without a second registry");
                });
            Check(!runtime.blockRegistration.IsRetained() && !runtime.stopping.load(), "verified registration leaves native owner reusable");
        };
        if (scenario == 0)
        {
            // The two matched MTA entries have identical counter-only effects.
            // Exercise the inherited public SDK methods on the actual runtime
            // owner, including unloaded rows and WORD underflow; compare every
            // bank/count byte so a hidden unload or metadata reset cannot pass.
            const auto originalBank = snapshot();
            const auto originalAnimations = Bytes::Load<std::int32_t>(image + NativeAnimationManagerProfile::AnimationCountRva);
            const auto registrationCalls = calls;
            const auto referenceAddress = image + Block::BankRva + NativeAnimationBlockRefProfile::RefOffset;
            for (const std::uint8_t loaded : {std::uint8_t{0}, std::uint8_t{1}})
                for (const std::uint16_t refs : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0xffff}})
                {
                    std::array<unsigned char, Block::RowBytes> row{};
                    std::memcpy(row.data(), originalBank.data(), row.size());
                    row[Block::LoadedOffset] = loaded;
                    row[0x11] = 0xa7;
                    std::memcpy(row.data() + NativeAnimationBlockRefProfile::RefOffset, &refs, sizeof(refs));
                    WorldGroundOwnerFixture::Writes::Apply(&saved, Block::BankRva, row.data(), row.size(), PAGE_READWRITE);
                    const auto before = snapshot();
                    auto       expected = before;
                    const auto decremented = static_cast<std::uint16_t>(static_cast<std::uint32_t>(refs) + 0xffffu);
                    std::memcpy(expected.data() + NativeAnimationBlockRefProfile::RefOffset, &decremented, sizeof(decremented));
                    std::int16_t signedDecremented = 0, signedBefore = 0;
                    std::memcpy(&signedDecremented, &decremented, sizeof(signedDecremented));
                    std::memcpy(&signedBefore, &refs, sizeof(signedBefore));
                    ClockAnimationBlockRefOwnerTest::Reset();
                    ClockAnimationBlockRefOwnerTest::address = referenceAddress;
                    action(
                        [&]
                        {
                            sdk.RemoveAnimBlockRef(0);
                            Check(sdk.GetNumRefsToAnimBlock(0) == signedDecremented,
                                  "actual public RemoveAnimBlockRef exposes signed WORD result without a last-reference unload");
                        });
                    const auto publicResult = snapshot();
                    Check(publicResult == expected && ClockAnimationBlockRefOwnerTest::exchanges == 1 && calls == registrationCalls &&
                              Bytes::Load<std::int32_t>(image + NativeAnimationManagerProfile::AnimationCountRva) == originalAnimations,
                          "matched MTA reference policy changes only one WORD for loaded0/1 and counters0/1/FFFF, with no registration stores");
                    WorldGroundOwnerFixture::Writes::Apply(&saved, Block::BankRva, before.data(), Block::RowBytes, PAGE_READWRITE);
                    ClockAnimationBlockRefOwnerTest::Reset();
                    action(
                        [&]
                        {
                            sdk.RemoveAnimBlockRefWithoutDelete(0);
                            Check(sdk.GetNumRefsToAnimBlock(0) == signedDecremented,
                                  "the separately inherited nondeleting API exposes the same signed WORD result");
                        });
                    Check(snapshot() == publicResult && ClockAnimationBlockRefOwnerTest::exchanges == 1 && calls == registrationCalls &&
                              Bytes::Load<std::int32_t>(image + NativeAnimationManagerProfile::AnimationCountRva) == originalAnimations,
                          "both actual public reference APIs have byte-identical complete bank/count effects");
                    action(
                        [&]
                        {
                            sdk.AddAnimBlockRef(0);
                            Check(sdk.GetNumRefsToAnimBlock(0) == signedBefore, "the inherited actual add/get APIs preserve full WORD modulo semantics");
                        });
                    Check(snapshot() == before && ClockAnimationBlockRefOwnerTest::exchanges == 2 && calls == registrationCalls &&
                              Bytes::Load<std::int32_t>(image + NativeAnimationManagerProfile::AnimationCountRva) == originalAnimations &&
                              !runtime.stopping.load() && !runtime.blockRegistration.IsRetained(),
                          "reference progress preserves every other row/count field and leaves the actual owner reusable");
                }
            WorldGroundOwnerFixture::Writes::Apply(&saved, Block::BankRva, originalBank.data(), Block::RowBytes, PAGE_READWRITE);
            ClockAnimationBlockRefOwnerTest::Reset();
            for (const int index : {-1, 1, static_cast<int>(Block::ObservationLimit)})
            {
                const auto before = snapshot();
                action(
                    [&]
                    {
                        refuses([&] { sdk.RemoveAnimBlockRef(index); }, "public reference removal refuses invalid/unobserved integer index");
                        refuses([&] { sdk.RemoveAnimBlockRefWithoutDelete(index); }, "nondeleting reference removal refuses invalid/unobserved integer index");
                        refuses([&] { sdk.AddAnimBlockRef(index); }, "public reference addition refuses invalid/unobserved integer index");
                        refuses([&] { sdk.GetNumRefsToAnimBlock(index); }, "public reference getter refuses invalid/unobserved integer index");
                    });
                Check(snapshot() == before && !ClockAnimationBlockRefOwnerTest::exchanges && calls == registrationCalls &&
                          Bytes::Load<std::int32_t>(image + NativeAnimationManagerProfile::AnimationCountRva) == originalAnimations && !runtime.stopping.load(),
                      "invalid SDK counter indices leave the full native bank/count unchanged and enter no writer");
            }
            AnimationManagerRegistrationReceiver staleManager(runtime.access, runtime.access, generation + 1);
            CAnimManager&                        stale = staleManager;
            action(
                [&]
                {
                    refuses([&] { stale.RemoveAnimBlockRef(0); }, "actual runtime rejects a stale-generation public reference removal");
                    refuses([&] { stale.RemoveAnimBlockRefWithoutDelete(0); }, "actual runtime rejects a stale-generation nondeleting reference removal");
                });
            refuses([&] { sdk.RemoveAnimBlockRef(0); }, "outside the actual callback no public reference write authority is lent");
            refuses([&] { sdk.RemoveAnimBlockRefWithoutDelete(0); }, "outside the actual callback no nondeleting reference write authority is lent");
            Check(snapshot() == originalBank && !ClockAnimationBlockRefOwnerTest::exchanges && calls == registrationCalls &&
                      Bytes::Load<std::int32_t>(image + NativeAnimationManagerProfile::AnimationCountRva) == originalAnimations && !runtime.stopping.load(),
                  "stale generation and callback-phase refusals preserve complete native storage without a writer");
            ClockAnimationBlockRefOwnerTest::address = 0;
            // Existing-name return consumes no definition lookup or writes.
            auto poison = definitions;
            std::fill(poison.begin(), poison.end(), static_cast<unsigned char>('x'));
            WorldGroundOwnerFixture::Writes::Apply(&saved, Definition::BankRva, poison.data(), poison.size(), PAGE_READWRITE);
            registration("StOcK", 0, 0, false);
            WorldGroundOwnerFixture::Writes::Apply(&saved, Definition::BankRva, definitions.data(), definitions.size(), PAGE_READWRITE);
            registration("new_a", 1, 3, true);
            registration("NEW_A", 1, 3, false);
            registration("new_b", 2, 118, true);
            registration("123456789abcdef", 3, 118, true);
            // Exercise the last row in the explicitly observed interval, then
            // full-prefix existing lookup; no capacity is inferred from it.
            for (unsigned row = 4; row != 179; ++row)
            {
                char spelling[16]{};
                std::snprintf(spelling, sizeof(spelling), "old%03u", row);
                saved.Global(Block::BankRva + row * 32, Name(spelling));
            }
            saved.Global(Block::CountRva, std::int32_t{179});
            registration("last", 179, 118, true);
            registration("LAST", 179, 118, false);
            const auto before = snapshot();
            action([&] { refuses([&] { sdk.RegisterAnimBlock("beyond"); }, "fresh append outside supported observation interval refuses"); });
            Check(snapshot() == before && !runtime.stopping.load(), "bounded full interval refusal has no new stores or native debt");
            Check(finish() && !service.IsRetained(), "verified register/find/append retires without owning native bank storage");
            std::puts("Production block registration actual SDK source policy fixture completed");
            std::puts("Production block registration existing fresh named default boundary effects fixture completed");
        }
        else if (scenario <= 2)
        {
            const auto check = [&](const auto& operation)
            {
                const auto before = snapshot();
                const auto beforeCalls = calls;
                action([&] { refuses(operation, "unadmitted registration refuses before any native bank store"); });
                Check(snapshot() == before && calls == beforeCalls && !runtime.blockRegistration.IsRetained() && !runtime.stopping.load(),
                      "registration refusal preserves complete current bank/count and leaves owner usable");
            };
            const auto command = [&] { sdk.RegisterAnimBlock("new_a"); };
            if (scenario == 2)
                check(command);
            else
            {
                check([&] { sdk.RegisterAnimBlock(nullptr); });
                check([&] { sdk.RegisterAnimBlock(""); });
                check([&] { sdk.RegisterAnimBlock("123456789abcdefg"); });
                const char high[] = {static_cast<char>(0xe9), 0};
                check([&] { sdk.RegisterAnimBlock(high); });
                saved.Global(Block::BankRva + 32 + 0x12, std::uint16_t{1});
                check(command);
                saved.Global(Block::BankRva + 32 + 0x12, std::uint16_t{0});
                saved.Global(Block::CountRva, std::int32_t{-1});
                check(command);
                saved.Global(Block::CountRva, std::int32_t{1});
                saved.Global(Block::CountRva, std::int32_t{181});
                check(command);
                saved.Global(Block::CountRva, std::int32_t{1});
                const auto& pin = Definition::BlockNameCode[0];
                saved.Global(pin.rva + pin.size - 1, static_cast<unsigned char>(pin.bytes[pin.size - 1] ^ 1), PAGE_EXECUTE_READ);
                check(command);
                saved.Global(pin.rva + pin.size - 1, pin.bytes[pin.size - 1], PAGE_EXECUTE_READ);
                check(
                    [&]
                    {
                        const auto serial = frameCallbackSerial;
                        --frameCallbackSerial;
                        try
                        {
                            command();
                        }
                        catch (...)
                        {
                            frameCallbackSerial = serial;
                            throw;
                        }
                        frameCallbackSerial = serial;
                    });
                refuses(command, "producer outside callback has no registration write authority");
            }
            Check(finish() && !service.IsRetained() && !calls, "registration pre-entry refusals retire cleanly");
            std::puts(scenario == 2 ? "Production block registration timer phase refusal fixture completed"
                                    : "Production block registration coupled source owner refusals fixture completed");
        }
        else
        {
            mode = scenario == 3   ? 1
                   : scenario == 4 ? 2
                   : scenario == 5 ? 3
                   : scenario == 6 ? 4
                   : scenario == 7 ? 5
                   : scenario == 8 ? 6
                   : scenario == 9 ? 7
                                   : 8;
            action([&] { refuses([&] { sdk.RegisterAnimBlock("new_a"); }, "uncertain entered registration never publishes an SDK index"); });
            const auto receipt = runtime.blockRegistrationReceipt;
            Check(calls == 1 && executed == (mode == 1 ? 0u : 1u) && receipt.entered && !receipt.effectsVerified && receipt.debt && receipt.index == -1 &&
                      runtime.blockRegistration.RecoveryPending() && NativeLifetimeRetained() && runtime.stopping.load(),
                  "entered uncertainty retains the actual ordered row/count journal and module fence");
            if (mode == 4)
                Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{0});
            const auto observed = snapshot();
            refuses([&] { sdk.RegisterAnimBlock("new_a"); }, "stale entered registration cannot replay its store list");
            Check(!finish() && service.IsRetained() && calls == 1 && snapshot() == observed,
                  "End and natural frames retain observed registration effects without replay, deletion or guessed repair");
            const char* labels[] = {"before", "after", "partial", "owner", "mismatch", "count", "prefix", "group"};
            std::printf("Production block registration %s retained debt fixture completed\n", labels[scenario - 3]);
        }
        image = hierarchy = 0;
        originalFrame = nullptr;
        ClockReaderTestHost::image = priorImage;
        Host::active = nullptr;
        saved.Restore();
    }
}
namespace ClockBlockRegistrationOwnerTest
{
    mta::game_de::ClockResult Write(const mta::game_de::NativeAnimationBlockRegistrationCalls::Store* stores, std::size_t count,
                                    std::uint32_t& exception) noexcept
    {
        return BlockRegistrationOwnerFixture::Write(stores, count, exception);
    }
}
