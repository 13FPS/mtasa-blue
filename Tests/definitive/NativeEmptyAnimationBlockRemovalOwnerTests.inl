/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK empty block removal through its genuine post-frame owner
 ****************************************************************************/
namespace EmptyBlockRemovalOwnerFixture
{
    namespace Calls = NativeAnimationBlockRemovalCalls;
    namespace Bytes = ClockPrivateAnimationAssociationTest;
    namespace Conversion = NativeAnimationHierarchyDecompressProfile;
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Profile = NativeAnimationAssociationProgressProfile;
    namespace Block = NativeAnimationBlockProfile;
    namespace Definition = NativeAnimationAssocDefinitionProfile;
    unsigned                     scenario = 0, calls = 0, executed = 0, mode = 0, groupCalls = 0, groupExecuted = 0;
    std::uintptr_t               image = 0, hierarchy = 0, groupBank = 0;
    NativeAnimationBlockIdentity identity{};
    bool                         valid = true, nestedBusy = false, gateBusy = false, retained = false;
    void                         Observe() noexcept
    {
        valid = valid && CheckApplicationTimingOwner() == ClockResult::Ok;
        nestedBusy = runtime.access.RemoveEmptyAnimationBlock(identity) == ClockResult::Busy;
        const bool entered = NativeStreamingCompletionContext::TryEnter();
        gateBusy = !entered;
        if (entered)
            NativeStreamingCompletionContext::Leave();
        retained = runtime.blockRemoval.IsRetained() && NativeLifetimeRetained();
        valid = valid && nestedBusy && gateBusy && retained;
    }
    ClockResult Group(Calls::GroupFunction entry, std::uintptr_t group, std::uint32_t& exception) noexcept
    {
        ++groupCalls;
        Observe();
        valid = valid && reinterpret_cast<std::uintptr_t>(entry) == image + 0xfeb580 && (group == groupBank || group == groupBank + 64) &&
                !Bytes::Load<std::uintptr_t>(group + 8) && !Bytes::Load<std::uint32_t>(group + 16);
        if (!valid)
            return ClockResult::BindingChanged;
        if (mode == 5)
            return ClockResult::WriteFailure;
        // Execute the complete existing qualified FEB580, including its own
        // null-child branch. No allocator, deleted-child or leaf substitute.
        const auto result = AnimationBlockRemovalDetail::InvokeOriginal(entry, group, exception);
        if (result != ClockResult::Ok)
            return result;
        ++groupExecuted;
        if (mode == 6)
            return ClockResult::WriteFailure;
        if (mode == 7)
            Bytes::Store(groupBank + 32 + 28, std::uint32_t{0x87654321});
        return ClockResult::Ok;
    }
    ClockResult Write(const Calls::Store* stores, std::size_t count, std::uint32_t& exception) noexcept
    {
        ++calls;
        Observe();
        valid = valid && count == 2 && stores[0].address == image + Block::BankRva + Block::LoadedOffset && stores[0].bytes == 1 &&
                stores[1].address == image + Block::BankRva + NativeAnimationBlockRefProfile::RefOffset && stores[1].bytes == 2;
        if (!valid)
            return ClockResult::BindingChanged;
        if (mode == 1)
            return ClockResult::WriteFailure;
        const auto result = AnimationBlockRemovalDetail::WriteStores(stores, mode == 3 ? 1 : count, exception);
        if (result != ClockResult::Ok)
            return result;
        ++executed;
        if (mode == 2 || mode == 3)
            return ClockResult::WriteFailure;
        if (mode == 4)
            Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{1});
        if (mode == 8)
            Bytes::Store(image + Block::BankRva + 17, static_cast<unsigned char>(0x19));
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
        AnimationManagerEmptyBlockRemovalReceiver manager(runtime.access, runtime.access, generation);
        CAnimManager&                             sdk = manager;
        const auto                                refuses = [&](const auto& operation, const char* why)
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

        // The same owned producer bank supplies genuine cookie-backed group
        // storage. Only current rows0/2 match; row1 owns unrelated children.
        groupBank = source.groups;
        groupCalls = groupExecuted = 0;
        std::array<unsigned char, Block::ObservationLimit * Block::RowBytes> bank{};
        for (std::size_t n = 0; n != 2 * Block::RowBytes; ++n)
            bank[n] = static_cast<unsigned char>(0x40 + n);
        // All16 bytes are deliberately nonterminated, including high bytes.
        for (unsigned n = 0; n != 16; ++n)
            bank[n] = static_cast<unsigned char>(0x90 + n);
        const std::uint32_t zero = 0, seven = 7;
        std::memcpy(bank.data() + Block::AnimationCountOffset, &zero, 4);
        std::memcpy(bank.data() + 32 + Block::AnimationCountOffset, &seven, 4);
        WorldGroundOwnerFixture::Writes::Apply(&saved, Block::BankRva, bank.data(), bank.size(), PAGE_READWRITE);
        saved.Global(Block::CountRva, std::int32_t{2});
        saved.Global(NativeAnimationManagerProfile::AnimationCountRva, std::int32_t{37});
        saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{3});
        for (unsigned n = 0; n != 3; ++n)
        {
            Bytes::Store(groupBank + n * 32, image + Block::BankRva + (n == 1 ? 32 : 0));
            Bytes::Store(groupBank + n * 32 + 8, n == 1 ? source.children : std::uintptr_t{0});
            Bytes::Store(groupBank + n * 32 + 16, std::uint32_t{n == 1 ? 2u : 0u});
            Bytes::Store(groupBank + n * 32 + 20, std::uint32_t{0x12340000u + n});
            Bytes::Store(groupBank + n * 32 + 24, std::uint64_t{0xaabbccdd00000000ull + n});
        }
        WorldGroundOwnerFixture::Writes::Apply(&saved, AnimationBlockRemovalDetail::ArmRva,
                                               NativeStreamingRemoveProfile::Remove.data() + AnimationBlockRemovalDetail::ArmOffset,
                                               AnimationBlockRemovalDetail::ArmBytes, PAGE_EXECUTE_READ);
        identity = {};
        identity.ownerGeneration = generation;
        std::memcpy(identity.name.data(), bank.data(), 16);
        const auto snapshot = [&]
        {
            std::vector<unsigned char> out;
            const auto                 add = [&](std::uintptr_t address, std::size_t bytes)
            {
                const auto old = out.size();
                out.resize(old + bytes);
                std::memcpy(out.data() + old, reinterpret_cast<const void*>(address), bytes);
            };
            add(image + Block::BankRva, bank.size());
            add(groupBank - 8, 8 + 3 * 32);
            add(source.children - 8, 8 + 2 * 32);
            add(source.hierarchy, 40);
            add(image + Block::CountRva, 8);
            add(image + NativeAnimationGroupArrayProfile::CountRva, 4);
            add(image + NativeAnimationGroupArrayProfile::GlobalRva, 8);
            return out;
        };
        const auto allocatorBefore = std::make_pair(host.mallocCalls, host.freeCalls);
        if (scenario == 0)
        {
            for (const auto loaded : {std::uint8_t{0}, std::uint8_t{1}})
                for (const auto refs : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{0xffff}})
                {
                    Bytes::Store(image + Block::BankRva + Block::LoadedOffset, loaded);
                    Bytes::Store(image + Block::BankRva + 0x12, refs);
                    const auto before = snapshot();
                    const auto beforeGroups = groupCalls, beforeWrites = calls;
                    auto       expected = before;
                    expected[Block::LoadedOffset] = expected[0x12] = expected[0x13] = 0;
                    action([&] { sdk.RemoveAnimBlock(0); });
                    const auto receipt = runtime.blockRemovalReceipt;
                    Check(snapshot() == expected && receipt.result == ClockResult::Ok && receipt.effectsVerified && receipt.entered && receipt.writeEntered &&
                              receipt.beforeLoaded == loaded && receipt.beforeRefs == refs && receipt.matchingGroups == 2 && receipt.groupCalls == 2 &&
                              receipt.groupReturns == 2 && groupCalls == beforeGroups + 2 && calls == beforeWrites + 1 && valid && nestedBusy && gateBusy &&
                              retained && !runtime.blockRemoval.IsRetained() && !runtime.stopping.load(),
                          "actual raw16 SDK removal preserves every untouched bank/group/hierarchy/global byte for loaded0/1 refs0/1/FFFF");
                    Check(std::make_pair(host.mallocCalls, host.freeCalls) == allocatorBefore,
                          "whole FEB580 null-child originals perform no native allocator or leaf teardown calls");
                }
            saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{0});
            saved.Global(NativeAnimationGroupArrayProfile::GlobalRva, std::uintptr_t{0});
            const auto beforeGroups = groupCalls;
            action([&] { sdk.RemoveAnimBlock(0); });
            Check(groupCalls == beforeGroups && runtime.blockRemovalReceipt.effectsVerified && !runtime.blockRemovalReceipt.matchingGroups,
                  "genuinely absent current group bank follows the source empty group loop without an invented call");
            Check(finish() && !service.IsRetained(), "verified empty-block removal retires its genuine frame owner without bank ownership");
            std::puts("Production empty block removal actual SDK owner fixture completed");
            std::puts("Production empty block removal exact empty effects whole FEB580 fixture completed");
        }
        else if (scenario <= 2)
        {
            const auto check = [&](const auto& operation)
            {
                const auto before = snapshot();
                const auto beforeGroups = groupCalls, beforeWrites = calls;
                action([&] { refuses(operation, "outside the proven empty domain the actual SDK refuses"); });
                Check(snapshot() == before && groupCalls == beforeGroups && calls == beforeWrites && !runtime.blockRemoval.IsRetained() &&
                          !runtime.stopping.load(),
                      "pre-entry refusal preserves the complete current bank/group/hierarchy/global bytes and owner usability");
            };
            const auto command = [&] { sdk.RemoveAnimBlock(0); };
            if (scenario == 2)
                check(command);
            else
            {
                check([&] { sdk.RemoveAnimBlock(-1); });
                check([&] { sdk.RemoveAnimBlock(2); });
                AnimationManagerEmptyBlockRemovalReceiver stale(runtime.access, runtime.access, generation + 1);
                check([&] { stale.RemoveAnimBlock(0); });
                for (const auto count : {std::uint32_t{1}, std::uint32_t{0x80000000u}})
                {
                    Bytes::Store(image + Block::BankRva + 24, count);
                    check(command);
                }
                Bytes::Store(image + Block::BankRva + 24, std::uint32_t{0});
                Bytes::Store(groupBank + 8, source.children);
                check(command);
                Bytes::Store(groupBank + 8, std::uintptr_t{0});
                Bytes::Store(groupBank + 16, std::uint32_t{1});
                check(command);
                Bytes::Store(groupBank + 16, std::uint32_t{0});
                saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{4098});
                check(command);
                saved.Global(NativeAnimationGroupArrayProfile::CountRva, std::int32_t{3});
                const auto& pin = NativeAnimationStaticAssociationProfile::Retire;
                saved.Global(0xfeb580 + pin.size() - 1, static_cast<unsigned char>(pin.back() ^ 1), PAGE_EXECUTE_READ);
                check(command);
                saved.Global(0xfeb580 + pin.size() - 1, pin.back(), PAGE_EXECUTE_READ);
                saved.Global(NativeApplicationFrameProfile::stateRva, std::uint32_t{8});
                check(command);
                saved.Global(NativeApplicationFrameProfile::stateRva, std::uint32_t{9});
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
                action(
                    [&]
                    {
                        auto wrong = identity;
                        wrong.name[15] ^= 1;
                        Check(runtime.access.RemoveEmptyAnimationBlock(wrong) != ClockResult::Ok, "raw16 name mismatch refuses");
                        wrong = identity;
                        ++wrong.ownerGeneration;
                        Check(runtime.access.RemoveEmptyAnimationBlock(wrong) == ClockResult::OwnerConflict, "foreign generation refuses");
                    });
                refuses(command, "producer outside callback cannot remove an empty block");
            }
            Check(finish() && !service.IsRetained() && !calls && !groupCalls, "empty removal pre-entry refusals retire cleanly");
            std::puts(scenario == 2 ? "Production empty block removal timer phase refusal fixture completed"
                                    : "Production empty block removal source owner refusals fixture completed");
        }
        else
        {
            mode = scenario - 2;
            Bytes::Store(image + Block::BankRva + Block::LoadedOffset, std::uint8_t{1});
            Bytes::Store(image + Block::BankRva + 0x12, std::uint16_t{0xffff});
            action([&] { refuses([&] { sdk.RemoveAnimBlock(0); }, "uncertain entered empty removal does not acknowledge SDK success"); });
            const auto receipt = runtime.blockRemovalReceipt;
            const bool groupFailure = mode >= 5 && mode <= 7;
            Check(receipt.entered && !receipt.effectsVerified && receipt.debt && runtime.blockRemoval.RecoveryPending() && NativeLifetimeRetained() &&
                      runtime.stopping.load() && calls == (groupFailure ? 0u : 1u) && groupCalls == (groupFailure ? 1u : 2u) &&
                      groupExecuted == (mode == 5      ? 0u
                                        : groupFailure ? 1u
                                                       : 2u) &&
                      valid,
                  "every entered unknown group/store/effect retains the exact CPU journal and native module fence");
            if (mode == 4)
                Bytes::Store(image + NativeBorrowedGcProfile::markerRva, LONG{0});
            const auto observed = snapshot();
            const auto heldWrites = calls, heldGroups = groupCalls;
            refuses([&] { sdk.RemoveAnimBlock(0); }, "stale uncertain SDK removal cannot replay");
            Check(!finish() && service.IsRetained() && snapshot() == observed && calls == heldWrites && groupCalls == heldGroups,
                  "End and natural frames preserve uncertain effects without retry repair deregistration or global count edits");
            Check(std::make_pair(host.mallocCalls, host.freeCalls) == allocatorBefore, "even uncertain empty retirement never reaches the native allocator");
            const char* labels[] = {"before", "after", "partial", "owner", "group-before", "group-after", "group-drift", "mismatch"};
            std::printf("Production empty block removal %s retained debt fixture completed\n", labels[scenario - 3]);
        }
        identity = {};
        image = hierarchy = groupBank = 0;
        originalFrame = nullptr;
        ClockReaderTestHost::image = priorImage;
        Host::active = nullptr;
        saved.Restore();
    }
}
namespace ClockEmptyBlockRemovalOwnerTest
{
    mta::game_de::ClockResult Group(mta::game_de::NativeAnimationBlockRemovalCalls::GroupFunction entry, std::uintptr_t group,
                                    std::uint32_t& exception) noexcept
    {
        return EmptyBlockRemovalOwnerFixture::Group(entry, group, exception);
    }
    mta::game_de::ClockResult Write(const mta::game_de::NativeAnimationBlockRemovalCalls::Store* stores, std::size_t count, std::uint32_t& exception) noexcept
    {
        return EmptyBlockRemovalOwnerFixture::Write(stores, count, exception);
    }
}
